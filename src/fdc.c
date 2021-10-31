/*
  RPCEmu - An Acorn system emulator

  Copyright (C) 2005-2010 Sarah Walker

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* NEC 765/Intel 82077 Floppy drive emulation, on RPC/A7000 a part of the
   SMC 37C665GT PC style Super IO chip */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "rpcemu.h"
#include "fdc.h"
#include "vidc20.h"
#include "iomd.h"
#include "ide.h"
#include "arm.h"
#include "disc.h"
#include "disc_adf.h"
#include "disc_hfe.h"

/* FDC commands */
enum {
	FD_CMD_SPECIFY			= 0x03,
	FD_CMD_SENSE_DRIVE_STATUS	= 0x04,
	FD_CMD_RECALIBRATE		= 0x07,
	FD_CMD_SENSE_INTERRUPT_STATUS	= 0x08,
	FD_CMD_READ_ID_FM		= 0x0a,
	FD_CMD_DUMPREG			= 0x0e,
	FD_CMD_SEEK			= 0x0f,
	FD_CMD_CONFIGURE		= 0x13,
	FD_CMD_WRITE_DATA_MFM		= 0x45,
	FD_CMD_READ_DATA_MFM		= 0x46,
	FD_CMD_READ_ID_MFM		= 0x4a,
	FD_CMD_FORMAT_TRACK_MFM		= 0x4d,
	FD_CMD_VERIFY_DATA_MFM		= 0x56,
};

static void fdcsend(uint8_t val);

static struct
{
	uint8_t dor;
	int reset;
	uint8_t status;
	int incommand;
	uint8_t command;
	uint8_t st0;
	uint8_t st1;
	uint8_t st2;
	uint8_t st3;
	int track;
	int sector;
	int side;
	uint8_t data;
	int params;
	int curparam;
	uint8_t parameters[10];
	uint8_t dmadat;
	int rate;
	int oldpos;
	uint8_t results[16];
	int result_rp, result_wp;
	int drive;
	int density;
	int tc;
	int data_ready;
	int written;
	int in_read;
} fdc;

/* This enumeration must be kept in sync with the formats[] array */
typedef enum {
	DISC_FORMAT_ADFS_DE_800K,
	DISC_FORMAT_ADFS_F_1600K,
	DISC_FORMAT_ADFS_L_640K,
	DISC_FORMAT_DOS_360K,
	DISC_FORMAT_DOS_720K,
	DISC_FORMAT_DOS_1440K,
} DiscFormat;

typedef struct {
	const char *name;
	const char *extension;
	int sides;
	int tracks;
	int sectors;
	int sectorsize;
	int sectorskew;		///< What is the first sector in a track? e.g 0-79 or 1-80
	int density;
} Format;

/* This array must be kept in sync with the disc_format enumeration */
static const Format formats[] = {
	{ "ADFS D/E 800KB",   "adf", 2, 80,  5, 1024, 0, 2 },
	{ "ADFS F 1600KB",    "adf", 2, 80, 10, 1024, 0, 0 },
	{ "ADFS L 640KB",     "adl", 2, 80, 16,  256, 0, 2 },
	{ "DOS 360KB",        "img", 2, 40,  9,  512, 1, 2 },
	{ "DOS 720KB",        "img", 2, 80,  9,  512, 1, 2 },
	{ "DOS 1440KB",       "img", 2, 80, 18,  512, 1, 0 }
};

int fdccallback = 0;
int motoron = 0;

static inline void
fdc_irq_raise(void)
{
	iomd.irqb.status |= IOMD_IRQB_FLOPPY;
	updateirqs();
}

static inline void
fdc_irq_lower(void)
{
	iomd.irqb.status &= ~IOMD_IRQB_FLOPPY;
	updateirqs();
}

static inline void
fdc_dma_raise(void)
{
	iomd.fiq.status |= IOMD_FIQ_FLOPPY_DMA_REQUEST;
	updateirqs();
}

