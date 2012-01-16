/*RPCemu v0.6 by Tom Walker
  IDE emulation*/

void callbackide(void);

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <sys/types.h>

#include <allegro.h>

#include "rpcemu.h"
#include "vidc20.h"
#include "mem.h"
#include "iomd.h"
#include "ide.h"
#include "arm.h"

/* Bits of 'atastat' */
#define ERR_STAT		0x01
#define DRQ_STAT		0x08 /* Data request */
#define READY_STAT		0x40
#define BUSY_STAT		0x80

/* Bits of 'error' */
#define ABRT_ERR		0x04 /* Command aborted */
#define MCR_ERR			0x08 /* Media change request */

/* ATA Commands */
#define WIN_SRST			0x08 /* ATAPI Device Reset */
#define WIN_RECAL			0x10
#define WIN_RESTORE			WIN_RECAL
#define WIN_READ			0x20 /* 28-Bit Read */
#define WIN_WRITE			0x30 /* 28-Bit Write */
#define WIN_VERIFY			0x40 /* 28-Bit Verify */
#define WIN_FORMAT			0x50
#define WIN_SEEK			0x70
#define WIN_SPECIFY			0x91 /* Initialize Drive Parameters */
#define WIN_PACKETCMD			0xA0 /* Send a packet command. */
#define WIN_PIDENTIFY			0xA1 /* Identify ATAPI device */
#define WIN_SETIDLE1			0xE3
#define WIN_IDENTIFY			0xEC /* Ask drive to identify itself */

/* ATAPI Commands */
#define GPCMD_INQUIRY			0x12
#define GPCMD_MODE_SELECT_10		0x55
#define GPCMD_MODE_SENSE_10		0x5a
#define GPCMD_PAUSE_RESUME		0x4b
#define GPCMD_PLAY_AUDIO_12		0xa5
#define GPCMD_READ_CD			0xbe
#define GPCMD_READ_HEADER		0x44
#define GPCMD_READ_SUBCHANNEL		0x42
#define GPCMD_READ_TOC_PMA_ATIP		0x43
#define GPCMD_REQUEST_SENSE		0x03
#define GPCMD_SEEK			0x2b
#define GPCMD_SEND_DVD_STRUCTURE	0xad
#define GPCMD_SET_SPEED			0xbb
#define GPCMD_START_STOP_UNIT		0x1b
#define GPCMD_TEST_UNIT_READY		0x00

/* Mode page codes for mode sense/set */
#define GPMODE_R_W_ERROR_PAGE		0x01
#define GPMODE_CDROM_PAGE		0x0d
#define GPMODE_CAPABILITIES_PAGE	0x2a
#define GPMODE_ALL_PAGES		0x3f

/* ATAPI Sense Keys */
#define SENSE_NONE		0
#define SENSE_NOT_READY		2
#define SENSE_ILLEGAL_REQUEST	5
#define SENSE_UNIT_ATTENTION	6

/* ATAPI Additional Sense Codes */
#define ASC_ILLEGAL_OPCODE		0x20
#define ASC_MEDIUM_NOT_PRESENT		0x3a

/* Tell RISC OS that we have a 4x CD-ROM drive (600kb/sec data, 706kb/sec raw).
   Not that it means anything */
#define CDROM_SPEED	706

/** Evaluate to non-zero if the currently selected drive is an ATAPI device */
#define IDE_DRIVE_IS_CDROM(ide) \
	(config.cdromenabled && (ide.drive | ide.board) == 1)

ATAPI *atapi;
int idecallback = 0;

static void callreadcd(void);
static void atapicommand(void);

static struct
{
        unsigned char atastat[4];
        unsigned char error;
        int secount,sector,cylinder,head,drive,cylprecomp;
        unsigned char command;
        unsigned char fdisk;
        int pos;
        int packlen;
        int spt[4],hpc[4];
        int packetstatus;
        int cdpos,cdlen;
        unsigned char asc;
        int discchanged;
        int board;
        int reset;
        FILE *hdfile[4];
        int skip512[4];
        uint16_t buffer[65536];
} ide;

static inline void
ide_irq_raise(void)
{
	if (ide.board == 0) {
		iomd.irqb.status |= IOMD_IRQB_IDE;
		updateirqs();
	}
}

static inline void
ide_irq_lower(void)
{
	if (ide.board == 0) {
		iomd.irqb.status &= ~IOMD_IRQB_IDE;
		updateirqs();
	}
}

/**
 * Copy a string into a buffer, padding with spaces, and placing characters as
 * if they were packed into 16-bit values, stored little-endian.
 *
 * @param str Destination buffer
 * @param src Source string
 * @param len Length of destination buffer to fill in. Strings shorter than
 *            this length will be padded with spaces.
 */
static void
ide_padstr(char *str, const char *src, int len)
{
	int i, v;

	for (i = 0; i < len; i++) {
		if (*src != '\0') {
			v = *src++;
		} else {
			v = ' ';
		}
		str[i ^ 1] = v;
	}
}

/**
 * Copy a string into a buffer, padding with spaces. Does not add string
 * terminator.
 *
 * @param buf      Destination buffer
 * @param buf_size Size of destination buffer to fill in. Strings shorter than
 *                 this length will be padded with spaces.
 * @param src      Source string
 */
