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

/* IOMD emulation */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "rpcemu.h"
#include "vidc20.h"
#include "keyboard.h"
#include "sound.h"
#include "iomd.h"
#include "arm.h"
#include "cmos.h"
#include "podules.h"

/* References -
   Acorn Risc PC - Technical Reference Manual
   ARM7500 Data Sheet
   ARM7500FE Data Sheet
*/

/* Defines for IOMD registers */
#define IOMD_0x000_IOCR     0x000 /* I/O control */
#define IOMD_0x004_KBDDAT   0x004 /* Keyboard data */
#define IOMD_0x008_KBDCR    0x008 /* Keyboard control */

#define IOMD_0x00C_IOLINES  0x00C /* General Purpose I/O lines (ARM7500/FE) */

#define IOMD_0x010_IRQSTA   0x010 /* IRQA status */
#define IOMD_0x014_IRQRQA   0x014 /* IRQA request/clear */
#define IOMD_0x018_IRQMSKA  0x018 /* IRQA mask */

#define IOMD_0x01C_SUSMODE  0x01C /* Enter SUSPEND mode (ARM7500/FE) */

#define IOMD_0x020_IRQSTB   0x020 /* IRQB status */
#define IOMD_0x024_IRQRQB   0x024 /* IRQB request/clear */
#define IOMD_0x028_IRQMSKB  0x028 /* IRQB mask */

//#define IOMD_0x02C_STOPMODE 0x02C /* Enter STOP mode (ARM7500/FE) */

#define IOMD_0x030_FIQST    0x030 /* FIQ status */
#define IOMD_0x034_FIQRQ    0x034 /* FIQ request */
#define IOMD_0x038_FIQMSK   0x038 /* FIQ mask */

#define IOMD_0x03C_CLKCTL   0x03C /* Clock divider control (ARM7500/FE) */

#define IOMD_0x040_T0LOW    0x040 /* Timer 0 low bits */
#define IOMD_0x044_T0HIGH   0x044 /* Timer 0 high bits */
#define IOMD_0x048_T0GO     0x048 /* Timer 0 Go command */
#define IOMD_0x04C_T0LAT    0x04C /* Timer 0 Latch command */

#define IOMD_0x050_T1LOW    0x050 /* Timer 1 low bits */
#define IOMD_0x054_T1HIGH   0x054 /* Timer 1 high bits */
#define IOMD_0x058_T1GO     0x058 /* Timer 1 Go command */
#define IOMD_0x05C_T1LAT    0x05C /* Timer 1 Latch command */

#define IOMD_0x060_IRQSTC   0x060 /* IRQC status        (ARM7500/FE) */
#define IOMD_0x064_IRQRQC   0x064 /* IRQC request/clear (ARM7500/FE) */
#define IOMD_0x068_IRQMSKC  0x068 /* IRQC mask          (ARM7500/FE) */

#define IOMD_0x06C_VIDIMUX  0x06C /* LCD and IIS control bits (ARM7500/FE) */

#define IOMD_0x070_IRQSTD   0x070 /* IRQD status        (ARM7500/FE) */
#define IOMD_0x074_IRQRQD   0x074 /* IRQD request/clear (ARM7500/FE) */
#define IOMD_0x078_IRQMSKD  0x078 /* IRQD mask          (ARM7500/FE) */

#define IOMD_0x080_ROMCR0   0x080 /* ROM control bank 0 */
#define IOMD_0x084_ROMCR1   0x084 /* ROM control bank 1 */
#define IOMD_0x088_DRAMCR   0x088 /* DRAM control (IOMD) */
#define IOMD_0x08C_VREFCR   0x08C /* IOMD VRAM control, IOMD/7500/FE DRAM refresh speed */

#define IOMD_0x090_FSIZE    0x090 /* Flyback line size (IOMD) */
#define IOMD_0x094_ID0      0x094 /* Chip ID no. low byte */
#define IOMD_0x098_ID1      0x098 /* Chip ID no. high byte */
#define IOMD_0x09C_VERSION  0x09C /* Chip version number */

#define IOMD_0x0A0_MOUSEX   0x0A0 /* Mouse X position (Quadrature - IOMD) */
#define IOMD_0x0A4_MOUSEY   0x0A4 /* Mouse Y position (Quadrature - IMOD) */
#define IOMD_0x0A8_MSEDAT   0x0A8 /* Mouse data    (PS/2 - ARM7500/FE) */
#define IOMD_0x0AC_MSECR    0x0AC /* Mouse control (PS/2 - ARM7500/FE) */

//#define IOMD_0x0C0_DMATCR   0x0C0 /* DACK timing control (IOMD) */
#define IOMD_0x0C4_IOTCR    0x0C4 /* I/O timing control */
#define IOMD_0x0C8_ECTCR    0x0C8 /* Expansion card timing */

/* This register has two different names depending on chip */
#define IOMD_0x0CC_DMAEXT   0x0CC /* DMA external control     (IOMD) */
//#define IOMD_0x0CC_ASTCR    0x0CC /* Async I/O timing control (ARM7500/FE) */

#define IOMD_0x0D0_DRAMWID  0x0D0 /* DRAM width control      (ARM7500/FE) */
#define IOMD_0x0D4_SELFREF  0x0D4 /* Force CAS/RAS lines low (ARM7500/FE) */


