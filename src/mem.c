/*RPCemu v0.6 by Tom Walker
  Memory handling*/
#include <assert.h>

#include "rpcemu.h"
#include "vidc20.h"
#include "mem.h"
#include "iomd.h"
#include "ide.h"
#include "arm.h"
#include "cp15.h"
#include "superio.h"
#include "podules.h"
#include "fdc.h"

/* References -
   Acorn Risc PC - Technical Reference Manual
*/

uint32_t *ram00 = NULL; /**< Word pointer to SIMM 0 Bank 0 of physical RAM */
uint32_t *ram01 = NULL; /**< Word pointer to SIMM 0 Bank 1 of physical RAM */
uint32_t *ram1  = NULL; /**< Word pointer to SIMM 1 of physical RAM */
uint32_t *rom   = NULL; /**< Word pointer to ROM */
uint32_t *vram  = NULL; /**< Word pointer to Video RAM */

int mmu = 0;     /**< Bool of whether the MMU is enabled */
int memmode = 0; /**< Bool of whether ARM is in a privileged mode */

uint32_t mem_rammask; /**< Mask used for SIMM Bank 0/1 to handle the repeating address space */

static uint8_t *ramb00 = NULL; /**< Byte pointer to SIMM 0 Bank 0 of physical RAM */
static uint8_t *ramb01 = NULL; /**< Byte pointer to SIMM 0 Bank 1 of physical RAM */
static uint8_t *ramb1  = NULL; /**< Byte pointer to SIMM 1 of physical RAM */
uint8_t *romb = NULL;          /**< Byte pointer to ROM */
static uint8_t *vramb  = NULL; /**< Byte pointer to Video RAM */

static uint32_t readmemcache = 0,readmemcache2 = 0;
static uint32_t writememcache = 0,writememcache2 = 0;
static uint32_t writemembcache = 0,writemembcache2 = 0;

static uint32_t phys_space_mask; /**< Mask used to convert to physical memory address space */

void clearmemcache(void)
{
	readmemcache = 0xffffffff;
	writememcache = 0xffffffff;
	writemembcache = 0xffffffff;
}

static int vraddrlpos, vwaddrlpos;

/**
 * Initialise memory (called only once on program startup)
 */
void mem_init(void)
{
	rom  = malloc(ROMSIZE);
	vram = malloc(8 * 1024 * 1024); /*8 meg VRAM!*/
	romb  = (uint8_t *) rom;
	vramb = (uint8_t *) vram;
}

/**
 * Initialise/reset RAM (called on startup and emulated machine reset)
 *
 * @param ramsize Amount of RAM in megabytes
 */
void mem_reset(uint32_t ramsize)
{
	assert(ramsize >= 4); /* At least 4MB */
	assert(ramsize <= 256); /* At most 256MB */
	assert(((ramsize - 1) & ramsize) == 0); /* Must be a power of 2 */

	/* Convert ramsize from bytes to megabytes */
	ramsize *= (1024 * 1024);

	if (ramsize == (256 * 1024 * 1024)) {
		ramsize = 128 * 1024 * 1024; /* 128MB for first SIMM */

		/* Allocate additional 128MB */
		ram1 = realloc(ram1, 128 * 1024 * 1024);
		ramb1 = (uint8_t *) ram1;
		memset(ram1, 0, 128 * 1024 * 1024);
	} else {
		free(ram1);
		ram1 = NULL;
		ramb1 = NULL;
	}

	/* Calculate mem_rammask */
	mem_rammask = (ramsize / 2) - 1;

	ram00 = realloc(ram00, ramsize / 2);
	ram01 = realloc(ram01, ramsize / 2);
	ramb00 = (uint8_t *) ram00;
	ramb01 = (uint8_t *) ram01;
	memset(ram00, 0, ramsize / 2);
	memset(ram01, 0, ramsize / 2);

	vraddrlpos = vwaddrlpos = 0;

	if (machine.model == Model_Phoebe) {
		/* 30 address bits are connected to IOMD2. This results in a
		   physical memory map of 1G that repeats in the 4G address space */
		phys_space_mask = 0x3fffffff;
	} else {
		/* 29 address bits are connected to IOMD. This results in a
		   physical memory map of 512M that repeats in the 4G address space */
		phys_space_mask = 0x1fffffff;
	}
}