static inline void
fdc_dma_lower(void)
{
	iomd.fiq.status &= ~IOMD_FIQ_FLOPPY_DMA_REQUEST;
	updateirqs();
}

void
fdc_reset(void)
{
	fdccallback = 0;
	motoron = 0;
	fdc.result_rp = 0;
	fdc.result_wp = 0;
}

void
fdc_init(void)
{
}

/**
 * Load a disc image into one of the two virtual floppy
 * disc drives.
 *
 * @param fn    Filename of disc image to load (including .adf .adl extension)
 * @param drive Which drive to load image into 0 or 1
 */
void
fdc_image_load(const char *fn, int drive)
{
	FILE *f;
	const char *extension;
	long len;
	const Format *format;
	int is_hfe = 0;

	assert(drive == 0 || drive == 1); // Only support two drives
	assert(fn != NULL); // Must have filename

	// must be at least a.ext
	if (strlen(fn) < 5 || fn[strlen(fn) - 4] != '.') {
		error("Disc image filename must include a file extension (.adf,.adl)");
		return;
	}

	f = fopen(fn, "rb");
	if (f == NULL) {
//		error("Unable to open disc image '%s'", fn);
		return;
	}

	fseek(f, 0, SEEK_END);
	len = ftell(f);
	if (len < 0) {
		error("Failed to read size of disc image file");
		fclose(f);
		return;
	}

	extension = fn + strlen(fn) - 4;

	if (strcasecmp(extension, ".adf") == 0) {
		if (len > 1000000) {
			format = &formats[DISC_FORMAT_ADFS_F_1600K];
		} else {
			format = &formats[DISC_FORMAT_ADFS_DE_800K];
		}
	} else if (strcasecmp(extension, ".adl") == 0) {
		format = &formats[DISC_FORMAT_ADFS_L_640K];
	} else if (strcasecmp(extension, ".img") == 0) {
		if (len > 1250000) {
			format = &formats[DISC_FORMAT_DOS_1440K];
		} else if (len > 400000) {
			format = &formats[DISC_FORMAT_DOS_720K];
		} else {
			format = &formats[DISC_FORMAT_DOS_360K];
		}
	}  else if (strcasecmp(extension, ".hfe") == 0) {
		is_hfe = 1;
	} else {
		error("Unknown disc image file extension '%s', must be .adf, .adl, .img or .hfe", extension);
		fclose(f);
		return;
	}
	fclose(f);

	if (drive_funcs[drive]) {
		drive_funcs[drive]->close(drive);
		drive_funcs[drive] = NULL;
	}

	rpclog("fdc_image_load: %s (%ld) loaded as '%s'\n", fn, len, is_hfe ? "HFE" : format->name);
	if (is_hfe)
		hfe_load(drive, fn);
	else
		adf_load(drive, fn, format->sectors, format->sectorsize, format->sides, format->tracks == 40,
			format->density ? 1 : 2, format->sectorskew);
}

void
fdc_image_save(const char *fn, int drive)
{
	(void)fn;
	if (drive_funcs[drive]) {
		drive_funcs[drive]->close(drive);
		drive_funcs[drive] = NULL;
	}
}

