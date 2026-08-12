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

#ifndef PODULES_H
#define PODULES_H

#include <stdint.h>

typedef enum {
	/// Access is in IOC space
	PODULE_IO_TYPE_IOC = 0,
	/// Access is in MEMC space
	PODULE_IO_TYPE_MEMC,
	/// Access is in EASI space (not Archimedes)
	PODULE_IO_TYPE_EASI,
} PoduleIoType;

void podules_write32(int num, PoduleIoType io_type, uint32_t addr, uint32_t val);
void podules_write16(int num, PoduleIoType io_type, uint32_t addr, uint16_t val);
void podules_write8(int num, PoduleIoType io_type, uint32_t addr, uint8_t val);
uint32_t podules_read32(int num, PoduleIoType io_type, uint32_t addr);
uint16_t podules_read16(int num, PoduleIoType io_type, uint32_t addr);
uint8_t  podules_read8(int num, PoduleIoType io_type, uint32_t addr);

typedef struct podule {
	void (*writeb)(struct podule *p, PoduleIoType io_type, uint32_t addr, uint8_t val);
	void (*writew)(struct podule *p, PoduleIoType io_type, uint32_t addr, uint16_t val);
	void (*writel)(struct podule *p, PoduleIoType io_type, uint32_t addr, uint32_t val);
	uint8_t  (*readb)(struct podule *p, PoduleIoType io_type, uint32_t addr);
	uint16_t (*readw)(struct podule *p, PoduleIoType io_type, uint32_t addr);
	uint32_t (*readl)(struct podule *p, PoduleIoType io_type, uint32_t addr);
	int (*timercallback)(struct podule *p);
	void (*reset)(struct podule *p);
	int irq;
	int fiq;
	int msectimer;
} podule;

void podule_fiq_raise(podule *p);
void podule_fiq_lower(podule *p);

void podule_irq_raise(podule *p);
void podule_irq_lower(podule *p);

podule *addpodule(void (*writel)(podule *p, PoduleIoType io_type, uint32_t addr, uint32_t val),
              void (*writew)(podule *p, PoduleIoType io_type, uint32_t addr, uint16_t val),
              void (*writeb)(podule *p, PoduleIoType io_type, uint32_t addr, uint8_t val),
              uint32_t (*readl)(podule *p, PoduleIoType io_type, uint32_t addr),
              uint16_t (*readw)(podule *p, PoduleIoType io_type, uint32_t addr),
              uint8_t  (*readb)(podule *p, PoduleIoType io_type, uint32_t addr),
              int (*timercallback)(podule *p),
              void (*reset)(podule *p));

void runpoduletimers(int t);
void podules_reset(void);

#endif