#define vradd(a,v,f,p) if (vraddrls[vraddrlpos]!=0xFFFFFFFF) vraddrl[vraddrls[vraddrlpos]]=0xFFFFFFFF; \
                       vraddrls[vraddrlpos]=a>>12; \
                       vraddrl[a>>12]=(unsigned long)(v);/*|(f);*/ \
                       vraddrphys[vraddrlpos]=p; \
                       vraddrlpos=(vraddrlpos+1)&1023;
                       
#define vwadd(a,v,f,p) cacheclearpage(a>>12); /*Invalidate all code blocks on this page, so that any blocks on this page are forced to be recompiled*/ \
                       if (vwaddrls[vwaddrlpos]!=0xFFFFFFFF) vwaddrl[vwaddrls[vwaddrlpos]]=0xFFFFFFFF; \
                       vwaddrls[vwaddrlpos]=a>>12; \
                       vwaddrl[a>>12]=(unsigned long)(v);/*|(f);*/ \
                       vwaddrphys[vwaddrlpos]=p; \
                       vwaddrlpos=(vwaddrlpos+1)&1023;

/**
 * Read a 32-bit word from a physical address.
 *
 * @param addr Physical address
 * @return 32-bit word read from given physical address
 */
static uint32_t
mem_phys_read32(uint32_t addr)
{
	addr &= phys_space_mask;

	switch (addr & (phys_space_mask & 0xff000000)) { /* Select in 16MB chunks */
	case 0x00000000: /* ROM */
		return rom[(addr & 0x7fffff) >> 2];

	case 0x02000000: /* VRAM */
		if (config.vrammask == 0)
			return 0xffffffff;
		return vram[(addr & config.vrammask) >> 2];

	case 0x03000000: /* IO */
		if ((addr & 0xc00000) == 0) {
			/* 03000000 - 033fffff */
			uint32_t bank = (addr >> 16) & 7;

			switch (bank) {
			case 0:
				/* IOMD Registers */
				return iomd_read(addr);
			case 1:
			case 2:
				if ((addr == 0x3310000) && (machine.iomd_type == IOMDType_IOMD))
					return iomd_mouse_buttons_read();
				if (addr >= 0x3010000 && addr < 0x3012000) {
					/* SuperIO */
					if ((addr & 0xffc) == 0x7c0) {
						return readidew();
					}
					return superio_read(addr);
				}
				break;
			case 4:
				/* Podule space 0, 1, 2, 3 */
				return readpodulew((addr >> 14) & 3, 0, addr & 0x3fff);
			case 7:
				/* Podule space 4, 5, 6, 7 */
				return readpodulew(((addr >> 14) & 3) + 4, 0, addr & 0x3fff);
			}
		}
		if ((machine.model == Model_Phoebe) && (addr & 0xcffffc) == 0x8007c0) {
			return readidew();
		}
		break;

	case 0x08000000: /* EASI space */
	case 0x09000000:
	case 0x0a000000:
	case 0x0b000000:
	case 0x0c000000:
	case 0x0d000000:
	case 0x0e000000:
	case 0x0f000000:
		return readpodulel((addr >> 24) & 7, 1, addr & 0xffffff);

	case 0x10000000: /* SIMM 0 bank 0 */
	case 0x11000000:
	case 0x12000000:
	case 0x13000000:
		return ram00[(addr & mem_rammask) >> 2];

	case 0x14000000: /* SIMM 0 bank 1 */
	case 0x15000000:
	case 0x16000000:
	case 0x17000000:
		return ram01[(addr & mem_rammask) >> 2];

	case 0x18000000: /* SIMM 1 bank 0 */
	case 0x19000000:
	case 0x1a000000:
	case 0x1b000000:
	case 0x1c000000: /* SIMM 1 bank 1 */
	case 0x1d000000:
	case 0x1e000000:
	case 0x1f000000:
		if (ram1 != NULL) {
			return ram1[(addr & 0x7ffffff) >> 2];
		}
	}
	return 0;
}

/**
 * Read a byte from a physical address.
 *
 * @param addr Physical address
 * @return Byte read from given physical address
 */
