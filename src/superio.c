/* PC style Super IO chips
 SMC 37C665GT or SMC 37C672
   Combination
    Floppy Controller
    IDE Controller
    Serial
    Parallel
 */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "rpcemu.h"
#include "fdc.h"
#include "vidc20.h"
#include "iomd.h"
#include "ide.h"
#include "arm.h"
#include "i8042.h"

/* The chips support entering a 'configuration' mode,
   allowing the behaviour of the chip to be altered.

   On the 37C665GT it is entered by
   writing 0x55 to register 0x3f0
   writing 0x55 to register 0x3f0 again

   On the 37C672 it is entered by
   writing 0x55 to register 0x3f0

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

#define SMI_IRQ2_IRINT	0x04

static SuperIOType super_type;     /**< Which variant of SuperIO chip are we emulating */

static int configmode = SUPERIO_MODE_NORMAL;
static uint8_t configregs665[16];  /**< Internal configuration registers of FDC 37C665GT */
static uint8_t configregs672[256]; /**< Internal configuration registers of FDC 37C672 */
static int configreg;

static uint8_t scratch, linectrl;

/** FDC37C672 GP Index Registers */
static int gp_index;		/**< Which register to select, 0-15 */
static uint8_t gp_regs[16];	/**< Values of GP registers */

static uint8_t printstat = 0;

static void
superio_smi_update(void)
{
	if ((gp_regs[0xe] & gp_regs[0xc]) || (gp_regs[0xf] & gp_regs[0xd])) {
		iomd.irqb.status |= IOMD2_IRQB_SUPERIO_SMI;
	} else {
		iomd.irqb.status &= ~IOMD2_IRQB_SUPERIO_SMI;
	}
	updateirqs();
}

void
superio_smi_setint1(uint8_t i)
{
	gp_regs[0x0e] |= i;
	superio_smi_update();
}

void
superio_smi_setint2(uint8_t i)
{
	gp_regs[0x0f] |= i;
	superio_smi_update();
}

void
superio_smi_clrint1(uint8_t i)
{
	gp_regs[0x0f] &= ~i;
	superio_smi_update();
}

void
superio_smi_clrint2(uint8_t i)
{
	gp_regs[0x0f] &= ~i;
	superio_smi_update();
}

/**
 * Write to one of the configuration registers of the SuperIO chip, allowing the
 * OS to set the behaviour of the SuperIO subsystems.
 *
 * @param configreg The configuration register to set
 * @param val       The value to write to the configuration register
 */
static void
superio_config_reg_write(uint8_t configreg, uint8_t val)
{
	assert(configreg < 256);

	/* Check for various configurations that we don't handle yet */
	switch (configreg) {
	case 1:
		/* Don't reconfigure parallel port address */
		if ((val & 0x3) != 0x3) {
			UNIMPLEMENTED("SuperIO Conf Reg",
			              "Unsupported Parallel Port address 0x%x",
			              val & 0x3);
		}
		break;

	case 2:
		/* Don't reconfigure serial addresses */
		if ((val & 0x3) != 0x0) {
			UNIMPLEMENTED("SuperIO Conf Reg",
			              "Unsupported Serial Port 1 address 0x%x",
			              val & 0x3);
		}
		if (((val >> 4) & 0x3) != 0x1) {
			UNIMPLEMENTED("SuperIO Conf Reg",
			              "Unsupported Serial Port 2 address 0x%x",
			              (val >> 4) & 0x3);
		}
		break;

	case 5:
		/* Don't reconfigure FDC address */
		if ((val & 0x1) != 0x0) {
			UNIMPLEMENTED("SuperIO Conf Reg",
			              "Unsupported FDC address 0x%x",
			              val & 0x1);
		}
		/* Don't reconfigure IDE address */
		if ((val & 0x2) != 0x0) {
			UNIMPLEMENTED("SuperIO Conf Reg",
			              "Unsupported IDE address 0x%x",
			              val & 0x2);
		}
		break;
	}

	if (super_type == SuperIOType_FDC37C672) {
		switch (configreg) {
		case 0xb4:
		        gp_regs[0x0c] = val;
		        return;
		case 0xb5:
		        gp_regs[0x0d] = val;
		        return;
		}
	}

	if (super_type == SuperIOType_FDC37C665GT) {
		configregs665[configreg] = val;
	} else if (super_type == SuperIOType_FDC37C672) {
		configregs672[configreg] = val;
	}
}

/**
 * Set the initial state of the SuperIO chip.
 *
 * Called on emulated machine startup and reset.
 *
 * @param chosen_super_type SuperIO type used in the machine
 */
void
superio_reset(SuperIOType chosen_super_type)
{
	assert(chosen_super_type == SuperIOType_FDC37C665GT || chosen_super_type == SuperIOType_FDC37C672);

	super_type = chosen_super_type;
	configmode = SUPERIO_MODE_NORMAL;
	printstat = 0;

	/* Initial configuration register default values from the datasheet */
	configregs665[0x0] = 0x3b;
	configregs665[0x1] = 0x9f;
	configregs665[0x2] = 0xdc;
	configregs665[0x3] = 0x78;
	configregs665[0x4] = 0x00;
	configregs665[0x5] = 0x00;
	configregs665[0x6] = 0xff; /* Floppy Drive types for four floppy drives */
	configregs665[0x7] = 0x00;
	configregs665[0x8] = 0x00;
	configregs665[0x9] = 0x00;
	configregs665[0xa] = 0x00; /* FIFO threshhold for ECP parallel port */
	configregs665[0xb] = 0x00; /* Reserved (undefined when read) */
	configregs665[0xc] = 0x00; /* Reserved (undefined when read) */
	configregs665[0xd] = 0x65; /* Chip ID */
	configregs665[0xe] = 0x02; /* Chip revision level
	                           (see page 127 not 119 of datasheet) */
	configregs665[0xf] = 0x00; /* Test modes reserved */

	configregs672[0x03] = 0x03;
	configregs672[0x20] = 0x40;
	configregs672[0x24] = 0x04;
	configregs672[0x26] = 0xf0;
	configregs672[0x27] = 0x03;
	fdc_reset();
}

