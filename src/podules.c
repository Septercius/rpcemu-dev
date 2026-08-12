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

#include <string.h>

#include "rpcemu.h"
#include "iomd.h"
#include "podules.h"

/* References
  Acorn Enhanced Expansion Card Specification
  Risc PC Technical Reference Manual
*/

/*Podules -
  0 is reserved for extension ROMs
  1 is for additional IDE interface
  2-3 are free
  4-7 are not implemented (yet)*/
static podule podules[8];
static int freepodule;

/**
 * Reset and empty all the podule slots
 *
 * Safe to call on program startup and user instigated virtual machine
 * reset.
 */
void
podules_reset(void)
{
	int c;

	/* Call any reset functions that an open podule may have to allow
	   then to tidy open files etc */
	for (c = 0; c < 8; c++) {
		if (podules[c].reset != NULL) {
			podules[c].reset(&podules[c]);
		}
	}

	// Blank all 8 podules
	memset(podules, 0, 8 * sizeof(podule));

	freepodule = 0;
}

/**
 * Add a new podule to the chain, with the specified functions, including reads and
 * writes to the podules memory areas, and calls for regular callbacks and reset.
 *
 * @param writel        Function pointer for the podule's 32-bit write function
 * @param writew        Function pointer for the podule's 16-bit write function
 * @param writeb        Function pointer for the podule's  8-bit write function
 * @param readl         Function pointer for the podule's 32-bit read function
 * @param readw         Function pointer for the podule's 16-bit read function
 * @param readb         Function pointer for the podule's  8-bit read function
 * @param timercallback
 * @param reset         Function pointer for the podule's reset function, called
 *                      at program startup and emulated machine reset
 * @return Pointer to entry in the podules array, or NULL on failure
 */
podule *
addpodule(void (*writel)(podule *p, PoduleIoType io_type, uint32_t addr, uint32_t val),
          void (*writew)(podule *p, PoduleIoType io_type, uint32_t addr, uint16_t val),
          void (*writeb)(podule *p, PoduleIoType io_type, uint32_t addr, uint8_t val),
          uint32_t (*readl)(podule *p, PoduleIoType io_type, uint32_t addr),
          uint16_t (*readw)(podule *p, PoduleIoType io_type, uint32_t addr),
          uint8_t  (*readb)(podule *p, PoduleIoType io_type, uint32_t addr),
          int (*timercallback)(podule *p),
          void (*reset)(podule *p))
{
	if (freepodule == 8) {
		return NULL; // All podules in use!
	}

	podules[freepodule].readl = readl;
	podules[freepodule].readw = readw;
	podules[freepodule].readb = readb;
	podules[freepodule].writel = writel;
	podules[freepodule].writew = writew;
	podules[freepodule].writeb = writeb;
	podules[freepodule].timercallback = timercallback;
	podules[freepodule].reset = reset;

	return &podules[freepodule++];
}

/**
 * Raise interrupts if any podules have requested them.
 */
static void
rethinkpoduleints(void)
{
	int c;

	iomd.irqb.status &= ~(IOMD_IRQB_PODULE | IOMD_IRQB_PODULE_FIQ_AS_IRQ);
	iomd.fiq.status  &= ~IOMD_FIQ_PODULE;

	for (c = 0; c < 8; c++) {
		if (podules[c].irq) {
			iomd.irqb.status |= IOMD_IRQB_PODULE;
		}
		if (podules[c].fiq) {
			iomd.irqb.status |= IOMD_IRQB_PODULE_FIQ_AS_IRQ;
			iomd.fiq.status  |= IOMD_FIQ_PODULE;
		}
	}
	updateirqs();
}

/**
 * Raise FIQ for the specified podule.
 *
 * @param p Pointer to 'podule' struct for the specified Podule
 */
void
podule_fiq_raise(podule *p)
{
	p->fiq = 1;
	rethinkpoduleints();
}

/**
 * Clear FIQ for the specified podule.
 *
 * @param p Pointer to 'podule' struct for the specified Podule
 */
void
podule_fiq_lower(podule *p)
{
	p->fiq = 0;
	rethinkpoduleints();
}

/**
 * Raise IRQ for the specified podule.
 *
 * @param p Pointer to 'podule' struct for the specified Podule
 */
void
podule_irq_raise(podule *p)
{
	p->irq = 1;
	rethinkpoduleints();
}

/**
 * Clear IRQ for the specified podule.
 *
 * @param p Pointer to 'podule' struct for the specified Podule
 */
void
podule_irq_lower(podule *p)
{
	p->irq = 0;
	rethinkpoduleints();
}

/**
 * Handle a 32-bit write to the podules memory map
 *
 * @param num     Podule number (0-7)
 * @param io_type Write to IOC, MEMC or EASI space
 * @param addr    Address to write to
 * @param val     Value to write
 */