#define IOMD_0x0E0_ATODICR  0x0E0 /* A to D interrupt control (ARM7500/FE) */
//#define IOMD_0x0E4_ATODSR   0x0E4 /* A to D status            (ARM7500/FE) */
//#define IOMD_0x0E8_ATODCC   0x0E8 /* A to D convertor control (ARM7500/FE) */
//#define IOMD_0x0EC_ATODCNT1 0x0EC /* A to D counter 1         (ARM7500/FE) */
//#define IOMD_0x0F0_ATODCNT2 0x0F0 /* A to D counter 2         (ARM7500/FE) */
//#define IOMD_0x0F4_ATODCNT3 0x0F4 /* A to D counter 3         (ARM7500/FE) */
//#define IOMD_0x0F8_ATODCNT4 0x0F8 /* A to D counter 4         (ARM7500/FE) */

//#define IOMD_0x100_IO0CURA  0x100 /* I/O DMA 0 CurA    (IOMD) */
//#define IOMD_0x104_IO0ENDA  0x104 /* I/O DMA 0 EndA    (IOMD) */
//#define IOMD_0x108_IO0CURB  0x108 /* I/O DMA 0 CurB    (IOMD) */
//#define IOMD_0x10C_IO0ENDB  0x10C /* I/O DMA 0 EndB    (IOMD) */
//#define IOMD_0x110_IO0CR    0x110 /* I/O DMA 0 Control (IOMD) */
//#define IOMD_0x114_IO0ST    0x114 /* I/O DMA 0 Status  (IOMD) */

//#define IOMD_0x120_IO1CURA  0x120 /* I/O DMA 1 CurA    (IOMD) */
//#define IOMD_0x124_IO1ENDA  0x124 /* I/O DMA 1 EndA    (IOMD) */
//#define IOMD_0x128_IO1CURB  0x128 /* I/O DMA 1 CurB    (IOMD) */
//#define IOMD_0x12C_IO1ENDB  0x12C /* I/O DMA 1 EndB    (IOMD) */
//#define IOMD_0x130_IO1CR    0x130 /* I/O DMA 1 Control (IOMD) */
//#define IOMD_0x134_IO1ST    0x134 /* I/O DMA 1 Status  (IOMD) */

//#define IOMD_0x140_IO2CURA  0x140 /* I/O DMA 2 CurA    (IOMD) */
//#define IOMD_0x144_IO2ENDA  0x144 /* I/O DMA 2 EndA    (IOMD) */
//#define IOMD_0x148_IO2CURB  0x148 /* I/O DMA 2 CurB    (IOMD) */
//#define IOMD_0x14C_IO2ENDB  0x14C /* I/O DMA 2 EndB    (IOMD) */
//#define IOMD_0x150_IO2CR    0x150 /* I/O DMA 2 Control (IOMD) */
//#define IOMD_0x154_IO2ST    0x154 /* I/O DMA 2 Status  (IOMD) */

//#define IOMD_0x160_IO3CURA  0x160 /* I/O DMA 3 CurA    (IOMD) */
//#define IOMD_0x164_IO3ENDA  0x164 /* I/O DMA 3 EndA    (IOMD) */
//#define IOMD_0x168_IO3CURB  0x168 /* I/O DMA 3 CurB    (IOMD) */
//#define IOMD_0x16C_IO3ENDB  0x16C /* I/O DMA 3 EndB    (IOMD) */
//#define IOMD_0x170_IO3CR    0x170 /* I/O DMA 3 Control (IOMD) */
//#define IOMD_0x174_IO3ST    0x174 /* I/O DMA 3 Status  (IOMD) */

#define IOMD_0x180_SD0CURA  0x180 /* Sound DMA 0 CurA */
#define IOMD_0x184_SD0ENDA  0x184 /* Sound DMA 0 EndA */
#define IOMD_0x188_SD0CURB  0x188 /* Sound DMA 0 CurB */
#define IOMD_0x18C_SD0ENDB  0x18C /* Sound DMA 0 EndB */
#define IOMD_0x190_SD0CR    0x190 /* Sound DMA 0 Control */
#define IOMD_0x194_SD0ST    0x194 /* Sound DMA 0 Status */

//#define IOMD_0x1A0_SD1CURA  0x1A0 /* Sound DMA 1 CurA    (IOMD) */
//#define IOMD_0x1A4_SD1ENDA  0x1A4 /* Sound DMA 1 EndA    (IOMD) */
//#define IOMD_0x1A8_SD1CURB  0x1A8 /* Sound DMA 1 CurB    (IOMD) */
//#define IOMD_0x1AC_SD1ENDB  0x1AC /* Sound DMA 1 EndB    (IOMD) */
//#define IOMD_0x1B0_SD1CR    0x1B0 /* Sound DMA 1 Control (IOMD) */
//#define IOMD_0x1B4_SD1ST    0x1B4 /* Sound DMA 1 Status  (IOMD) */

#define IOMD_0x1C0_CURSCUR  0x1C0 /* Cursor DMA Current */
#define IOMD_0x1C4_CURSINIT 0x1C4 /* Cursor DMA Init */
//#define IOMD_0x1C8_VIDCURB  0x1C8 /* Duplex LCD Current B (ARM7500/FE) */