static void
ide_padstr8(uint8_t *buf, int buf_size, const char *src)
{
	int i;

	for (i = 0; i < buf_size; i++) {
		if (*src != '\0') {
			buf[i] = *src++;
		} else {
			buf[i] = ' ';
		}
	}
}

/**
 * Fill in ide.buffer with the output of the "IDENTIFY DEVICE" command
 */
static void
ide_identify(void)
{
	memset(ide.buffer, 0, 512);

	//ide.buffer[1] = 101; /* Cylinders */
	ide.buffer[1] = 65535; /* Cylinders */
	ide.buffer[3] = 16;  /* Heads */
	ide.buffer[6] = 63;  /* Sectors */
	ide_padstr((char *) (ide.buffer + 10), "", 20); /* Serial Number */
	ide_padstr((char *) (ide.buffer + 23), "v1.0", 8); /* Firmware */
	ide_padstr((char *) (ide.buffer + 27), "RPCEmuHD", 40); /* Model */
	ide.buffer[50] = 0x4000; /* Capabilities */
}

/**
 * Fill in ide.buffer with the output of the "IDENTIFY PACKET DEVICE" command
 */
static void
ide_atapi_identify(void)
{
	memset(ide.buffer, 0, 512);

	ide.buffer[0] = 0x8000 | (5<<8) | 0x80; /* ATAPI device, CD-ROM drive, removable media */
	ide_padstr((char *) (ide.buffer + 10), "", 20); /* Serial Number */
	ide_padstr((char *) (ide.buffer + 23), "v1.0", 8); /* Firmware */
	ide_padstr((char *) (ide.buffer + 27), "RPCEmuCD", 40); /* Model */
	ide.buffer[49] = 0x200; /* LBA supported */
}

/**
 * Fill in ide.buffer with the output of the ATAPI "MODE SENSE" command
 *
 * @param pos Offset within the buffer to start filling in data
 *
 * @return Offset within the buffer after the end of the data
 */
static uint32_t
ide_atapi_mode_sense(uint32_t pos)
{
	uint8_t *buf = (uint8_t *) ide.buffer;

	/* &01 - Read error recovery */
	buf[pos++] = GPMODE_R_W_ERROR_PAGE;
	buf[pos++] = 6; /* Page length */
	buf[pos++] = 0; /* Error recovery parameters */
	buf[pos++] = 3; /* Read retry count */
	buf[pos++] = 0; /* Reserved */
	buf[pos++] = 0; /* Reserved */
	buf[pos++] = 0; /* Reserved */
	buf[pos++] = 0; /* Reserved */

	/* &0D - CD-ROM Parameters */
	buf[pos++] = GPMODE_CDROM_PAGE;
	buf[pos++] = 6; /* Page length */
	buf[pos++] = 0; /* Reserved */
	buf[pos++] = 1; /* Inactivity time multiplier *NEEDED BY RISCOS* value is a guess */
	buf[pos++] = 0; buf[pos++] = 60; /* MSF settings */
	buf[pos++] = 0; buf[pos++] = 75; /* MSF settings */

	/* &2A - CD-ROM capabilities and mechanical status */
	buf[pos++] = GPMODE_CAPABILITIES_PAGE;
	buf[pos++] = 0x12; /* Page length */
	buf[pos++] = 0; buf[pos++] = 0; /* CD-R methods */
	buf[pos++] = 1; /* Supports audio play, not multisession */
	buf[pos++] = 0; /* Some other stuff not supported */
	buf[pos++] = 0; /* Some other stuff not supported (lock state + eject) */
	buf[pos++] = 0; /* Some other stuff not supported */
	buf[pos++] = (uint8_t) (CDROM_SPEED >> 8);
	buf[pos++] = (uint8_t) CDROM_SPEED; /* Maximum speed */
	buf[pos++] = 0; buf[pos++] = 2; /* Number of audio levels - on and off only */
	buf[pos++] = 0; buf[pos++] = 0; /* Buffer size - none */
	buf[pos++] = (uint8_t) (CDROM_SPEED >> 8);
	buf[pos++] = (uint8_t) CDROM_SPEED; /* Current speed */
	buf[pos++] = 0; /* Reserved */
	buf[pos++] = 0; /* Drive digital format */
	buf[pos++] = 0; /* Reserved */
	buf[pos++] = 0; /* Reserved */

	return pos;
}

/*
 * Return the sector offset for the current register values
 */
static off64_t
ide_get_sector(void)
{
	int heads = ide.hpc[ide.drive | ide.board];
	int sectors = ide.spt[ide.drive | ide.board];
	int skip = ide.skip512[ide.drive | ide.board];

	return ((((off64_t) ide.cylinder * heads) + ide.head) *
	          sectors) + (ide.sector - 1) + skip;
}

/**
 * Move to the next sector using CHS addressing
 */
static void
ide_next_sector(void)
{
	ide.sector++;
	if (ide.sector == (ide.spt[ide.drive | ide.board] + 1)) {
		ide.sector = 1;
		ide.head++;
		if (ide.head == ide.hpc[ide.drive | ide.board]) {
			ide.head = 0;
			ide.cylinder++;
		}
	}
}