void
podules_write32(int num, PoduleIoType io_type, uint32_t addr, uint32_t val)
{
	const int oldirq = podules[num].irq;
	const int oldfiq = podules[num].fiq;

	if (podules[num].writel != NULL) {
		podules[num].writel(&podules[num], io_type, addr, val);
	}
	if (podules[num].irq != oldirq || podules[num].fiq != oldfiq) {
		rethinkpoduleints();
	}
}

/**
 * Handle a 16-bit write to the podules memory map
 *
 * @param num     Podule number (0-7)
 * @param io_type Write to IOC, MEMC or EASI space
 * @param addr    Address to write to
 * @param val     Value to write
 */
void
podules_write16(int num, PoduleIoType io_type, uint32_t addr, uint16_t val)
{
	const int oldirq = podules[num].irq;
	const int oldfiq = podules[num].fiq;

	if (podules[num].writew != NULL) {
		podules[num].writew(&podules[num], io_type, addr, val);
	}
	if (podules[num].irq != oldirq || podules[num].fiq != oldfiq) {
		rethinkpoduleints();
	}
}

/**
 * Handle an 8-bit write to the podules memory map
 *
 * @param num     Podule number (0-7)
 * @param io_type Write to IOC, MEMC or EASI space
 * @param addr    Address to write to
 * @param val     Value to write
 */
void
podules_write8(int num, PoduleIoType io_type, uint32_t addr, uint8_t val)
{
	const int oldirq = podules[num].irq;
	const int oldfiq = podules[num].fiq;

	if (podules[num].writeb != NULL) {
		podules[num].writeb(&podules[num], io_type, addr, val);
	}
	if (podules[num].irq != oldirq || podules[num].fiq != oldfiq) {
		rethinkpoduleints();
	}
}

/**
 * Handle a 32-bit read from the podules memory map
 *
 * @param num     Podule number (0-7)
 * @param io_type Read from IOC, MEMC or EASI space
 * @param addr    Address to read from
 * @return Value at memory address
 */
uint32_t
podules_read32(int num, PoduleIoType io_type, uint32_t addr)
{
	const int oldirq = podules[num].irq;
	const int oldfiq = podules[num].fiq;
	uint32_t temp;

	if (podules[num].readl != NULL) {
		temp = podules[num].readl(&podules[num], io_type, addr);
		if (podules[num].irq != oldirq || podules[num].fiq != oldfiq) {
			rethinkpoduleints();
		}
		return temp;
	}
	return 0xffffffff;
}

/**
 * Handle a 16-bit read from the podules memory map
 *
 * @param num     Podule number (0-7)
 * @param io_type Read from IOC, MEMC or EASI space
 * @param addr    Address to read from
 * @return Value at memory address
 */
uint16_t
podules_read16(int num, PoduleIoType io_type, uint32_t addr)
{
	const int oldirq = podules[num].irq;
	const int oldfiq = podules[num].fiq;
	uint16_t temp;

	if (podules[num].readw != NULL) {
		temp = podules[num].readw(&podules[num], io_type, addr);
		if (podules[num].irq != oldirq || podules[num].fiq != oldfiq) {
			rethinkpoduleints();
		}
		return temp;
	}
	return 0xffff;
}

/**
 * Handle an 8-bit read from the podules memory map
 *
 * @param num     Podule number (0-7)
 * @param io_type Read from IOC, MEMC or EASI space
 * @param addr    Address to read from
 * @return Value at memory address
 */
uint8_t
podules_read8(int num, PoduleIoType io_type, uint32_t addr)
{
	const int oldirq = podules[num].irq;
	const int oldfiq = podules[num].fiq;
	uint8_t temp;

	if (podules[num].readb != NULL) {
		temp = podules[num].readb(&podules[num], io_type, addr);
		if (podules[num].irq != oldirq || podules[num].fiq != oldfiq) {
			rethinkpoduleints();
		}
		return temp;
	}
	return 0xff;
}

/**
 * AT MOMENT NO PODULE REGISTERS A timercallback() SO THIS FUNCTION IS SUPERFLUOUS
 *
 * @param t
 */
void
runpoduletimers(int t)
{
	int c, d;

	/* Loop through podules, ignoring 0 (extn rom) */
	/* This should really make use of the 'freepodule' variable to prevent
	   looping over podules that aren't registered */
	for (c = 1; c < 8; c++) {
		if (podules[c].timercallback != NULL && podules[c].msectimer != 0) {
			podules[c].msectimer -= t;
			d = 1;
			while (podules[c].msectimer <= 0 && d != 0) {
				const int oldirq = podules[c].irq;
				const int oldfiq = podules[c].fiq;

				d = podules[c].timercallback(&podules[c]);
				if (d == 0) {
					podules[c].msectimer = 0;
				} else {
					podules[c].msectimer += d;
				}
				if (podules[c].irq != oldirq || podules[c].fiq != oldfiq) {
					rethinkpoduleints();
				}
			}
		}
	}
}