#define IOMD_0x1D0_VIDCUR   0x1D0 /* Video DMA Current */
#define IOMD_0x1D4_VIDEND   0x1D4 /* Video DMA End */
#define IOMD_0x1D8_VIDSTART 0x1D8 /* Video DMA Start */ 
#define IOMD_0x1DC_VIDINIT  0x1DC /* Video DMA Init */
#define IOMD_0x1E0_VIDCR    0x1E0 /* Video DMA Control */
//#define IOMD_0x1E8_VIDINITB 0x1E8 /* Duplex LCD Init B (ARM7500/FE) */

#define IOMD_0x1F0_DMAST    0x1F0 /* DMA interupt status */
#define IOMD_0x1F4_DMARQ    0x1F4 /* DMA interupt request */
#define IOMD_0x1F8_DMAMSK   0x1F8 /* DMA interupt mask */

#define IDE_BIT (1 << 0x03)

struct iomd iomd;

uint32_t cinit = 0; /**< Cursor DMA Init */

/**
 * Structure to hold information about the variants of IOMD that are supported.
 */
typedef struct {
	uint8_t id_low;     /**< Low byte of ID */
	uint8_t id_high;    /**< High byte of ID */
	uint8_t id_version; /**< Byte of chip version */
} IOMDVariant;

/**
 * Details of the variants of IOMD that are supported.
 *
 * This array must be kept in the same order as IOMDType in iomd.h
 */
static const IOMDVariant iomds[] = {
	{ 0xe7, 0xd4, 0 }, /* IOMD */
	{ 0x98, 0x5b, 0 }, /* ARM7500 */
	{ 0x7c, 0xaa, 0 }, /* ARM7500FE */
	{ 0xe8, 0xd5, 1 }, /* IOMD2 */
};

static IOMDType iomd_type; /**< The current type of IOMD we're emulating */

static int sndon = 0;
static int flyback=0;

void
updateirqs(void)
{
	if ((iomd.irqa.status & iomd.irqa.mask) ||
	    (iomd.irqb.status & iomd.irqb.mask) ||
	    (iomd.irqd.status & iomd.irqd.mask) ||
	    (iomd.irqdma.status & iomd.irqdma.mask))
	{
		arm.event |= 1;
	} else {
		arm.event &= ~1u;
	}
	if (iomd.fiq.status & iomd.fiq.mask) {
		arm.event |= 2;
	} else {
		arm.event &= ~2u;
	}
}

/**
 * Handle the regularly ticking interrupts, the two
 * IOMD timers, the sound interrupt and podule
 * interrupts
 *
 * Called (theoretically) 500 times a second IMPROVE.
 */
void gentimerirq(void)
{
        iomd.t0.counter -= 4000; /* 4000 * 500Hz = 2MHz (the IO clock speed) */
        while (iomd.t0.counter < 0 && iomd.t0.in_latch)
        {
                iomd.t0.counter += iomd.t0.in_latch;
                iomd.irqa.status |= IOMD_IRQA_TIMER_0;
                updateirqs();
        }

        iomd.t1.counter -= 4000;
        while (iomd.t1.counter < 0 && iomd.t1.in_latch)
        {
                iomd.t1.counter += iomd.t1.in_latch;
                iomd.irqa.status |= IOMD_IRQA_TIMER_1;
                updateirqs();
        }

        if (soundinited && sndon)
        {
                soundcount -= 4000;
                if (soundcount<0)
                {
                        sound_irq_update();
                        soundcount+=soundlatch;
//                        rpclog("soundcount now %i %i\n",soundcount,soundlatch);
                }
        }

        /* Update Podule interrupts */
        runpoduletimers(2); /* 2ms * 500 = 1 sec */
}

/**
 * Handle writes to the IOMD memory space
 *
 * @param addr Absolute memory address in IOMD space
 * @param val Value to write to that addresses' function
 */