static void loadhd(int d, const char *fn)
{
	char pathname[512];

	append_filename(pathname, rpcemu_get_datadir(), fn, 512);

	if (ide.hdfile[d] == NULL) {
		/* Try to open existing hard disk image */
		ide.hdfile[d] = fopen64(pathname, "rb+");
		if (ide.hdfile[d] == NULL) {
			/* Failed to open existing hard disk image */
			if (errno == ENOENT) {
				/* Failed because it does not exist,
				   so try to create new file */
				ide.hdfile[d] = fopen64(pathname, "wb+");
				if (ide.hdfile[d] == NULL) {
					fatal("Cannot create file '%s': %s",
					      pathname, strerror(errno));
				}
			} else {
				/* Failed for another reason */
				fatal("Cannot open file '%s': %s",
				      pathname, strerror(errno));
			}
		}
	}

        fseek(ide.hdfile[d], 0xfc1, SEEK_SET);
        ide.spt[d] = getc(ide.hdfile[d]);
        ide.hpc[d] = getc(ide.hdfile[d]);
        ide.skip512[d] = 1;
//        rpclog("First check - spt %i hpc %i\n",ide.spt[0],ide.hpc[0]);
        if (!ide.spt[d] || !ide.hpc[d])
        {
                fseek(ide.hdfile[d], 0xdc1, SEEK_SET);
                ide.spt[d] = getc(ide.hdfile[d]);
                ide.hpc[d] = getc(ide.hdfile[d]);
//                rpclog("Second check - spt %i hpc %i\n",ide.spt[0],ide.hpc[0]);
                ide.skip512[d] = 0;
                if (!ide.spt[d] || !ide.hpc[d])
                {
                        ide.spt[d]=63;
                        ide.hpc[d]=16;
                        ide.skip512[d] = 1;
//        rpclog("Final check - spt %i hpc %i\n",ide.spt[0],ide.hpc[0]);
                }
        }
}

void resetide(void)
{
        int d;

        /* Close hard disk image files (if previously open) */
        for (d = 0; d < 4; d++) {
                if (ide.hdfile[d] != NULL) {
                        fclose(ide.hdfile[d]);
                        ide.hdfile[d] = NULL;
                }
        }

        ide.atastat[0] = ide.atastat[2] = READY_STAT;
        idecallback = 0;
        loadhd(0,"hd4.hdf");
        if (config.cdromenabled)
        {
                /* Hard disk images for ICS IDE disabled
                loadhd(2,"hd5.hdf");
                loadhd(3,"hd6.hdf");
                */
        }
        else
        {
                loadhd(1,"hd5.hdf");
                /* Hard disk images for ICS IDE disabled
                loadhd(2,"hd6.hdf");
                loadhd(3,"hd7.hdf");
                */
        }
}

void writeidew(uint16_t val)
{
//        rpclog("Write data %08X %04X %i %07X %08X\n",ide.pos,val,ide.packetstatus,PC,armregs[5]);
#ifdef _RPCEMU_BIG_ENDIAN
		val=(val>>8)|(val<<8);
#endif
        ide.buffer[ide.pos >> 1] = val;
        ide.pos+=2;

        if (ide.packetstatus==4)
        {
                if (ide.pos>=(ide.packlen+2))
                {
                        ide.packetstatus=5;
                        idecallback=6;
//                        rpclog("Packet over!\n");
                        ide_irq_lower();
                }
                return;
        }
        else if (ide.packetstatus==5) return;
        else if (ide.command == WIN_PACKETCMD && ide.pos>=0xC)
        {
                ide.pos=0;
                ide.atastat[ide.board] = BUSY_STAT;
                ide.packetstatus=1;
                idecallback=60;
//                rpclog("Packet now waiting!\n");
        }
        else if (ide.pos>=512)
        {
                ide.pos=0;
                ide.atastat[ide.board] = BUSY_STAT;
                idecallback=0;
                callbackide();
        }
}