static uint32_t
mem_phys_read8(uint32_t addr)
{
	addr &= phys_space_mask;

	switch (addr & (phys_space_mask & 0xff000000)) { /* Select in 16MB chunks */
	case 0x00000000: /* ROM */
#ifdef _RPCEMU_BIG_ENDIAN
		addr ^= 3;
#endif
		return romb[addr & 0x7fffff];

	case 0x02000000: /* VRAM */
		if (config.vrammask == 0)
			return 0xff;
#ifdef _RPCEMU_BIG_ENDIAN
		addr ^= 3;
#endif
		return vramb[addr & config.vrammask];

	case 0x03000000: /* IO */
		if ((addr & 0xc00000) == 0) {
			/* 03000000 - 033fffff */
			uint32_t bank = (addr >> 16) & 7;

			switch (bank) {
			case 0:
				/* IOMD Registers */
				return iomd_read(addr);
			case 1:
			case 2:
				if ((addr == 0x3310000) && (machine.iomd_type == IOMDType_IOMD))
					return iomd_mouse_buttons_read();
				if (addr >= 0x3012000 && addr <= 0x302a000)
					return fdc_dma_read(addr);
				if ((addr & 0xfff400) == 0x02b000) {
					/* Network podule */
					return 0xffffffff;
				}
				if (addr >= 0x3010000 && addr < 0x3012000) {
					/* SuperIO */
					return superio_read(addr);
				}
				break;
			case 4:
				/* Podule space 0, 1, 2, 3 */
				return readpoduleb((addr >> 14) & 3, 0, addr & 0x3fff);
			case 7:
				/* Podule space 4, 5, 6, 7 */
				return readpoduleb(((addr >> 14) & 3) + 4, 0, addr & 0x3fff);
			}
		}
		if ((machine.model == Model_Phoebe) && (addr & 0xcff000) == 0x800000) {
			return readide((addr >> 2) & 0x3ff);
		}
		break;

	case 0x08000000: /* EASI space */
	case 0x09000000:
	case 0x0a000000:
	case 0x0b000000:
	case 0x0c000000:
	case 0x0d000000:
	case 0x0e000000:
	case 0x0f000000:
		return readpoduleb((addr >> 24) & 7, 1, addr & 0xffffff);

	case 0x10000000: /* SIMM 0 bank 0 */
	case 0x11000000:
	case 0x12000000:
	case 0x13000000:
#ifdef _RPCEMU_BIG_ENDIAN
		addr ^= 3;
#endif
		return ramb00[addr & mem_rammask];

	case 0x14000000: /* SIMM 0 bank 1 */
	case 0x15000000:
	case 0x16000000:
	case 0x17000000:
#ifdef _RPCEMU_BIG_ENDIAN
		addr ^= 3;
#endif
		return ramb01[addr & mem_rammask];

	case 0x18000000: /* SIMM 1 bank 0 */
	case 0x19000000:
	case 0x1a000000:
	case 0x1b000000:
	case 0x1c000000: /* SIMM 1 bank 1 */
	case 0x1d000000:
	case 0x1e000000:
	case 0x1f000000:
		if (ramb1 != NULL) {
#ifdef _RPCEMU_BIG_ENDIAN
			addr ^= 3;
#endif
			return ramb1[addr & 0x7ffffff];
		}
	}
	return 0xff;
}

/**
 * Write a 32-bit word to a physical address.
 *
 * @param addr Physical address
 * @param val  32-bit word to write
 */