void
iomd_write(uint32_t addr, uint32_t val)
{
        static int readinc = 0;

	uint32_t reg;

	if (iomd_type == IOMDType_IOMD2) {
		reg = addr & 0x3fc; /* IOMD2 has 256 registers*/
	} else {
		reg = addr & 0x1fc; /* IOMD1 variants have 128 registers */
	}

        switch (reg)
        {
        case IOMD_0x000_IOCR: /* I/O control */
                cmosi2cchange((val >> 1) & 1, val & 1);
                iomd.ctrl=val;
                return;

        case IOMD_0x004_KBDDAT: /* Keyboard data */
		if (iomd_type != IOMDType_IOMD2) {
			keyboard_data_write(val);
		}
                return;
        case IOMD_0x008_KBDCR: /* Keyboard control */
		if (iomd_type != IOMDType_IOMD2) {
			keyboard_control_write(val & 8);
		}
                return;

	case IOMD_0x00C_IOLINES: /* General Purpose I/O lines (ARM7500/FE) */
		if (iomd_type == IOMDType_ARM7500 || iomd_type == IOMDType_ARM7500FE) {
			/* Connected to pin/bit 0 is the Monitor Id bit */
			/* All other pin/bit connections are currently unknown */
			if ((val & 0xfe) != 0) {
				UNIMPLEMENTED("IOMD ARM7500 IOLINES",
				              "Write to enable unknown IO device 0x%x", val);
			}
		}
		return;

        case IOMD_0x014_IRQRQA: /* IRQA request/clear */
                iomd.irqa.status &= ~val;
                iomd.irqa.status |= IOMD_IRQA_FORCE_BIT; /* Bit 7 always active on IRQA */
                updateirqs();
                return;
        case IOMD_0x018_IRQMSKA: /* IRQA mask */
                iomd.irqa.mask = val;
                updateirqs();
                return;

	case IOMD_0x01C_SUSMODE: /* Enter SUSPEND mode (ARM7500/FE) */
		if (iomd_type == IOMDType_ARM7500 || iomd_type == IOMDType_ARM7500FE) {
			UNIMPLEMENTED("IOMD Register write",
			              "Call to enter SUSPEND mode 0x%x", val);
			iomd.susmode = val;
		}
		return;

        case IOMD_0x028_IRQMSKB: /* IRQB mask */
                iomd.irqb.mask = val;
                updateirqs();
                return;

        case IOMD_0x038_FIQMSK: /* FIQ mask */
                iomd.fiq.mask = val;
                return;

	case IOMD_0x03C_CLKCTL: /* Clock divider control (ARM7500/FE) */
		if (iomd_type == IOMDType_ARM7500 || iomd_type == IOMDType_ARM7500FE) {
			iomd.clkctrl = val;
		}
		return;

        case IOMD_0x040_T0LOW: /* Timer 0 low bits */
                iomd.t0.in_latch = (iomd.t0.in_latch & 0xff00) | (val & 0xff);
                break;
        case IOMD_0x044_T0HIGH: /* Timer 0 high bits */
                iomd.t0.in_latch = (iomd.t0.in_latch & 0xff) | ((val & 0xff) << 8);
                break;
        case IOMD_0x048_T0GO: /* Timer 0 Go command */
                iomd.t0.counter = iomd.t0.in_latch - 1;
                break;
        case IOMD_0x04C_T0LAT: /* Timer 0 Latch command */
                readinc ^= 1;
                iomd.t0.out_latch = iomd.t0.counter;
                if (readinc) {
                        iomd.t0.counter--;
                        if (iomd.t0.counter < 0) {
                                iomd.t0.counter += iomd.t0.in_latch;
                        }
                }
                break;

        case IOMD_0x050_T1LOW: /* Timer 1 low bits */
                iomd.t1.in_latch = (iomd.t1.in_latch & 0xff00) | (val & 0xff);
                break;
        case IOMD_0x054_T1HIGH: /* Timer 1 high bits */
                iomd.t1.in_latch = (iomd.t1.in_latch & 0xff) | ((val & 0xff) << 8);
                break;
        case IOMD_0x058_T1GO: /* Timer 1 Go command */
                iomd.t1.counter = iomd.t1.in_latch - 1;
                break;
        case IOMD_0x05C_T1LAT: /* Timer 1 Latch command */
                readinc ^= 1;
                iomd.t1.out_latch = iomd.t1.counter;
                if (readinc) {
                        iomd.t1.counter--;
                        if (iomd.t1.counter < 0) {
                                iomd.t1.counter += iomd.t1.in_latch;
                        }
                }
                break;

        case IOMD_0x068_IRQMSKC: /* IRQC mask (ARM7500/FE) */
                iomd.irqc.mask = val;
                return;

	case IOMD_0x06C_VIDIMUX: /* LCD and IIS control bits (ARM7500/FE) */
		if (iomd_type == IOMDType_ARM7500 || iomd_type == IOMDType_ARM7500FE) {
			iomd.vidimux = val;
		}
		return;

        case IOMD_0x074_IRQRQD: /* IRQD request/clear (ARM7500/FE) */
                return;
        case IOMD_0x078_IRQMSKD: /* IRQD mask (ARM7500/FE) */
                iomd.irqd.mask = val;
                return;

        case IOMD_0x080_ROMCR0: /* ROM control bank 0 */
                iomd.romcr0=val;
                return;
        case IOMD_0x084_ROMCR1: /* ROM control bank 1 */
                iomd.romcr1=val;
                return;

        case IOMD_0x088_DRAMCR: /* DRAM control (IOMD) */
                /* Control the DRAM row address options, no need to implement */
                return;

	case IOMD_0x08C_VREFCR: /* IOMD VRAM control, IOMD/7500/FE DRAM refresh speed */
		iomd.refcr = val;
		return;

        case IOMD_0x090_FSIZE: /* Flyback line size (IOMD) */
                return;

	case IOMD_0x0A0_MOUSEX: /* Mouse X position (Quadrature - IOMD) */
		if (iomd_type == IOMDType_IOMD) {
			iomd.mousex = val;
		}
		return;
	case IOMD_0x0A4_MOUSEY: /* Mouse Y position (Quadrature - IOMD) */
		if (iomd_type == IOMDType_IOMD) {
			iomd.mousey = val;
		}
		return;

	case IOMD_0x0A8_MSEDAT: /* Mouse data (PS/2 - ARM7500/FE) */
		if (iomd_type == IOMDType_ARM7500 || iomd_type == IOMDType_ARM7500FE) {
			mouse_data_write(val);
		}
		return;
	case IOMD_0x0AC_MSECR: /* Mouse control (PS/2 - ARM7500/FE) */
		if (iomd_type == IOMDType_ARM7500 || iomd_type == IOMDType_ARM7500FE) {
			mouse_control_write(val);
		}
		return;

	case IOMD_0x0C4_IOTCR: /* I/O timing control */
		iomd.iotcr = val;
		return;

	case IOMD_0x0C8_ECTCR: /* I/O expansion timing control */
		iomd.ectcr = val;
		return;

	case IOMD_0x0CC_DMAEXT: /* DMA external control (IOMD) */
		if (iomd_type == IOMDType_IOMD) {
			iomd.dmaext = val;
			return;
		}
		/* Register 0xcc has a different meaning in ARM7500 */
		UNIMPLEMENTED("IOMD Register write",
		              "Write to register 0xcc in ARM7500/FE mode val 0x%x", val);
		return;

	case IOMD_0x0D0_DRAMWID: /* DRAM width control (ARM7500/FE) */
		if (iomd_type == IOMDType_ARM7500 || iomd_type == IOMDType_ARM7500FE) {
			iomd.dramcr = val;
		}
		return;

	case IOMD_0x0D4_SELFREF: /* Force CAS/RAS lines low (ARM7500/FE) */
		if (iomd_type == IOMDType_ARM7500 || iomd_type == IOMDType_ARM7500FE) {
			iomd.selfref = val;
		}
		return;

        case IOMD_0x0E0_ATODICR: /* A to D interrupt control (ARM7500/FE) */
                /* We do not support enabling any of the interrupts */
                if (val != 0) {
                        UNIMPLEMENTED("IOMD ATODICR write",
                                      "Unsupported A to D control write 0x%x",
                                      val);
                }
                return;

        case IOMD_0x180_SD0CURA: /* Sound DMA 0 CurA */
        case IOMD_0x184_SD0ENDA: /* Sound DMA 0 EndA */
        case IOMD_0x188_SD0CURB: /* Sound DMA 0 CurB */
        case IOMD_0x18C_SD0ENDB: /* Sound DMA 0 EndB */
                // rpclog("Write sound DMA %08X %02X\n",addr,val);
                iomd.sndstat &= IOMD_DMA_STATUS_BUFFER;
                iomd.irqdma.status &= ~IOMD_IRQDMA_SOUND_0;
                updateirqs();
                soundaddr[(addr>>2)&3]=val;
                // rpclog("Buffer A start %08X len %08X\nBuffer B start %08X len %08X\n",
                // soundaddr[0],(soundaddr[1]-soundaddr[0])&0xFFC,soundaddr[2],
                // (soundaddr[3]-soundaddr[2])&0xFFC);
                return;
        case IOMD_0x190_SD0CR: /* Sound DMA 0 Control */
                // rpclog("Write sound CTRL %08X %02X\n",addr,val);
                if (val&0x80)
                {
                        iomd.sndstat = IOMD_DMA_STATUS_INTERRUPT |
                                       IOMD_DMA_STATUS_OVERRUN;
                        soundinited=1;
                        iomd.irqdma.status |= IOMD_IRQDMA_SOUND_0;
                        updateirqs();
                }
                sndon=val&0x20;
                return;

        case IOMD_0x1C0_CURSCUR: /* Cursor DMA Current */
                return;
        case IOMD_0x1C4_CURSINIT: /* Cursor DMA Init */
                // if (cinit!=val) curchange=1;
                cinit=val;
                // curchange=1;
                return;

	case IOMD_0x1D0_VIDCUR: /* Video DMA Current */
		iomd.vidcur = val & 0x1ffffff0; /* Mask out bits ignored by IOMD */
		return;
	case IOMD_0x1D4_VIDEND: /* Video DMA End */
		iomd.vidend = val & 0x1ffffff0; /* Mask out bits ignored by IOMD */
		resetbuffer();
		return;
	case IOMD_0x1D8_VIDSTART: /* Video DMA Start */
		iomd.vidstart = val & 0x1ffffff0; /* Mask out bits ignored by IOMD */
		if (!((iomd.vidstart >= 0x02000000 && iomd.vidstart < 0x03000000)
		   || (iomd.vidstart >= 0x10000000 && iomd.vidstart < 0x14000000)))
		{
			UNIMPLEMENTED("Video RAM location", "Video memory not in VRAM or RAM SIMM 0 bank 0 (0x%08x)", iomd.vidstart);
		}
		resetbuffer();
		return;
	case IOMD_0x1DC_VIDINIT: /* Video DMA Init */
		iomd.vidinit = val & 0xfffffff0; /* Ignore bottom 4 bits (quad word aligned) */
		resetbuffer();
		return;
	case IOMD_0x1E0_VIDCR: /* Video DMA Control */
		iomd.vidcr = val;
		resetbuffer();
		return;

        case IOMD_0x1F0_DMAST: /* DMA interupt status */
        case IOMD_0x1F4_DMARQ: /* DMA interupt request */
                return;
        case IOMD_0x1F8_DMAMSK: /* DMA interupt mask */
                iomd.irqdma.mask = val;
                return;

	case 0x314:
		if (val & (1 << 0x06))
			iomd.irqa.status &= ~IOMD_IRQA_FLYBACK;
		if (val & (1 << 0x10))
			iomd.irqa.status &= ~IOMD_IRQA_TIMER_0;
		if (val & (1 << 0x0a))
			iomd.irqb.status &= ~IOMD2_IRQB_SUPERIO_SMI;
		if (val & IDE_BIT)
			iomd.irqb.status &= ~IOMD_IRQB_IDE;

		iomd.irqa.status |= IOMD_IRQA_FORCE_BIT; /* Bit 7 always active on IRQA */
		updateirqs();
		return;
	case 0x318:
		if (val & (1 << 0x06))
			iomd.irqa.mask |= IOMD_IRQA_FLYBACK;
		else
			iomd.irqa.mask &= ~IOMD_IRQA_FLYBACK;
		if (val & (1 << 0x10))
			iomd.irqa.mask |= IOMD_IRQA_TIMER_0;
		else
			iomd.irqa.mask &= ~IOMD_IRQA_TIMER_0;
		if (val & (1 << 0x0a))
			iomd.irqb.mask |= IOMD2_IRQB_SUPERIO_SMI;
		else
			iomd.irqb.mask &= ~IOMD2_IRQB_SUPERIO_SMI;
		if (val & IDE_BIT)
			iomd.irqb.mask |= IOMD_IRQB_IDE;
		else
			iomd.irqb.mask &= ~IOMD_IRQB_IDE;
		updateirqs();
		break;

        default:
                UNIMPLEMENTED("IOMD Register write",
                              "Unknown register 0x%x val 0x%x", reg, val);
                return;
        }
}