void writeide(uint16_t addr, uint8_t val)
{
        uint8_t *idebufferb = (uint8_t *) ide.buffer;
//        int c;
//        rpclog("Write IDE %08X %02X %08X %08X\n",addr,val,PC-8,armregs[12]);

        switch (addr)
        {
        case 0x1F0: /* Data */
                idebufferb[ide.pos++]=val;
                if (ide.pos>=512)
                {
                        ide.pos=0;
                        ide.atastat[ide.board] = BUSY_STAT;
                        idecallback=1000;
                }
                return;

        case 0x1F1: /* Features */
                ide.cylprecomp=val;
                return;

        case 0x1F2: /* Sector count */
                ide.secount=val;
                return;

        case 0x1F3: /* Sector */
                ide.sector=val;
                return;

        case 0x1F4: /* Cylinder low */
                ide.cylinder=(ide.cylinder&0xFF00)|val;
                return;

        case 0x1F5: /* Cylinder high */
                ide.cylinder=(ide.cylinder&0xFF)|(val<<8);
                return;

        case 0x1F6: /* Drive/Head */
                ide.head=val&0xF;
                if (((val>>4)&1)!=ide.drive)
                {
                        idecallback=0;
                        ide.atastat[ide.board] = READY_STAT;
                        ide.error=0;
                        ide.secount=1;
                        ide.sector=1;
                        ide.head=0;
                        ide.cylinder=0;
                        ide.cylprecomp=0;
                        ide.reset = 0;
                        ide.command=0;
                        ide.packetstatus=0;
                        ide.packlen=0;
                        ide.cdlen=ide.cdpos=ide.pos=0;
                        memset(ide.buffer, 0, 512);
                        ide_irq_lower();
                }
                ide.drive=(val>>4)&1;
                ide.pos=0;
                ide.atastat[ide.board] = READY_STAT;
                return;

        case 0x1F7: /* Command register */
                ide.command=val;
//                rpclog("New IDE command - %02X %i %i %08X\n",ide.command,ide.drive,ide.board,PC-8);
                ide.error=0;
                switch (val)
                {
                case WIN_SRST: /* ATAPI Device Reset */
                        ide.atastat[ide.board] = READY_STAT;
                        idecallback=100;
                        return;

                case WIN_RESTORE:
                case WIN_SEEK:
                        ide.atastat[ide.board] = READY_STAT;
                        idecallback=100;
                        return;

                case WIN_READ:
/*                        if (ide.secount>1)
                        {
                                fatal("Read %i sectors from sector %i cylinder %i head %i\n",ide.secount,ide.sector,ide.cylinder,ide.head);
                        }*/
//                        rpclog("Read %i sectors from sector %i cylinder %i head %i\n",ide.secount,ide.sector,ide.cylinder,ide.head);
                        ide.atastat[ide.board] = BUSY_STAT;
                        idecallback=200;
                        return;

                case WIN_WRITE:
/*                        if (ide.secount>1)
                        {
                                fatal("Write %i sectors to sector %i cylinder %i head %i\n",ide.secount,ide.sector,ide.cylinder,ide.head);
                        }*/
//                        rpclog("Write %i sectors to sector %i cylinder %i head %i\n",ide.secount,ide.sector,ide.cylinder,ide.head);
                        ide.atastat[ide.board] = DRQ_STAT;
                        ide.pos=0;
                        return;

                case WIN_VERIFY:
//                        rpclog("Read verify %i sectors from sector %i cylinder %i head %i\n",ide.secount,ide.sector,ide.cylinder,ide.head);
                        ide.atastat[ide.board] = BUSY_STAT;
                        idecallback=200;
                        return;

                case WIN_FORMAT:
//                        rpclog("Format track %i head %i\n",ide.cylinder,ide.head);
                        ide.atastat[ide.board] = DRQ_STAT;
//                        idecallback=200;
                        ide.pos=0;
                        return;

                case WIN_SPECIFY: /* Initialize Drive Parameters */
                        ide.atastat[ide.board] = BUSY_STAT;
                        idecallback=200;
                        return;

                case WIN_PIDENTIFY: /* Identify Packet Device */
                case WIN_SETIDLE1: /* Idle */
                        ide.atastat[ide.board] = BUSY_STAT;
                        idecallback=200;
                        return;

                case WIN_IDENTIFY: /* Identify Device */
                        ide.atastat[ide.board] = BUSY_STAT;
                        idecallback=200;
                        return;

                case WIN_PACKETCMD: /* ATAPI Packet */
                        ide.packetstatus=0;
                        ide.atastat[ide.board] = BUSY_STAT;
                        idecallback=30;
                        ide.pos=0;
                        return;
                }
                fatal("Bad IDE command %02X\n", val);
                return;

        case 0x3F6: /* Device control */
                if ((ide.fdisk&4) && !(val&4))
                {
                        idecallback=500;
                        ide.reset = 1;
                        ide.atastat[ide.board] = BUSY_STAT;
//                        rpclog("IDE Reset\n");
                }
                ide.fdisk=val;
                return;
        }
        fatal("Bad IDE write %04X %02X\n", addr, val);
}

uint8_t readide(uint16_t addr)
{
        const uint8_t *idebufferb = (const uint8_t *) ide.buffer;
        uint8_t temp;

        switch (addr)
        {
        case 0x1F0: /* Data */
//                rpclog("Read data %08X ",ide.pos);
                temp=idebufferb[ide.pos++];
//                rpclog("%04X\n",temp);
                if (ide.pos>=512)
                {
                        ide.pos=0;
                        ide.atastat[ide.board] = READY_STAT;
//                        rpclog("End of transfer\n");
                }
                return temp;

        case 0x1F1: /* Error */
                return ide.error;

        case 0x1F2: /* Sector count */
                return (uint8_t)ide.secount;

        case 0x1F3: /* Sector */
                return (uint8_t)ide.sector;

        case 0x1F4: /* Cylinder low */
                return (uint8_t)(ide.cylinder&0xFF);

        case 0x1F5: /* Cylinder high */
                return (uint8_t)(ide.cylinder>>8);

        case 0x1F6: /* Drive/Head */
                return (uint8_t)(ide.head|(ide.drive<<4));

        case 0x1F7: /* Status */
                ide_irq_lower();
                return ide.atastat[ide.board];

        case 0x3F6: /* Alternate Status */
                return ide.atastat[ide.board];
        }
        fatal("Bad IDE read %04X\n", addr);
}

