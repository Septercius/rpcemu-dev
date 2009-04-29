/* SMC 37C665GT PC style Super IO chip
   Combination
    Floppy Controller
    IDE Controller
    Serial
    Parallel
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "rpcemu.h"
#include "fdc.h"
#include "vidc20.h"
#include "iomd.h"
#include "ide.h"
#include "arm.h"

/* The chip supports entering a 'configuration' mode,
   allowing the behaviour of the chip to be altered.

   It is entered by
   writing 0x55 to register 0x3f0
   writing 0x55 to register 0x3f0 again

   It is exited by
   writing 0xaa to register 0x3f0

   Once in 'configuration' mode a value can
   be written to the internal configuration by
   writing 'which value' to register 0x3f0, then
   writing 'new value' to register 0x3f1

   Configuration is read from 'configuration' mode by
   writing 'which value' to register 0x3f0, then
   reading the value from register 0x3f1
*/

#define SUPERIO_MODE_NORMAL		0
#define SUPERIO_MODE_INTERMEDIATE	1
#define SUPERIO_MODE_CONFIGURATION	2

static int configmode = 0;
static uint8_t configregs[16];
static int configreg;

static uint8_t scratch, linectrl;


void superio_reset(void)
{
        configregs[0xA]=0;
        configregs[0xD]=0x65;
        configregs[0xE]=2;
        configregs[0xF]=0;
        fdc_reset();
}


void superio_write(uint32_t addr, uint32_t val)
{
	static unsigned char printstat = 0;

        /* Convert memory-mapped address to IO port */
        addr = (addr >> 2) & 0x3ff;

	if (configmode != SUPERIO_MODE_CONFIGURATION) {
		if ((addr == 0x3f0) && (val == 0x55)) {
			/* Attempting to enter configuration mode */
			if (configmode == SUPERIO_MODE_NORMAL) {
				configmode = SUPERIO_MODE_INTERMEDIATE;
			} else if (configmode == SUPERIO_MODE_INTERMEDIATE) {
				configmode = SUPERIO_MODE_CONFIGURATION;
			}
			return;
		} else {
			configmode = SUPERIO_MODE_NORMAL;
		}
	}

	if (configmode == SUPERIO_MODE_CONFIGURATION) {
		if (addr == 0x3f0) {
			if (val == 0xaa) {
				/* Leave configuration mode */
				configmode = SUPERIO_MODE_NORMAL;
			} else {
				/* Select a configuration register */
				configreg = val & 0xf;
			}
		} else {
			/* Write to a configuration register */
			configregs[configreg] = val;
		}
		return;
	}

	if ((addr >= 0x1f0 && addr <= 0x1f7) || addr == 0x3f6) {
		/* IDE */
		ideboard = 0;
		writeide(addr, val);

	} else if ((addr >= 0x278) && (addr <= 0x27f)) {
		/* Parallel */
		if (addr == 0x27a) {
			if ((val & 0x10) || ((printstat ^ val) & 1 && !(val & 1))) {
				// rpclog("Printer interrupt %02X\n", iomd.irqa.mask);
				iomd.irqa.status |= 1;
				updateirqs();
			}

			printstat = val;
		} else {
			UNIMPLEMENTED("Parallel write",
			              "Unknown register 0x%03x", addr);
		}

	} else if ((addr >= 0x3f0) && (addr <= 0x3f7)) {
		/* Floppy */
		writefdc(addr, val);

	} else if ((addr >= 0x3f8) && (addr <= 0x3ff)) {
		/* Serial Port 1 */
		if ((addr == 0x3f9) && (val & 2))
		{
			// printf("Serial transmit empty interrupt\n");
			iomd.fiq.status |= 0x10;
			updateirqs();
		} else if (addr == 0x3fb) {
			linectrl = val;
		} else if (addr == 0x3fe) {
			scratch = val;
		} else {
			UNIMPLEMENTED("Serial1 write",
			              "Unknown register 0x%03x", addr);
		}

	} else {
		UNIMPLEMENTED("SuperIO write",
		              "Unknown register 0x%03x", addr);

	}
}



uint8_t superio_read(uint32_t addr)
{
        /* Convert memory-mapped address to IO port */
        addr = (addr >> 2) & 0x3ff;

	if (configmode == SUPERIO_MODE_CONFIGURATION && addr == 0x3f1) {
		/* Read from configuration register */
		return configregs[configreg];
	}

	if ((addr >= 0x1f0 && addr <= 0x1f7) || addr == 0x3f6) {
		/* IDE */
		ideboard = 0;
		return readide(addr);

	} else if ((addr >= 0x278) && (addr <= 0x27f)) {
		/* Parallel */
		if (addr == 0x279) {
			return 0x90;
		} else {
			UNIMPLEMENTED("Parallel read",
			              "Unknown register 0x%03x", addr);
		}

	} else if ((addr >= 0x3f0) && (addr <= 0x3f7)) {
		/* Floppy */
		return readfdc(addr);

	} else if ((addr >= 0x3f8) && (addr <= 0x3ff)) {
		/* Serial Port 1 */
		if (addr == 0x3fa) {
			iomd.fiq.status &= ~0x10;
			updateirqs();
			return 2;
		} else if (addr == 0x3fb) {
			return linectrl;
		} else if (addr == 0x3fe) {
			return scratch;
		} else {
			UNIMPLEMENTED("Serial1 read",
			              "Unknown register 0x%03x", addr);
		}

	} else {
		UNIMPLEMENTED("SuperIO read",
		              "Unknown register 0x%03x", addr);
	}

	return 0;
}