/**
 * Handle reads from IOMD memory space
 *
 * @param addr Absolute memory address in IOMD space
 * @return Value associated with that memory address function
 */
uint32_t
iomd_read(uint32_t addr)
{
	uint32_t reg;

	if (iomd_type == IOMDType_IOMD2) {
		reg = addr & 0x3fc; /* IOMD2 has 256 registers*/
	} else {
		reg = addr & 0x1fc; /* IOMD1 variants have 128 registers */
	}

        switch (reg)
        {
        case IOMD_0x000_IOCR: /* I/O control */
                return ((i2cclock)?2:0)|((i2cdata)?1:0)|(iomd.ctrl&0x7C)|4|((flyback)?0x80:0);
        case IOMD_0x004_KBDDAT: /* Keyboard data */
		if (iomd_type != IOMDType_IOMD2) {
			return keyboard_data_read();
		} else {
			return 0;
		}
        case IOMD_0x008_KBDCR: /* Keyboard control */
		if (iomd_type != IOMDType_IOMD2) {
			return keyboard_status_read();
		} else {
			return 0;
		}

	case IOMD_0x00C_IOLINES: /* General Purpose I/O lines (ARM7500/FE) */
		if (iomd_type == IOMDType_ARM7500 || iomd_type == IOMDType_ARM7500FE) {
			/* Connected to pin/bit 0 is the Monitor Id bit, 0 for VGA, 1 for TV res */
			/* All other pin/bit connections are currently unknown */
			return 0;
		}
		return 0;

        case IOMD_0x010_IRQSTA: /* IRQA status */
                return iomd.irqa.status;
        case IOMD_0x014_IRQRQA: /* IRQA request/clear */
                return iomd.irqa.status & iomd.irqa.mask;
        case IOMD_0x018_IRQMSKA: /* IRQA mask */
                return iomd.irqa.mask;

	case IOMD_0x01C_SUSMODE: /* Enter SUSPEND mode (ARM7500/FE) */
		if (iomd_type == IOMDType_ARM7500 || iomd_type == IOMDType_ARM7500FE) {
			return iomd.susmode;
		}
		return 0;

        case IOMD_0x020_IRQSTB: /* IRQB status */
                return iomd.irqb.status;
        case IOMD_0x024_IRQRQB: /* IRQB request/clear */
                return iomd.irqb.status & iomd.irqb.mask;
        case IOMD_0x028_IRQMSKB: /* IRQB mask */
                return iomd.irqb.mask;

        case IOMD_0x030_FIQST: /* FIQ status */
                return iomd.fiq.status;
        case IOMD_0x034_FIQRQ: /* FIQ request */
                return iomd.fiq.status & iomd.fiq.mask;
        case IOMD_0x038_FIQMSK: /* FIQ mask */
                return iomd.fiq.mask;

	case IOMD_0x03C_CLKCTL: /* Clock divider control (ARM7500/FE) */
		if (iomd_type == IOMDType_ARM7500 || iomd_type == IOMDType_ARM7500FE) {
			return iomd.clkctrl;
		}
		return 0;

        case IOMD_0x040_T0LOW: /* Timer 0 low bits */
                return iomd.t0.out_latch & 0xff;
        case IOMD_0x044_T0HIGH: /* Timer 0 high bits */
                return iomd.t0.out_latch >> 8;

        case IOMD_0x050_T1LOW: /* Timer 1 low bits */
                return iomd.t1.out_latch & 0xff;
        case IOMD_0x054_T1HIGH: /* Timer 1 high bits */
                return iomd.t1.out_latch >> 8;

        case IOMD_0x060_IRQSTC: /* IRQC status (ARM7500/FE) */
                return iomd.irqc.status;
        case IOMD_0x064_IRQRQC: /* IRQC request/clear (ARM7500/FE) */
                return iomd.irqc.status & iomd.irqc.mask;
        case IOMD_0x068_IRQMSKC: /* IRQC mask (ARM7500/FE) */
                return iomd.irqc.mask;

	case IOMD_0x06C_VIDIMUX: /* LCD and IIS control bits (ARM7500/FE) */
		if (iomd_type == IOMDType_ARM7500 || iomd_type == IOMDType_ARM7500FE) {
			return iomd.vidimux;
		}
		return 0;

        case IOMD_0x070_IRQSTD: /* IRQD status (ARM7500/FE) */
                return iomd.irqd.status;
        case IOMD_0x074_IRQRQD: /* IRQD request/clear (ARM7500/FE) */
                return iomd.irqd.status & iomd.irqd.mask;
        case IOMD_0x078_IRQMSKD: /* IRQD mask (ARM7500/FE) */
                return iomd.irqd.mask;

        case IOMD_0x080_ROMCR0: /* ROM control bank 0 */
                return iomd.romcr0;
        case IOMD_0x084_ROMCR1: /* ROM control bank 1 */
                return iomd.romcr1;
	case IOMD_0x08C_VREFCR: /* IOMD VRAM control, IOMD/7500/FE DRAM refresh speed */
		return iomd.refcr;

        case IOMD_0x094_ID0: /* Chip ID no. low byte */
		return iomds[iomd_type].id_low;
        case IOMD_0x098_ID1: /* Chip ID no. high byte */
		return iomds[iomd_type].id_high;
        case IOMD_0x09C_VERSION: /* Chip version number */
		return iomds[iomd_type].id_version;

	case IOMD_0x0A0_MOUSEX: /* Mouse X position (Quadrature - IOMD) */
		if (iomd_type == IOMDType_IOMD) {
			return iomd.mousex;
		}
		return 0;
	case IOMD_0x0A4_MOUSEY: /* Mouse Y position (Quadrature - IOMD) */
		if (iomd_type == IOMDType_IOMD) {
			return iomd.mousey;
		}
		return 0;

	case IOMD_0x0A8_MSEDAT: /* Mouse data (PS/2 - ARM7500/FE) */
		if (iomd_type == IOMDType_ARM7500 || iomd_type == IOMDType_ARM7500FE) {
			return mouse_data_read();
		}
		return 0;
	case IOMD_0x0AC_MSECR: /* Mouse control (PS/2 - ARM7500/FE) */
		if (iomd_type == IOMDType_ARM7500 || iomd_type == IOMDType_ARM7500FE) {
			return mouse_status_read();
		}
		return 0;

	case IOMD_0x0C4_IOTCR: /* I/O timing control */
		return iomd.iotcr;
	case IOMD_0x0C8_ECTCR: /* I/O expansion timing control */
		return iomd.ectcr;

	case IOMD_0x0CC_DMAEXT: /* DMA external control     (IOMD) */
		if (iomd_type != IOMDType_ARM7500 && iomd_type != IOMDType_ARM7500FE) {
			return iomd.dmaext;
		}
		/* 0xcc has a different meaning in ARM7500 */
		UNIMPLEMENTED("IOMD Register read",
		              "Read from register 0xcc in ARM7500/FE mode");
		return 0;

	case IOMD_0x0D0_DRAMWID: /* DRAM width control (ARM7500/FE) */
		if (iomd_type == IOMDType_ARM7500 || iomd_type == IOMDType_ARM7500FE) {
			return iomd.dramcr;
		}
		return 0;

	case IOMD_0x0D4_SELFREF: /* Force CAS/RAS lines low (ARM7500/FE) */
		if (iomd_type == IOMDType_ARM7500 || iomd_type == IOMDType_ARM7500FE) {
			return iomd.selfref;
		}
		return 0;

        case IOMD_0x180_SD0CURA: /* Sound DMA 0 CurA */
        case IOMD_0x184_SD0ENDA: /* Sound DMA 0 EndA */
        case IOMD_0x188_SD0CURB: /* Sound DMA 0 CurB */
        case IOMD_0x18C_SD0ENDB: /* Sound DMA 0 EndB */
                return 0;
        case IOMD_0x194_SD0ST: /* Sound DMA 0 Status */
                return iomd.sndstat;

	case IOMD_0x1D0_VIDCUR: /* Video DMA Current */
		return iomd.vidcur;
	case IOMD_0x1D4_VIDEND: /* Video DMA End */
		return iomd.vidend;
	case IOMD_0x1D8_VIDSTART: /* Video DMA Start */
		return iomd.vidstart;
	case IOMD_0x1DC_VIDINIT: /* Video DMA Init */
		return iomd.vidinit;
	case IOMD_0x1E0_VIDCR: /* Video DMA Control */
		return iomd.vidcr | 0x50;

        case IOMD_0x1F0_DMAST: /* DMA interupt status */
                return iomd.irqdma.status;
        case IOMD_0x1F4_DMARQ: /* DMA interupt request */
                return iomd.irqdma.status & iomd.irqdma.mask;
        case IOMD_0x1F8_DMAMSK: /* DMA interupt mask */
                return iomd.irqdma.mask;

	case 0x314:
		{
			uint32_t ret = 0;
			if ((iomd.irqa.status & iomd.irqa.mask) & IOMD_IRQA_FLYBACK)
				ret |= (1 << 0x06);
			if ((iomd.irqa.status & iomd.irqa.mask) & IOMD_IRQA_TIMER_0)
				ret |= (1 << 0x10);
			if ((iomd.irqb.status & iomd.irqb.mask) & IOMD2_IRQB_SUPERIO_SMI)
				ret |= (1 << 0x0a);
			if ((iomd.irqb.status & iomd.irqb.mask) & IOMD_IRQB_IDE)
				ret |= IDE_BIT;
			return ret;
		}

	case 0x318:
		{
			uint32_t ret = 0;
			if (iomd.irqa.mask & IOMD_IRQA_FLYBACK)
				ret |= (1 << 0x06);
			if (iomd.irqa.mask & IOMD_IRQA_TIMER_0)
				ret |= (1 << 0x10);
			if (iomd.irqb.mask & IOMD2_IRQB_SUPERIO_SMI)
				ret |= (1 << 0x0a);
			if (iomd.irqb.mask & IOMD_IRQB_IDE)
				ret |= IDE_BIT;
			return ret;
		}

        default:
                UNIMPLEMENTED("IOMD Register read",
                              "Unknown register 0x%x", reg);

        }
        return 0;
}