uint16_t readidew(void)
{
        uint16_t temp;

        temp = ide.buffer[ide.pos >> 1];
	#ifdef _RPCEMU_BIG_ENDIAN
		temp=(temp>>8)|(temp<<8);
	#endif
        ide.pos+=2;
        if ((ide.pos>=512 && ide.command != WIN_PACKETCMD) || (ide.command == WIN_PACKETCMD && ide.pos>=ide.packlen))
        {
//                rpclog("Over! packlen %i %i\n",ide.packlen,ide.pos);
                ide.pos=0;
                if (ide.command == WIN_PACKETCMD && ide.packetstatus==6)
                {
                        callreadcd();
                }
                else
                {
                        ide.atastat[ide.board] = READY_STAT;
                        ide.packetstatus=0;
                        if (ide.command == WIN_READ)
                        {
                                ide.secount--;
                                if (ide.secount)
                                {
                                        ide_next_sector();
                                        ide.atastat[ide.board] = BUSY_STAT;
                                        idecallback=0;
                                        callbackide();
                                }
                        }
                }
        }
        return temp;
}

void callbackide(void)
{
        off64_t addr;
        int c;
        if (ide.reset)
        {
                ide.atastat[ide.board] = READY_STAT;
                ide.error=0;
                ide.secount=1;
                ide.sector=1;
                ide.head=0;
                ide.cylinder=0;
                ide.reset = 0;
//                rpclog("Reset callback\n");
                return;
        }
        switch (ide.command)
        {
        case WIN_SRST: /*ATAPI Device Reset */
                ide.atastat[ide.board] = READY_STAT;
                ide.error=1; /*Device passed*/
                ide.secount = ide.sector = 1;
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        ide.cylinder = 0xeb14;
                } else {
                        ide.cylinder = 0;
                }
                ide_irq_raise();
                return;

        case WIN_RESTORE:
        case WIN_SEEK:
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        goto abort_cmd;
                }
//                rpclog("Restore callback\n");
                ide.atastat[ide.board] = READY_STAT;
                ide_irq_raise();
                return;

        case WIN_READ:
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        goto abort_cmd;
                }
                addr = ide_get_sector() * 512;
//                rpclog("Read %i %i %i %08X\n",ide.cylinder,ide.head,ide.sector,addr);
                /*                if (ide.cylinder || ide.head)
                {
                        fatal("Read from other cylinder/head");
                }*/
                fseeko64(ide.hdfile[ide.drive | ide.board], addr, SEEK_SET);
                fread(ide.buffer, 512, 1, ide.hdfile[ide.drive | ide.board]);
                ide.pos=0;
                ide.atastat[ide.board] = DRQ_STAT;
//                rpclog("Read sector callback %i %i %i offset %08X %i left %i\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount,ide.spt);
                ide_irq_raise();
                return;

        case WIN_WRITE:
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        goto abort_cmd;
                }
                addr = ide_get_sector() * 512;
//                rpclog("Write sector callback %i %i %i offset %08X %i left %i\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount,ide.spt);
                fseeko64(ide.hdfile[ide.drive | ide.board], addr, SEEK_SET);
                fwrite(ide.buffer, 512, 1, ide.hdfile[ide.drive | ide.board]);
                ide_irq_raise();
                ide.secount--;
                if (ide.secount)
                {
                        ide.atastat[ide.board] = DRQ_STAT;
                        ide.pos=0;
                        ide_next_sector();
                }
                else
                   ide.atastat[ide.board] = READY_STAT;
                return;

        case WIN_VERIFY:
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        goto abort_cmd;
                }
                ide.pos=0;
                ide.atastat[ide.board] = READY_STAT;
//                rpclog("Read verify callback %i %i %i offset %08X %i left\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount);
                ide_irq_raise();
                return;

        case WIN_FORMAT:
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        goto abort_cmd;
                }
                addr = ide_get_sector() * 512;
//                rpclog("Format cyl %i head %i offset %08X %08X %08X secount %i\n",ide.cylinder,ide.head,addr,addr>>32,addr,ide.secount);
                fseeko64(ide.hdfile[ide.drive | ide.board], addr, SEEK_SET);
                memset(ide.buffer, 0, 512);
                for (c=0;c<ide.secount;c++)
                {
                        fwrite(ide.buffer, 512, 1, ide.hdfile[ide.drive | ide.board]);
                }
                ide.atastat[ide.board] = READY_STAT;
                ide_irq_raise();
                return;

        case WIN_SPECIFY: /* Initialize Drive Parameters */
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        goto abort_cmd;
                }
                ide.spt[ide.drive|ide.board]=ide.secount;
                ide.hpc[ide.drive|ide.board]=ide.head+1;
                ide.atastat[ide.board] = READY_STAT;