static void
mem_phys_write32(uint32_t addr, uint32_t val)
{
	addr &= phys_space_mask;

	switch (addr & (phys_space_mask & 0xff000000)) { /* Select in 16MB chunks */
	case 0x02000000: /* VRAM */
		if (config.vrammask == 0)
			return;
		vram[(addr & config.vrammask) >> 2] = val;
		dirtybuffer[(addr & config.vrammask) >> 12] = 1;
		break;

	case 0x03000000: /* IO */
		if ((addr & 0xc00000) == 0) {
			uint32_t bank = (addr >> 16) & 7;

			switch (bank) {
			case 0:
				iomd_write(addr, val);
				return;
			case 1:
			case 2:
				if (addr >= 0x3010000 && addr < 0x3012000) {
					/* SuperIO */
					if ((addr & 0xffc) == 0x7c0) {
						writeidew(val);
						return;
					}
					superio_write(addr, val);
					return;
				}
				if ((addr & 0xfff0000) == 0x33a0000) {
					/* Econet? */
					return;
				}
				break;
			case 4:
				/* Podule space 0, 1, 2, 3 */
				writepodulew((addr >> 14) & 3, 0, addr & 0x3fff, val >> 16);
				break;
			case 7:
				/* Podule space 4, 5, 6, 7 */
				writepodulew(((addr >> 14) & 3) + 4, 0, addr & 0x3fff, val >> 16);
				break;
			}
		}
		if ((addr & 0xc00000) == 0x400000) {
			/* VIDC20 */
			writevidc20(val);
			return;
		}
		if ((machine.model == Model_Phoebe) && (addr & 0xcffffc) == 0x8007c0) {
			writeidew(val);
			return;
		}
		break;

	case 0x08000000: /* EASI space */
	case 0x09000000:
	case 0x0a000000:
	case 0x0b000000:
	case 0x0c000000:
	case 0x0d000000:
	case 0x0e000000:
	case 0x0f000000:
		writepodulel((addr >> 24) & 7, 1, addr & 0xffffff, val);
		return;

	case 0x10000000: /* SIMM 0 bank 0 */
	case 0x11000000:
	case 0x12000000:
	case 0x13000000:
		ram00[(addr & mem_rammask) >> 2] = val;
		/* TODO fff00000 only allows blocks on 1MB boundaries not 8MB/16MB
		   this suggests writes to > 1MB inside video mem in dram mode aren't updating dirtybuffer */
		if ((config.vrammask == 0) && ((addr & 0xfff00000) == (iomd.vidstart & 0xfff00000))) {
			dirtybuffer[(addr & mem_rammask) >> 12] = 1;
		}
		return;

	case 0x14000000: /* SIMM 0 bank 1 */
	case 0x15000000:
	case 0x16000000:
	case 0x17000000:
		ram01[(addr & mem_rammask) >> 2] = val;
		return;

	case 0x18000000: /* SIMM 1 bank 0 */
	case 0x19000000:
	case 0x1a000000:
	case 0x1b000000:
	case 0x1c000000: /* SIMM 1 bank 1 */
	case 0x1d000000:
	case 0x1e000000:
	case 0x1f000000:
		if (ram1 != NULL) {
			ram1[(addr & 0x7ffffff) >> 2] = val;
		}
		return;
	}
}

/**
 * Write a byte to a physical address.
 *
 * @param addr Physical address
 * @param val  Byte to write
 */
static void
mem_phys_write8(uint32_t addr, uint8_t val)
{
	addr &= phys_space_mask;

	switch (addr & (phys_space_mask & 0xff000000)) { /* Select in 16MB chunks */
	case 0x02000000: /* VRAM */
		if (config.vrammask == 0)
			return;
#ifdef _RPCEMU_BIG_ENDIAN
		addr ^= 3;
#endif
		vramb[addr & config.vrammask] = val;
		dirtybuffer[(addr & config.vrammask) >> 12] = 1;
		return;

	case 0x03000000: /* IO */
		if ((addr & 0xc00000) == 0) {
			uint32_t bank = (addr >> 16) & 7;

			switch (bank) {
			case 0:
				iomd_write(addr, val);
				return;
			case 1:
			case 2:
				if (addr == 0x3310000)
					return;
				if ((addr & 0xfc0000) == 0x240000)
					return;
				if (addr >= 0x3012000 && addr <= 0x302a000) {
					fdc_dma_write(addr, val);
					return;
				}
				if (addr >= 0x3010000 && addr < 0x3012000) {
					/* SuperIO */
					superio_write(addr, val);
					return;
				}
				if ((addr & 0xfff0000) == 0x33a0000) {
					/* Econet? */
					return;
				}
				break;
			case 4:
				/* Podule space 0, 1, 2, 3 */
				writepoduleb((addr >> 14) & 3, 0, addr & 0x3fff, val);
				break;
			case 7:
				/* Podule space 4, 5, 6, 7 */
				writepoduleb(((addr >> 14) & 3) + 4, 0, addr & 0x3fff, val);
				break;
			}
		}
		if ((machine.model == Model_Phoebe) && (addr & 0xcff000) == 0x800000) {
			writeide((addr >> 2) & 0x3ff, val);
			return;
		}
		break;

	case 0x08000000: /* EASI space */
	case 0x09000000:
	case 0x0a000000:
	case 0x0b000000:
	case 0x0c000000:
	case 0x0d000000:
	case 0x0e000000:
	case 0x0f000000:
		writepoduleb((addr >> 24) & 7, 1, addr & 0xffffff, val);
		return;

	case 0x10000000: /* SIMM 0 bank 0 */
	case 0x11000000:
	case 0x12000000:
	case 0x13000000:
#ifdef _RPCEMU_BIG_ENDIAN
		addr ^= 3;
#endif
		ramb00[addr & mem_rammask] = val;
		/* TODO fff00000 only allows blocks on 1MB boundaries not 8MB/16MB
		   this suggests writes to > 1MB inside video mem in dram mode aren't updating dirtybuffer */
		if ((config.vrammask == 0) && ((addr & 0xfff00000) == (iomd.vidstart & 0xfff00000))) {
			dirtybuffer[(addr & mem_rammask) >> 12] = 1;
		}
		return;

	case 0x14000000: /* SIMM 0 bank 1 */
	case 0x15000000:
	case 0x16000000:
	case 0x17000000:
#ifdef _RPCEMU_BIG_ENDIAN
		addr ^= 3;
#endif
		ramb01[addr & mem_rammask] = val;
		return;

	case 0x18000000: /* SIMM 1 bank 0 */
	case 0x19000000:
	case 0x1a000000:
	case 0x1b000000:
	case 0x1c000000: /* SIMM 1 bank 1 */
	case 0x1d000000:
	case 0x1e000000:
	case 0x1f000000:
		if (ramb1 != NULL) {
#ifdef _RPCEMU_BIG_ENDIAN
			addr ^= 3;
#endif
			ramb1[addr & 0x7ffffff] = val;
		}
		return;
	}
}