void
fdc_write(uint32_t addr, uint32_t val)
{
	switch (addr) {
	case 0x3f2: /* Digital Output Register (DOR) */
		if ((val & 4) && !(fdc.dor & 4)) { /*Reset*/
			fdc.reset   = 1;
			fdccallback = 500;
		}
		if (!(val & 4)) {
			fdc.status = 0x80;
		}
		motoron = val & 0x30;
		if (val & 0x10)
			disc_set_drivesel(0);
		else if (val & 0x20)
			disc_set_drivesel(1);
		break;

	case 0x3f4: /* Data Rate Select Register (DSR) */
		break;

	case 0x3f5: /* Data (FIFO) - Command */
		if (fdc.params) {
			fdc.parameters[fdc.curparam++] = val;

			if (fdc.curparam == fdc.params) {
				fdc.status &= ~0x80;
				fdc.tc = 0;
				fdc.data_ready = 0;
				fdc.in_read = 0;
				switch (fdc.command) {
				case FD_CMD_SPECIFY:
					fdccallback = 100;
					break;

				case FD_CMD_SENSE_DRIVE_STATUS:
					fdccallback = 100;
					break;

				case FD_CMD_RECALIBRATE:
					fdccallback = 500;
					fdc.status |= 1;
					disc_seek(fdc.parameters[0] & 1, 0);
					break;

				case FD_CMD_SEEK:
					fdccallback = 500;
					fdc.status |= 1;
					disc_seek(fdc.parameters[0] & 1, fdc.parameters[1]);
					break;

				case FD_CMD_CONFIGURE:
					fdccallback = 100;
					break;

				case FD_CMD_WRITE_DATA_MFM:
					fdc.st0        = fdc.parameters[0] & 7;
					fdc.st1        = 0;
					fdc.st2        = 0;
					fdc.track      = fdc.parameters[1];
					fdc.side       = fdc.parameters[2];
					fdc.sector     = fdc.parameters[3];
					fdc.drive      = fdc.st0 & 1;
					disc_writesector(fdc.st0 & 1, fdc.sector, fdc.track, fdc.side, fdc.density);
					fdc_dma_raise();
					break;

				case FD_CMD_READ_DATA_MFM:
					fdc.in_read = 1;
				case FD_CMD_VERIFY_DATA_MFM:
					fdc.st0        = fdc.parameters[0] & 7;
					fdc.st1        = 0;
					fdc.st2        = 0;
					fdc.track      = fdc.parameters[1];
					fdc.side       = fdc.parameters[2];
					fdc.sector     = fdc.parameters[3];
					fdc.drive      = fdc.st0 & 1;
					disc_readsector(fdc.st0 & 1, fdc.sector, fdc.track, fdc.side, fdc.density);
					break;

				case FD_CMD_READ_ID_MFM:
					fdc.st0        = fdc.parameters[0] & 7;
					fdc.st1        = 0;
					fdc.st2        = 0;
					fdc.drive      = fdc.st0 & 1;
					disc_readaddress(fdc.st0 & 1, fdc.track, fdc.side, fdc.density);
					break;

				case FD_CMD_READ_ID_FM:
					fdccallback    = 4000;
					fdc.st0        = fdc.parameters[0] & 7;
					fdc.st1        = 0;
					fdc.st2        = 0;
					fdc.drive      = fdc.st0 & 1;
					break;

				case FD_CMD_FORMAT_TRACK_MFM:
					fdc.st0        = fdc.parameters[0] & 7;
					fdc.st1        = 0;
					fdc.st2        = 0;
					fdc.drive      = fdc.st0 & 1;
					break;

				default:
					UNIMPLEMENTED("FDC command",
						"Unknown command 0x%02x",
						fdc.command);
				}
			}
			return;
		}

		if (fdc.incommand) {
			fatal("FDC already in command\n");
		}
		fdc.incommand  = 1;
		fdc.command    = val;

		switch (fdc.command) {
		case FD_CMD_SPECIFY:
			fdc.params   = 2;
			fdc.curparam = 0;
			fdc.status   = 0x90;
			break;

		case FD_CMD_SENSE_DRIVE_STATUS:
			fdc.params   = 1;
			fdc.curparam = 0;
			fdc.status   = 0x90;
			break;

		case FD_CMD_RECALIBRATE:
			fdc.params   = 1;
			fdc.curparam = 0;
			fdc.status   = 0x90;
			break;

		case FD_CMD_SENSE_INTERRUPT_STATUS:
			fdccallback = 100;
			fdc.status  = 0x10;
			break;

		case FD_CMD_DUMPREG: /* Used by Linux to identify FDC type. */
			fdc.st0       = 0x80;
			fdcsend(fdc.st0);
			fdc_irq_raise();
			fdc.incommand = 0;
			fdccallback   = 0;
			fdc.status    = 0x80;
			break;

		case FD_CMD_SEEK:
			fdc.params   = 2;
			fdc.curparam = 0;
			fdc.status   = 0x90;
			break;

		case FD_CMD_CONFIGURE:
			fdc.params   = 3;
			fdc.curparam = 0;
			fdc.status   = 0x90;
			break;

		case FD_CMD_WRITE_DATA_MFM:
		case FD_CMD_READ_DATA_MFM:
		case FD_CMD_VERIFY_DATA_MFM:
			fdc.params   = 8;
			fdc.curparam = 0;
			fdc.status   = 0x90;
			break;

		case FD_CMD_READ_ID_FM:
		case FD_CMD_READ_ID_MFM:
			fdc.params   = 1;
			fdc.curparam = 0;
			fdc.status   = 0x90;
			break;

		case FD_CMD_FORMAT_TRACK_MFM:
			fdc.params   = 5;
			fdc.curparam = 0;
			fdc.status   = 0x80;
			break;

		default:
			UNIMPLEMENTED("FDC command 2",
				"Unknown command 0x%02x", fdc.command);
			fatal("Bad FDC command %02X\n", val);
		}
		break;

	case 0x3f7: /* Configuration Control Register (CCR) */
		fdc.rate = val & 3;
		switch (val & 3) {
		case 0:
			fdc.density = 2;
			break;
		case 1: case 2:
			fdc.density = 1;
			break;
		case 3:
			fdc.density = 3;
			break;
		}
		break;

	default:
		UNIMPLEMENTED("FDC write", "Unknown register 0x%03x", addr);
	}
}