/**
 * Read the state of the Quadrature (bus) mouse
 * found on the RPC.
 *
 * bit 0 is the monitor ID bit
 * bit 4 is the right mouse button (active low)
 * bit 5 is the middle mouse button (active low)
 * bit 6 is the left mouse button (active low)
 * bit 7 is cmos reset bit (active high) TODO not sure of interpretation
 * bit 8 is 16 bit sound (active low) else 8 bit (high)
 */
uint32_t
iomd_mouse_buttons_read(void)
{
        uint32_t temp = 0;

	int mouse_buttons = mouse_buttons_get();

	/* Left */
	if (mouse_buttons & 1) {
		temp |= 0x40; // bit 6
	}
	/* Middle */
	if (mouse_buttons & 4) {
#ifdef __APPLE__
        temp |= 0x20;
#else
		if (config.mousetwobutton) {
			temp |= 0x10; // bit 4
		} else {
			temp |= 0x20; // bit 5
		}
#endif
	}
	/* Right */
	if (mouse_buttons & 2) {
#ifdef __APPLE__
        temp |= 0x10;
#else
		if (config.mousetwobutton) {
			temp |= 0x20; // bit 5
		} else {
			temp |= 0x10; // bit 4
		}
#endif
	}

	/* bit 0 contains the monitor id bit, 0 for VGA, 1 for TV type monitors.
	   As we will probably always need VGA, leave as 0 */

	/* bit 8 is used to detect to presence of 16 bit sound, if written high
	   and read back low it's 16 bit, due to weak pullup resistor, always
	   return 0 to indicate bit */

	/* Invert the button values */
	return temp ^ 0x70; // bit 4 5 and 6
}