//                rpclog("%i sectors per track, %i heads per cylinder\n",ide.spt,ide.hpc);
                ide_irq_raise();
                return;

        case WIN_PIDENTIFY: /* Identify Packet Device */
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        ide_atapi_identify();
                        ide.pos=0;
                        ide.error=0;
                        ide.atastat[ide.board] = DRQ_STAT;
                        ide_irq_raise();
                        return;
                }
                goto abort_cmd;

        case WIN_SETIDLE1: /* Idle */
                goto abort_cmd;

        case WIN_IDENTIFY: /* Identify Device */
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        ide.secount=1;
                        ide.sector=1;
                        ide.cylinder=0xEB14;
                        ide.drive=ide.head=0;
                        goto abort_cmd;
                }
                ide_identify();
                ide.pos=0;
                ide.atastat[ide.board] = DRQ_STAT;
//                rpclog("ID callback\n");
                ide_irq_raise();
                return;

        case WIN_PACKETCMD: /* ATAPI Packet */
//                rpclog("Packet callback! %i\n",ide.packetstatus);
                if (!ide.packetstatus)
                {
                        ide.pos=0;
                        ide.error=(uint8_t)((ide.secount&0xF8)|1);
                        ide.atastat[ide.board] = DRQ_STAT;
                        ide_irq_raise();
//                        rpclog("Preparing to recieve packet max DRQ count %04X\n",ide.cylinder);
                }
                else if (ide.packetstatus==1)
                {
                        ide.atastat[ide.board] = BUSY_STAT;
                        
                        atapicommand();
//                        exit(-1);
                }
                else if (ide.packetstatus==2)
                {
//                        rpclog("packetstatus==2\n");
                        ide.atastat[ide.board] = READY_STAT;
                        ide_irq_raise();
                }
                else if (ide.packetstatus==3)
                {
                        ide.atastat[ide.board] = DRQ_STAT;
//                        rpclog("Recieve data packet!\n");
                        ide_irq_raise();
                        ide.packetstatus=0xFF;
                }
                else if (ide.packetstatus==4)
                {
                        ide.atastat[ide.board] = DRQ_STAT;
//                        rpclog("Send data packet!\n");
                        ide_irq_raise();
//                        ide.packetstatus=5;
                        ide.pos=2;
                }
                else if (ide.packetstatus==5)
                {
//                        rpclog("Packetstatus 5 !\n");
                        atapicommand();
                }
                else if (ide.packetstatus==6) /*READ CD callback*/
                {
                        ide.atastat[ide.board] = DRQ_STAT;
//                        rpclog("Recieve data packet 6!\n");
                        ide_irq_raise();
//                        ide.packetstatus=0xFF;
                }
                else if (ide.packetstatus==0x80) /*Error callback*/
                {
                        ide.atastat[ide.board] = READY_STAT | ERR_STAT;
                        ide_irq_raise();
                }
                return;
        }

abort_cmd:
	ide.atastat[ide.board] = READY_STAT | ERR_STAT;
	ide.error = ABRT_ERR;
	ide_irq_raise();
}

/*ATAPI CD-ROM emulation
  This mostly seems to work. It is implemented only on Windows at the moment as
  I haven't had time to implement any interfaces for it in the generic gui.
  It mostly depends on driver files - cdrom-iso.c for ISO image files (in theory
  on any platform) and cdrom-ioctl.c for Win32 IOCTL access. There's an ATAPI
  interface defined in ide.h.
  There are a couple of bugs in the CD audio handling.
  */

static void atapi_notready(void)
{
        /*Medium not present is 02/3A/--*/
        /*cylprecomp is error number*/
        /*SENSE/ASC/ASCQ*/
        ide.atastat[0] = READY_STAT | ERR_STAT;    /*CHECK CONDITION*/
        ide.error = (SENSE_NOT_READY << 4) | ABRT_ERR;
        if (ide.discchanged) {
                ide.error |= MCR_ERR;
        }
        ide.discchanged=0;
        ide.asc = ASC_MEDIUM_NOT_PRESENT;
        ide.packetstatus=0x80;
        idecallback=50;
}

void atapi_discchanged(void)
{
        ide.discchanged=1;
}