uint8_t
fdc_read(uint32_t addr)
{
	switch (addr) {
	case 0x3f4: /* Main Status Register (MSR) */
		fdc_irq_lower();
		return fdc.status;

	case 0x3f5: { /* Data (FIFO) */
		uint8_t data = fdc.results[fdc.result_rp & 0xf];

		if (fdc.result_rp != fdc.result_wp) {
			fdc.result_rp++;
		}

		if (fdc.result_rp == fdc.result_wp) {
			fdc.status = 0x80;
		} else {
			fdc.status |= 0xc0;
		}

		return data;
	}

	// case 0x3f7: /* Digital Input Register (DIR) */
		// TODO Disc changed flag
		// return 0x80;

	default:
		UNIMPLEMENTED("FDC read", "Unknown register 0x%03x", addr);
	}

	return 0;
}

static void
fdcsend(uint8_t val)
{
	fdc.results[fdc.result_wp & 0xf] = val;
	fdc.result_wp++;
}

static void
fdc_end_command(void)
{
	fdc.status = 0xD0;
	fdc.incommand = 0;
	fdc.params = 0;
	fdccallback = 0;
	fdc_irq_raise();
}

void
fdc_callback(void)
{
	if (fdc.reset) {
		fdc.reset     = 0;
		fdc.status    = 0x80;
		fdc.incommand = 0;
		fdc.st0       = 0xc0;
		fdc.track     = 0;
		fdc.curparam  = 0;
		fdc.params    = 0;
		fdc.rate      = 2;
		fdc_irq_raise();
		return;
	}

	switch (fdc.command) {
	case FD_CMD_SPECIFY:
		fdc.incommand = 0;
		fdc.status    = 0x80;
		fdc.params    = 0;
		fdc.curparam  = 0;
		break;

	case FD_CMD_SENSE_DRIVE_STATUS:
		fdc.st3 = (fdc.parameters[0] & 7) | 0x28; // Drive number, 'two side' and 'ready'
		if (fdc.track == 0) {
			fdc.st3 |= 0x10; // Track is zero
		}
		// TODO Mix in Head number?
		// TODO Mix in read only?

		fdc.incommand = 0;
		fdcsend(fdc.st3);
		fdc.status = 0xD0;
		fdc_irq_raise();
		fdc.params    = 0;
		fdc.curparam  = 0;
		break;

	case FD_CMD_RECALIBRATE:
		fdc.track     = 0;
		fdc.incommand = 0;
		fdc.status    = 0x80;
		fdc.params    = 0;
		fdc.curparam  = 0;
		// TODO set drive back to track 0
		fdc.st0       = 0x20; // Seek End
		fdc_irq_raise();
		break;

	case FD_CMD_SENSE_INTERRUPT_STATUS:
		fdcsend(fdc.st0);
		fdcsend(fdc.track);
		fdc.status = 0xD0;
		fdc_irq_raise();
		fdc.incommand = 0;
		break;

//	case FD_CMD_DUMPREG: /*Dump registers - act as invalid command*/
//		fdc.st0 = 0x80;
//		fdcsend(fdc.st0);
//		fdc.incommand = 0;
//		break;

	case FD_CMD_SEEK:
		fdc.track     = fdc.parameters[1];
		fdc.incommand = 0;
		fdc.status    = 0x80;
		fdc.params    = 0;
		fdc.curparam  = 0;
		fdc.st0       = 0x20;
		fdc_irq_raise();
		break;

	case FD_CMD_CONFIGURE:
		fdc.incommand = 0;
		fdc.status    = 0x80;
		fdc.params    = 0;
		fdc.curparam  = 0;
		break;

	case FD_CMD_WRITE_DATA_MFM:
		fdc.sector++;
		if (fdc.sector > fdc.parameters[5])
			fdc.tc = 1;
		if (fdc.tc) {
			fdcsend(fdc.st0);
			fdcsend(fdc.st1);
			fdcsend(fdc.st2);
			fdcsend(fdc.track);
			fdcsend((fdc.parameters[0] & 4) ? 1 : 0);
			fdcsend(fdc.sector);
			fdcsend(fdc.parameters[4]);
			fdc.status = 0xD0;
			fdc_irq_raise();
			fdc.incommand = 0;
			fdc.params    = 0;
			fdc.curparam  = 0;
			fdccallback   = 0;
		} else {
			disc_writesector(fdc.drive, fdc.sector, fdc.track, fdc.side, fdc.density);
			fdc_dma_raise();
		}
		break;

	case FD_CMD_READ_DATA_MFM:
		fdc.sector++;
		if (fdc.sector > fdc.parameters[5])
			fdc.tc = 1;
		if (fdc.tc) {
			fdcsend(fdc.st0);
			fdcsend(fdc.st1);
			fdcsend(fdc.st2);
			fdcsend(fdc.track);
			fdcsend((fdc.parameters[0] & 4) ? 1 : 0);
			fdcsend(fdc.sector);
			fdcsend(fdc.parameters[4]);
			fdc.status = 0xD0;
			fdc_irq_raise();
			fdc.incommand = 0;
			fdc.params    = 0;
			fdc.curparam  = 0;
			fdccallback   = 0;
		} else {
			disc_readsector(fdc.drive, fdc.sector, fdc.track, fdc.side, fdc.density);
		}
		break;

	case FD_CMD_READ_ID_FM:
		fdc.st0       = 0x40 | (fdc.parameters[0] & 7);
		fdc.st1       = 1;
		fdc.st2       = 1;
		fdc.incommand = 0;
		fdc.params    = 0;
		fdc.curparam  = 0;
		fdc_irq_raise();
		break;

	case FD_CMD_FORMAT_TRACK_MFM:
		fdcsend(fdc.st0);
		fdcsend(fdc.st1);
		fdcsend(fdc.st2);
		fdcsend(0);
		fdcsend(0);
		fdcsend(0);
		fdcsend(0);
		fdc.status = 0xD0;
		fdc_irq_raise();
		fdc.incommand = 0;
		fdc.params    = 0;
		fdc.curparam  = 0;
		break;

	case FD_CMD_VERIFY_DATA_MFM:
		/* Verified OK (amazing!), jump straight to the end */
		if (fdc.parameters[0] & 0x80 /* EC */) {
			fdc.sector = fdc.parameters[7];
		} else {
			fdc.sector = fdc.parameters[5];
		}
		fdcsend(fdc.st0);
		fdcsend(fdc.st1);
		fdcsend(fdc.st2);
		fdcsend(fdc.track);
		fdcsend((fdc.parameters[0] & 4) ? 1 : 0);
		fdcsend(fdc.sector);
		fdcsend(fdc.parameters[4]);
		fdc.status = 0xD0;
		fdc_irq_raise();
		fdc.incommand = 0;
		fdc.params    = 0;
		fdc.curparam  = 0;
		break;
	}
}