uint32_t
readmemfl(uint32_t addr)
{
	uint32_t addr2;

	if (mmu) {
		addr2 = addr;
		if ((addr >> 12) == readmemcache) {
			addr = readmemcache2 + (addr & 0xfff);
		} else {
			readmemcache = addr >> 12;
			armirq &= ~0x40;
			addr = translateaddress(addr, 0, 0);
			if (armirq & 0x40) {
				vraddrl[addr2 >> 12] = readmemcache = 0xffffffff;
				return 0;
			}
			readmemcache2 = addr & 0xfffff000;
		}
		switch (readmemcache2 & (phys_space_mask & 0xff000000)) {
		case 0x00000000: /* ROM */
			vradd(addr2, &rom[((readmemcache2 & 0x7ff000) - (long) (addr2 & ~0xfff)) >> 2], 2, readmemcache2);
			return *(const uint32_t *) ((vraddrl[addr2 >> 12] & ~3) + (addr2 & ~3));

		case 0x02000000: /* VRAM */
			if (config.vrammask != 0) {
				vradd(addr2, &vram[((readmemcache2 & 0x7ff000) - (long) (addr2 & ~0xfff)) >> 2], 0, readmemcache2);
				return *(const uint32_t *) (vraddrl[addr2 >> 12] + (addr2 & ~3));
			}
			break;

		case 0x10000000: /* SIMM 0 bank 0 */
		case 0x11000000:
		case 0x12000000:
		case 0x13000000:
			vradd(addr2, &ram00[((readmemcache2 & mem_rammask) - (long) (addr2 & ~0xfff)) >> 2], 0, readmemcache2);
			return *(const uint32_t *) (vraddrl[addr2 >> 12] + (addr2 & ~3));

		case 0x14000000: /* SIMM 0 bank 1 */
		case 0x15000000:
		case 0x16000000:
		case 0x17000000:
			vradd(addr2, &ram01[((readmemcache2 & mem_rammask) - (long) (addr2 & ~0xfff)) >> 2], 0, readmemcache2);
			return *(const uint32_t *) (vraddrl[addr2 >> 12] + (addr2 & ~3));

		case 0x18000000: /* SIMM 1 bank 0 */
		case 0x19000000:
		case 0x1a000000:
		case 0x1b000000:
		case 0x1c000000: /* SIMM 1 bank 1 */
		case 0x1d000000:
		case 0x1e000000:
		case 0x1f000000:
			if (ram1 != NULL) {
				vradd(addr2, &ram1[((readmemcache2 & 0x7ffffff) - (long) (addr2 & ~0xfff)) >> 2], 0, readmemcache2);
				return *(const uint32_t *) (vraddrl[addr2 >> 12] + (addr2 & ~3));
			}
			break;
		}
	} else {
		switch (addr & (phys_space_mask & 0xff000000)) {
		case 0x00000000: /* ROM */
			//vradd(addr, &rom[((addr & 0x7ff000) - (long) (addr & ~0xfff)) >> 2], 2, addr);
			break;
		case 0x02000000: /* VRAM */
			if (config.vrammask != 0) {
				vradd(addr, &vram[((addr & 0x7ff000) - (long) (addr & ~0xfff)) >> 2], 0, addr);
			}
			break;
		case 0x10000000: /* SIMM 0 bank 0 */
		case 0x11000000:
		case 0x12000000:
		case 0x13000000:
			vradd(addr, &ram00[((addr & mem_rammask & ~0xfff) - (long) (addr & ~0xfff)) >> 2], 0, addr);
			break;
		case 0x14000000: /* SIMM 0 bank 1 */
		case 0x15000000:
		case 0x16000000:
		case 0x17000000:
			vradd(addr, &ram01[((addr & mem_rammask & ~0xfff) - (long) (addr & ~0xfff)) >> 2], 0, addr);
			break;
		case 0x18000000: /* SIMM 1 bank 0 */
		case 0x19000000:
		case 0x1a000000:
		case 0x1b000000:
		case 0x1c000000: /* SIMM 1 bank 1 */
		case 0x1d000000:
		case 0x1e000000:
		case 0x1f000000:
			if (ram1 != NULL) {
				vradd(addr, &ram1[((addr & 0x7ffffff & ~0xfff) - (long) (addr & ~0xfff)) >> 2], 0, addr);
			}
			break;
		}
	}
	/* At this point 'addr' is a physical address */
	return mem_phys_read32(addr);
}