/**
 * Initialise the power-on state of the IOMD chip
 *
 * Called on program startup and on program reset when
 * the configuration is changed.
 *
 * @param type Variant of IOMD chip to emulate
 */
void
iomd_reset(IOMDType type)
{
	assert(type == IOMDType_IOMD || type == IOMDType_ARM7500 || type == IOMDType_ARM7500FE || type == IOMDType_IOMD2);
	iomd_type = type;

	iomd.romcr0 = 0x40; /* ROM Control 0, set to 16bit slowest access time */
	iomd.romcr1 = 0x40; /* ROM Control 1, set to 16bit slowest access time */

        iomd.sndstat = IOMD_DMA_STATUS_INTERRUPT | IOMD_DMA_STATUS_OVERRUN;

        iomd.irqa.status   = IOMD_IRQA_FORCE_BIT | IOMD_IRQA_POWER_ON_RESET;
        iomd.irqb.status   = 0;
        iomd.irqc.status   = 0;
        iomd.irqd.status   = IOMD_IRQD_EVENT_1 | IOMD_IRQD_EVENT_2;
        iomd.fiq.status    = IOMD_FIQ_FORCE_BIT;
        iomd.irqdma.status = 0;

        iomd.irqa.mask   = 0;
        iomd.irqb.mask   = 0;
        iomd.irqc.mask   = 0;
        iomd.irqd.mask   = 0;
        iomd.fiq.mask    = 0;
        iomd.irqdma.mask = 0;

	/* Investigate further */
	// iomd.ctrl = 0x0b; /* I/O Control, ID and I2C pins set as input */

	/* The bottom 4 bits of these fields should be always 0 */
	iomd.vidcur   = 0;
	iomd.vidstart = 0;
	iomd.vidend   = 0;
	iomd.vidinit  = 0;

        soundcount=100000;
        iomd.t0.counter = 0xffff;
        iomd.t1.counter = 0xffff;
        iomd.t0.in_latch = 0xffff;
        iomd.t1.in_latch = 0xffff;

	if (iomd_type == IOMDType_ARM7500 || iomd_type == IOMDType_ARM7500FE) {
		/* ARM7500/ARM7500FE only */
		iomd.susmode = 0;    /* SUSPEND Mode, not in suspend mode */
		iomd.vidimux = 0;    /* LCD and IIS control bits, set to normal */
		iomd.refcr = 1;      /* DRAM refresh, fastest possible refresh rate */
		iomd.dramcr = 0;     /* 3 mem clockcycles RAS precharge, 2 mem clockcycles
		                        RAS-to-CAS delay, FPM memory, all banks set to 32bit wide */
		iomd.selfref = 0;    /* DRAM self refresh control, nCAS and nRAS to normal */

		/* Power on Reset only, push button reset has no effect */
		iomd.clkctrl = 0;    /* Clock Control, all clocks set to divide-by-2 */
	} else {
		/* RPC IOMD21 only */
		iomd.refcr = 0;      /* DRAM refresh */
	}

	cinit = 0;
	sndon = 0;
	flyback = 0;

}

/**
 * Called on program shutdown, free up any resources
 */
void
iomd_end(void)
{
}

/**
 * Signal a change in the Flyback signal from VIDC
 *
 * @param flyback_new New value of Flyback signal
 */
void
iomd_flyback(int flyback_new)
{
	flyback = flyback_new;

	if (flyback) {
		iomd.irqa.status |= IOMD_IRQA_FLYBACK;
		updateirqs();
	}
}