static void atapicommand(void)
{
        uint8_t *idebufferb = (uint8_t *) ide.buffer;
        int c;
        int len;
        int msf;
        int pos=0;
//        rpclog("New ATAPI command %02X\n",idebufferb[0]);
                msf=idebufferb[1]&2;

        switch (idebufferb[0])
        {
        case GPCMD_TEST_UNIT_READY:
                if (!atapi->ready()) { atapi_notready(); return; }
//                if (atapi->ready())
//                {
                        ide.packetstatus=2;
                        idecallback=50;
//                }
//                else
//                {
//                        rpclog("Medium not present!\n");
//                }
                break;

        case GPCMD_REQUEST_SENSE: /* Used by ROS 4+ */
                /*Will return 18 bytes of 0*/
                memset(idebufferb,0,512);
                ide.packetstatus=3;
                ide.cylinder=18;
                ide.secount=2;
                ide.pos=0;
                idecallback=60;
                ide.packlen=18;
                break;

        case GPCMD_SET_SPEED:
                ide.packetstatus=2;
                idecallback=50;
                break;

        case GPCMD_READ_TOC_PMA_ATIP:
                if (!atapi->ready()) { atapi_notready(); return; }
                if (idebufferb[9]>>7)
                {
                        rpclog("Bad read TOC format\n");
                        rpclog("Packet data :\n");
                        for (c=0;c<12;c++)
                            rpclog("%02X ",idebufferb[c]);
                        rpclog("\n");
                        exit(-1);
                }
                len=atapi->readtoc(idebufferb,idebufferb[6],msf);
  /*      rpclog("ATAPI buffer len %i\n",len);
        for (c=0;c<len;c++) rpclog("%02X ",idebufferb[c]);
        rpclog("\n");*/
        ide.packetstatus=3;
        ide.cylinder=len;
        ide.secount=2;
//        ide.atastat = DRQ_STAT;
        ide.pos=0;
                idecallback=60;
                ide.packlen=len;
//        rpclog("Sending packet\n");
        return;
        
                switch (idebufferb[6])
                {
                        case 0xAA: /*Start address of lead-out*/
                        break;
                }
                rpclog("Read bad track %02X in read TOC\n",idebufferb[6]);
                rpclog("Packet data :\n");
                for (c=0;c<12;c++)
                    rpclog("%02X\n",idebufferb[c]);
                exit(-1);
                
        case GPCMD_READ_CD:
                if (!atapi->ready()) { atapi_notready(); return; }
//                rpclog("Read CD : start LBA %02X%02X%02X%02X Length %02X%02X%02X Flags %02X\n",idebufferb[2],idebufferb[3],idebufferb[4],idebufferb[5],idebufferb[6],idebufferb[7],idebufferb[8],idebufferb[9]);
                if (idebufferb[9]!=0x10)
                {
                        rpclog("Bad flags bits %02X\n",idebufferb[9]);
                        exit(-1);
                }
/*                if (idebufferb[6] || idebufferb[7] || (idebufferb[8]!=1))
                {
                        rpclog("More than 1 sector!\n");
                        exit(-1);
                }*/
                ide.cdlen=(idebufferb[6]<<16)|(idebufferb[7]<<8)|idebufferb[8];
                ide.cdpos=(idebufferb[2]<<24)|(idebufferb[3]<<16)|(idebufferb[4]<<8)|idebufferb[5];
//                rpclog("Read at %08X %08X\n",ide.cdpos,ide.cdpos*2048);
                atapi->readsector(idebufferb,ide.cdpos);

                ide.cdpos++;
                ide.cdlen--;
                if (ide.cdlen>=0) ide.packetstatus=6;
                else              ide.packetstatus=3;
                ide.cylinder=2048;
                ide.secount=2;
                ide.pos=0;
                idecallback=60;
                ide.packlen=2048;
                return;
                
        case GPCMD_READ_HEADER:
                if (!atapi->ready()) { atapi_notready(); return; }
                if (msf)
                {
                        rpclog("Read Header MSF!\n");
                        exit(-1);
                }
                for (c=0;c<4;c++) idebufferb[c+4]=idebufferb[c+2];
                idebufferb[0]=1; /*2048 bytes user data*/
                idebufferb[1]=idebufferb[2]=idebufferb[3]=0;
                
                ide.packetstatus=3;
                ide.cylinder=8;
                ide.secount=2;
                ide.pos=0;
                idecallback=60;
                ide.packlen=8;
                return;
                
        case GPCMD_MODE_SENSE_10:
                if (!atapi->ready()) { atapi_notready(); return; }
                if (idebufferb[2] != GPMODE_ALL_PAGES)
                {
                        rpclog("Bad mode sense - not 3F\n");
                        rpclog("Packet data :\n");
                        for (c=0;c<12;c++)
                            rpclog("%02X\n",idebufferb[c]);
                        exit(-1);
                }
                len=(idebufferb[8]|(idebufferb[7]<<8));
//                rpclog("Mode sense! %i\n",len);
                for (c=0;c<len;c++) idebufferb[c]=0;
                /*Set mode parameter header - bytes 0 & 1 are data length (filled out later),
                  byte 2 is media type*/
                idebufferb[2]=1; /*120mm data CD-ROM*/

                len = ide_atapi_mode_sense(8);

                idebufferb[0]=len>>8;
                idebufferb[1]=len&255;
/*        rpclog("ATAPI buffer len %i\n",len);
        for (c=0;c<len;c++) rpclog("%02X ",idebufferb[c]);
        rpclog("\n");*/
        ide.packetstatus=3;
        ide.cylinder=len;
        ide.secount=2;
//        ide.atastat = DRQ_STAT;
        ide.pos=0;
                idecallback=60;
                ide.packlen=len;
//        rpclog("Sending packet\n");
        return;

        case GPCMD_MODE_SELECT_10:
                if (!atapi->ready()) { atapi_notready(); return; }
                if (ide.packetstatus==5)
                {
                        ide.atastat[0] = 0;
//                        rpclog("Recieve data packet!\n");
                        ide_irq_raise();
                        ide.packetstatus=0xFF;
                        ide.pos=0;
                }
                else
                {
                        len=(idebufferb[7]<<8)|idebufferb[8];
                        ide.packetstatus=4;
                        ide.cylinder=len;
                        ide.secount=2;
                        ide.pos=0;
                        idecallback=6;
                        ide.packlen=len;
/*                        rpclog("Waiting for ARM to send packet %i\n",len);
                rpclog("Packet data :\n");
                for (c=0;c<12;c++)
                    rpclog("%02X ",idebufferb[c]);
                    rpclog("\n");*/
                }
                return;

        case GPCMD_PLAY_AUDIO_12:
                if (!atapi->ready()) { atapi_notready(); return; }
                /*This is apparently deprecated in the ATAPI spec, and apparently
                  has been since 1995 (!). Hence I'm having to guess most of it*/
                pos=(idebufferb[3]<<16)|(idebufferb[4]<<8)|idebufferb[5];
                len=(idebufferb[7]<<16)|(idebufferb[8]<<8)|idebufferb[9];
                atapi->playaudio(pos,len);
                ide.packetstatus=2;
                idecallback=50;
                break;

        case GPCMD_READ_SUBCHANNEL:
                if (!atapi->ready()) { atapi_notready(); return; }
                if (idebufferb[3]!=1)
                {
                        rpclog("Bad read subchannel!\n");
                        rpclog("Packet data :\n");
                        for (c=0;c<12;c++)
                            rpclog("%02X\n",idebufferb[c]);
                        dumpregs();
                        exit(-1);
                }
                pos=0;
                idebufferb[pos++]=0;
                idebufferb[pos++]=0; /*Audio status*/
                idebufferb[pos++]=0; idebufferb[pos++]=0; /*Subchannel length*/
                idebufferb[pos++]=1; /*Format code*/
                idebufferb[1]=atapi->getcurrentsubchannel(&idebufferb[5],msf);
                len=11+5;
                ide.packetstatus=3;
                ide.cylinder=len;
                ide.secount=2;
                ide.pos=0;
                idecallback=60;
                ide.packlen=len;
                break;

        case GPCMD_START_STOP_UNIT:
                if (idebufferb[4]!=2 && idebufferb[4]!=3 && idebufferb[4])
                {
                        rpclog("Bad start/stop unit command\n");
                        rpclog("Packet data :\n");
                        for (c=0;c<12;c++)
                            rpclog("%02X\n",idebufferb[c]);
                        exit(-1);
                }
                if (!idebufferb[4])        atapi->stop();
                else if (idebufferb[4]==2) atapi->eject();
                else                       atapi->load();
                ide.packetstatus=2;
                idecallback=50;
                break;
                
        case GPCMD_INQUIRY:
                idebufferb[0] = 5; /*CD-ROM*/
                idebufferb[1] = 0;
                idebufferb[2] = 0;
                idebufferb[3] = 0;
                idebufferb[4] = 31;
                idebufferb[5] = 0;
                idebufferb[6] = 0;
                idebufferb[7] = 0;
                ide_padstr8(idebufferb + 8, 8, "RPCEmu"); /* Vendor */
                ide_padstr8(idebufferb + 16, 16, "RPCEmuCD"); /* Product */
                ide_padstr8(idebufferb + 32, 4, "v1.0"); /* Revision */

                len=36;
                ide.packetstatus=3;
                ide.cylinder=len;
                ide.secount=2;
                ide.pos=0;
                idecallback=60;
                ide.packlen=len;
                break;
                
        case GPCMD_PAUSE_RESUME:
                if (!atapi->ready()) { atapi_notready(); return; }
                if (idebufferb[8]&1) atapi->resume();
                else                 atapi->pause();
                ide.packetstatus=2;
                idecallback=50;
                break;

        case GPCMD_SEEK:
                if (!atapi->ready()) { atapi_notready(); return; }
                pos=(idebufferb[3]<<16)|(idebufferb[4]<<8)|idebufferb[5];
                atapi->seek(pos);
                ide.packetstatus=2;
                idecallback=50;
                break;

        case GPCMD_SEND_DVD_STRUCTURE:
        default:
                ide.atastat[0] = READY_STAT | ERR_STAT;    /*CHECK CONDITION*/
                ide.error = (SENSE_ILLEGAL_REQUEST << 4) | ABRT_ERR;
                if (ide.discchanged) {
                        ide.error |= MCR_ERR;
                }
                ide.discchanged=0;
                ide.asc = ASC_ILLEGAL_OPCODE;
                ide.packetstatus=0x80;
                idecallback=50;
                break;
                
/*                default:
                rpclog("Bad ATAPI command %02X\n",idebufferb[0]);
                rpclog("Packet data :\n");
                for (c=0;c<12;c++)
                    rpclog("%02X\n",idebufferb[c]);
                exit(-1);*/
        }
}

static void callreadcd(void)
{
        ide_irq_lower();
        if (ide.cdlen<=0)
        {
                ide.packetstatus=2;
                idecallback=20;
                return;
        }
//        rpclog("Continue readcd! %i blocks left\n",ide.cdlen);
        ide.atastat[0] = BUSY_STAT;
        
        atapi->readsector((uint8_t *) ide.buffer, ide.cdpos);

                ide.cdpos++;
                ide.cdlen--;
                ide.packetstatus=6;
                ide.cylinder=2048;
                ide.secount=2;
                ide.pos=0;
                idecallback=60;
                ide.packlen=2048;
}