static void fdc_overrun(void)
{
	disc_stop(fdc.drive);

	fdcsend(0x40 | (fdc.side ? 4 : 0) | fdc.drive);
	fdcsend(0x10); /*Overrun*/
	fdcsend(0);
	fdcsend(fdc.track);
	fdcsend(fdc.side);
	fdcsend(fdc.sector);
	fdcsend(fdc.parameters[4]);
	fdc_end_command();
}

void fdc_data(uint8_t dat)
{
	if (fdc.tc || !fdc.in_read)
		return;

	if (fdc.data_ready) {
		fdc_overrun();
	} else {
		fdc.dmadat = dat;
		fdc.data_ready = 1;
		fdc_dma_raise();
	}
}

void fdc_finishread(void)
{
	fdccallback = 25;
}

void fdc_notfound(void)
{
	fdcsend(0x40 | (fdc.side ? 4 : 0) | fdc.drive);
	fdcsend(5);
	fdcsend(0);
	fdcsend(0);
	fdcsend(0);
	fdcsend(0);
	fdcsend(0);
	fdc_end_command();
}

void fdc_datacrcerror(void)
{
	fdcsend(0x40 | (fdc.side ? 4 : 0) | fdc.drive);
	fdcsend(0x20); /*Data error*/
	fdcsend(0x20); /*Data error in data field*/
	fdcsend(fdc.track);
	fdcsend(fdc.side);
	fdcsend(fdc.sector);
	fdcsend(fdc.parameters[4]);
	fdc_end_command();
}