/**
 * Write to the IO space of the SuperIO chip.
 *
 * @param addr Address to write to
 * @param val  Value to write to 'addr'
 */
void
superio_write(uint32_t addr, uint32_t val)
{
        /* Convert memory-mapped address to IO port */
        addr = (addr >> 2) & 0x3ff;

	if (configmode != SUPERIO_MODE_CONFIGURATION) {
		if ((addr == 0x3f0) && (val == 0x55)) {
			/* Attempting to enter configuration mode */
			if (super_type == SuperIOType_FDC37C665GT) {
				if (configmode == SUPERIO_MODE_NORMAL) {
					configmode = SUPERIO_MODE_INTERMEDIATE;
				} else if (configmode == SUPERIO_MODE_INTERMEDIATE) {
					configmode = SUPERIO_MODE_CONFIGURATION;
				}
			} else if (super_type == SuperIOType_FDC37C672) {
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
				if (super_type == SuperIOType_FDC37C665GT) {
					configreg = val & 0xf;
				} else if (super_type == SuperIOType_FDC37C672) {
					configreg = val & 0xff;
				}
			}
		} else {
			/* Write to a configuration register */
			superio_config_reg_write(configreg, val);
		}
		return;
	}

	if (super_type == SuperIOType_FDC37C672) {
		/* Embedded Intel 8042 PS/2 keyboard controller */
		if (addr == 0x60) {
			i8042_data_write(val);
			return;
		} else if (addr == 0x64) {
			i8042_command_write(val);
			return;

		} else if (addr == 0xea) {
			/* GP Index Register */
			gp_index = val & 0xf;
			return;
		} else if (addr == 0xeb) {
			/* GP Data Register */
			gp_regs[gp_index] = val;
			return;
		}
	}

	if ((addr >= 0x1f0 && addr <= 0x1f7) || addr == 0x3f6) {
		/* IDE */
		if (super_type == SuperIOType_FDC37C665GT) {
			writeide(addr, val);
		}
	} else if ((addr >= 0x278) && (addr <= 0x27f)) {
		/* Parallel */
		if (addr == 0x27a) {
			if ((val & 0x10) || ((printstat ^ val) & 1 && !(val & 1))) {
				// rpclog("Printer interrupt %02X\n", iomd.irqa.mask);
				iomd.irqa.status |= IOMD_IRQA_PARALLEL;
				updateirqs();
			}

			printstat = val;
		} else {
			UNIMPLEMENTED("Parallel write",
			              "Unknown register 0x%03x", addr);
		}

	} else if ((addr >= 0x3f0) && (addr <= 0x3f7)) {
		/* Floppy */
		fdc_write(addr, val);

	} else if ((addr >= 0x3f8) && (addr <= 0x3ff)) {
		/* Serial Port 1 */
		if ((addr == 0x3f9) && (val & 2))
		{
			// printf("Serial transmit empty interrupt\n");
			iomd.fiq.status |= IOMD_FIQ_SERIAL;
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

/**
 * Read from the IO space of the SuperIO chip.
 *
 * @param addr   Address to read from
 * @return       Value of register at given address
 */
uint8_t
superio_read(uint32_t addr)
{
        /* Convert memory-mapped address to IO port */
        addr = (addr >> 2) & 0x3ff;

	if (configmode == SUPERIO_MODE_CONFIGURATION && addr == 0x3f1) {
		/* Read from configuration register */
		if (super_type == SuperIOType_FDC37C672) {
			/* Config registers contain a copy of the GP Data Registers */
		        switch (configreg) {
		        case 0xb4:
		                return gp_regs[0x0c];
			case 0xb5:
		                return gp_regs[0x0d];
		        case 0xb6:
		                return gp_regs[0x0e];
			case 0xb7:
		                return gp_regs[0x0f];
		        }
		}

		if (super_type == SuperIOType_FDC37C665GT) {
			return configregs665[configreg];
		} else if (super_type == SuperIOType_FDC37C672) {
			return configregs672[configreg];
		}
	}

	if (super_type == SuperIOType_FDC37C672) {
		/* Embedded Intel 8042 PS/2 keyboard controller */
		if (addr == 0x60) {
			return i8042_data_read();
		} else if (addr == 0x64) {
			return i8042_status_read();

		} else if (addr == 0xea) {
			/* GP Index Register */
			return gp_index;
		} else if (addr == 0xeb) {
			/* GP Data Register */
			return gp_regs[gp_index];
		}
	}

	if ((addr >= 0x1f0 && addr <= 0x1f7) || addr == 0x3f6) {
		/* IDE */
		if (super_type == SuperIOType_FDC37C665GT) {
			return readide(addr);
		}

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
		return fdc_read(addr);

	} else if ((addr >= 0x3f8) && (addr <= 0x3ff)) {
		/* Serial Port 1 */
		if (addr == 0x3fa) {
			iomd.fiq.status &= ~IOMD_FIQ_SERIAL;
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