uint32_t
readmemfb(uint32_t addr)
{
	uint32_t addr2;

	if (mmu) {
		addr2 = addr;
		if ((addr >> 12) == readmemcache) {
			addr = readmemcache2 + (addr & 0xfff);
		} else {
			readmemcache = addr >> 12;
			armirq &= ~0x40;
			addr = translateaddress(addr, 0, 0);
			if (armirq & 0x40) {
				readmemcache = 0xffffffff;
				return 0;
			}
			readmemcache2 = addr & 0xfffff000;
		}
		switch (readmemcache2 & (phys_space_mask & 0xff000000)) {
		case 0x00000000: /* ROM */
			vradd(addr2, &rom[((readmemcache2 & 0x7ff000) - (long) (addr2 & ~0xfff)) >> 2], 2, readmemcache2);
#ifdef _RPCEMU_BIG_ENDIAN
			addr2 ^= 3;
#endif
			return *(const uint8_t *) ((vraddrl[addr2 >> 12] & ~3) + addr2);

		case 0x02000000: /* VRAM */
			if (config.vrammask != 0) {
				vradd(addr2, &vram[((readmemcache2 & 0x7ff000) - (long) (addr2 & ~0xfff)) >> 2], 0, readmemcache2);
#ifdef _RPCEMU_BIG_ENDIAN
				addr2 ^= 3;
#endif
				return *(const uint8_t *) (vraddrl[addr2 >> 12] + addr2);
			}
			break;

		case 0x10000000: /* SIMM 0 bank 0 */
		case 0x11000000:
		case 0x12000000:
		case 0x13000000:
			vradd(addr2, &ram00[((readmemcache2 & mem_rammask) - (long) (addr2 & ~0xfff)) >> 2], 0, readmemcache2);
#ifdef _RPCEMU_BIG_ENDIAN
			addr2 ^= 3;
#endif
			return *(const uint8_t *) (vraddrl[addr2 >> 12] + addr2);

		case 0x14000000: /* SIMM 0 bank 1 */
		case 0x15000000:
		case 0x16000000:
		case 0x17000000:
			vradd(addr2, &ram01[((readmemcache2 & mem_rammask) - (long) (addr2 & ~0xfff)) >> 2], 0, readmemcache2);
#ifdef _RPCEMU_BIG_ENDIAN
			addr2 ^= 3;
#endif
			return *(const uint8_t *) (vraddrl[addr2 >> 12] + addr2);

		case 0x18000000: /* SIMM 1 bank 0 */
		case 0x19000000:
		case 0x1a000000:
		case 0x1b000000:
		case 0x1c000000: /* SIMM 1 bank 1 */
		case 0x1d000000:
		case 0x1e000000:
		case 0x1f000000:
			if (ram1 != NULL) {
				vradd(addr2, &ram1[((readmemcache2 & 0x7ffffff) - (long) (addr2 & ~0xfff)) >> 2], 0, readmemcache2);
#ifdef _RPCEMU_BIG_ENDIAN
				addr2 ^= 3;
#endif
				return *(const uint8_t *) (vraddrl[addr2 >> 12] + addr2);
			}
			break;
		}
	}
	/* At this point 'addr' is a physical address */
	return mem_phys_read8(addr);
}