void fdc_headercrcerror(void)
{
	fdcsend(0x40 | (fdc.side ? 4 : 0) | fdc.drive);
	fdcsend(0x20); /*Data error*/
	fdcsend(0);
	fdcsend(fdc.track);
	fdcsend(fdc.side);
	fdcsend(fdc.sector);
	fdcsend(fdc.parameters[4]);
	fdc_end_command();
}

void fdc_writeprotect(void)
{
	fdcsend(0x40 | (fdc.side ? 4 : 0) | fdc.drive);
	fdcsend(0x02); /*Not writeable*/
	fdcsend(0);
	fdcsend(0);
	fdcsend(0);
	fdcsend(0);
	fdcsend(0);
	fdc_end_command();
}

int fdc_getdata(int last)
{
	uint8_t temp;

	if (!fdc.written && !fdc.tc)
		return -1;
	if (!last && !fdc.tc)
		fdc_dma_raise();
	fdc.written = 0;
	temp = fdc.dmadat;
	return temp;
}

void fdc_sectorid(uint8_t track, uint8_t side, uint8_t sector, uint8_t size)
{
	fdcsend((fdc.side ? 4 : 0) | fdc.drive);
	fdcsend(0);
	fdcsend(0);
	fdcsend(track);
	fdcsend(side);
	fdcsend(sector);
	fdcsend(size);
	fdc_end_command();
}

void fdc_indexpulse(void)
{
	iomd.irqa.status |= IOMD_IRQA_FLOPPY_INDEX;
	updateirqs();
}

uint8_t
fdc_dma_read(uint32_t addr)
{
	fdc_dma_lower();
	fdc.data_ready = 0;
	if (addr == 0x302a000) {
		fdc.tc = 1;
		fdc.st0        = 0;
	}
	return fdc.dmadat;
}

void
fdc_dma_write(uint32_t addr, uint8_t val)
{
	fdc_dma_lower();
	if (addr == 0x302a000) {
		fdc.tc = 1;
		fdc.st0        = 0;
	}
	fdc.written = 1;
	fdc.dmadat = val;
}
