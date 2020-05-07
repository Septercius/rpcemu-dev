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

#ifndef MEM_H
#define MEM_H

#include <stdint.h>

#include "rpcemu.h"

extern uint32_t mem_phys_read32(uint32_t addr);

extern uint32_t readmemfl(uint32_t addr);
extern uint32_t readmemfb(uint32_t addr);
extern void writememfb(uint32_t addr, uint8_t val);
extern void writememfl(uint32_t addr, uint32_t val);

extern void clearmemcache(void);
extern void mem_init(void);
extern void mem_reset(uint32_t ramsize, uint32_t vram_size);

extern uintptr_t vraddrl[0x100000];
extern uint32_t vraddrls[1024],vraddrphys[1024];

extern uintptr_t vwaddrl[0x100000];
extern uint32_t vwaddrls[1024],vwaddrphys[1024];

//uint8_t pagedirty[0x1000];
#define HASH(l) (((l)>>2)&0x7FFF)

#define ROMSIZE (8*1024*1024)

extern uint32_t *ram00, *ram01, *ram1, *rom, *vram;
extern uint8_t *romb;

extern uint32_t tlbcache[0x100000];
#define translateaddress(addr,rw,prefetch) ((/*!((addr)&0xFC000000) && */!(tlbcache[((addr)>>12)/*&0x3FFF*/]&0xFFF))?(tlbcache[(addr)>>12]|((addr)&0xFFF)):translateaddress2(addr,rw,prefetch))

extern int mmu,memmode;

extern void cacheclearpage(uint32_t a);

extern uint32_t mem_rammask;
extern uint32_t mem_vrammask;

/**
 * Read a 32-bit word from a virtual address.
 *
 * Performs direct access if possible.
 *
 * @param addr Virtual address
 * @return 32-bit word read from given virtual address
 */
static inline uint32_t
mem_read32(uint32_t addr)
{
	if (vraddrl[addr >> 12] & 1) {
		return readmemfl(addr);
	} else {
		return *((const uint32_t *) (addr + vraddrl[addr >> 12]));
	}
}

/**
 * Read a byte from a virtual address.
 *
 * Performs direct access if possible.
 *
 * @param addr Virtual address
 * @return Byte read from given virtual address
 */
static inline uint32_t
mem_read8(uint32_t addr)
{
	if (vraddrl[addr >> 12] & 1) {
		return readmemfb(addr);
	} else {
#ifdef _RPCEMU_BIG_ENDIAN
		return *((const uint8_t *) ((addr ^ 3) + vraddrl[addr >> 12]));
#else
		return *((const uint8_t *) (addr + vraddrl[addr >> 12]));
#endif
	}
}

/**
 * Write a 32-bit word to a virtual address.
 *
 * Performs direct access if possible.
 *
 * @param addr Virtual address
 * @param val  32-bit word to write
 */
static inline void
mem_write32(uint32_t addr, uint32_t val)
{
	if (vwaddrl[addr >> 12] & 3) {
		writememfl(addr, val);
	} else {
		*((uint32_t *) (addr + vwaddrl[addr >> 12])) = val;
	}
}

/**
 * Write a byte to a virtual address.
 *
 * Performs direct access if possible.
 *
 * @param addr Virtual address
 * @param val  Byte to write
 */
static inline void
mem_write8(uint32_t addr, uint8_t val)
{
	if (vwaddrl[addr >> 12] & 3) {
		writememfb(addr, val);
	} else {
#ifdef _RPCEMU_BIG_ENDIAN
		*((uint8_t *) ((addr ^ 3) + vwaddrl[addr >> 12])) = val;
#else
		*((uint8_t *) (addr + vwaddrl[addr >> 12])) = val;
#endif
	}
}

#endif /* MEM_H */