void
writememfl(uint32_t addr, uint32_t val)
{
	uint32_t addr2 = addr;

	if (mmu) {
		if ((addr >> 12) == writememcache) {
			addr = writememcache2 + (addr & 0xfff);
		} else {
			writememcache = addr >> 12;
			armirq &= ~0x40;
			addr = translateaddress(addr, 1, 0);
			if (armirq & 0x40) {
				writememcache = 0xffffffff;
				return;
			}
			writememcache2 = addr & 0xfffff000;
		}
		switch (writememcache2 & (phys_space_mask & 0xff000000)) {
		case 0x02000000: /* VRAM */
			if (config.vrammask != 0) {
				vwadd(addr2, &vram[((writememcache2 & 0x7ff000) - (long) (addr2 & ~0xfff)) >> 2], 0, writememcache2);
			}
			break;

		case 0x10000000: /* SIMM 0 bank 0 */
		case 0x11000000:
		case 0x12000000:
		case 0x13000000:
			vwadd(addr2, &ram00[((writememcache2 & mem_rammask) - (long) (addr2 & ~0xfff)) >> 2], 0, writememcache2);
			break;

		case 0x14000000: /* SIMM 0 bank 1 */
		case 0x15000000:
		case 0x16000000:
		case 0x17000000:
			vwadd(addr2, &ram01[((writememcache2 & mem_rammask) - (long) (addr2 & ~0xfff)) >> 2], 0, writememcache2);
			break;

		case 0x18000000: /* SIMM 1 bank 0 */
		case 0x19000000:
		case 0x1a000000:
		case 0x1b000000:
		case 0x1c000000: /* SIMM 1 bank 1 */
		case 0x1d000000:
		case 0x1e000000:
		case 0x1f000000:
			if (ram1 != NULL) {
				vwadd(addr2, &ram1[((writememcache2 & 0x7ffffff) - (long) (addr2 & ~0xfff)) >> 2], 0, writememcache2);
			}
			break;
		}
	}
	/* At this point 'addr' is a physical address */
	mem_phys_write32(addr, val);
}

void
writememfb(uint32_t addr, uint8_t val)
{
	uint32_t addr2 = addr;

	if (mmu) {
		if ((addr >> 12) == writemembcache) {
			addr = writemembcache2 + (addr & 0xfff);
		} else {
			writemembcache = addr >> 12;
			armirq &= ~0x40;
			addr = translateaddress(addr, 1, 0);
			if (armirq & 0x40) {
				writemembcache = 0xffffffff;
				return;
			}
			writemembcache2 = addr & 0xfffff000;
		}
		switch (writemembcache2 & (phys_space_mask & 0xff000000)) {
		case 0x02000000: /* VRAM */
			if (config.vrammask != 0) {
				vwadd(addr2, &vram[((writemembcache2 & 0x7ff000) - (long) (addr2 & ~0xfff)) >> 2], 0, writemembcache2);
			}
			break;

		case 0x10000000: /* SIMM 0 bank 0 */
		case 0x11000000:
		case 0x12000000:
		case 0x13000000:
			vwadd(addr2, &ram00[((writemembcache2 & mem_rammask) - (long) (addr2 & ~0xfff)) >> 2], 0, writemembcache2);
			break;

		case 0x14000000: /* SIMM 0 bank 1 */
		case 0x15000000:
		case 0x16000000:
		case 0x17000000:
			vwadd(addr2, &ram01[((writemembcache2 & mem_rammask) - (long) (addr2 & ~0xfff)) >> 2], 0, writemembcache2);
			break;

		case 0x18000000: /* SIMM 1 bank 0 */
		case 0x19000000:
		case 0x1a000000:
		case 0x1b000000:
		case 0x1c000000: /* SIMM 1 bank 1 */
		case 0x1d000000:
		case 0x1e000000:
		case 0x1f000000:
			if (ram1 != NULL) {
				vwadd(addr2, &ram1[((writemembcache2 & 0x7ffffff) - (long) (addr2 & ~0xfff)) >> 2], 0, writemembcache2);
			}
			break;
		}
	}
	/* At this point 'addr' is a physical address */
	mem_phys_write8(addr, val);
}
