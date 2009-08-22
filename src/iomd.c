/*RPCemu v0.6 by Tom Walker
  IOMD emulation*/
#include <stdint.h>
#include <allegro.h>
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

//#define IOMD_0x01C_SUSMODE  0x01C /* Enter SUSPEND mode (ARM7500/FE) */

#define IOMD_0x020_IRQSTB   0x020 /* IRQB status */
#define IOMD_0x024_IRQRQB   0x024 /* IRQB request/clear */
#define IOMD_0x028_IRQMSKB  0x028 /* IRQB mask */

//#define IOMD_0x02C_STOPMODE 0x02C /* Enter STOP mode (ARM7500/FE) */

//#define IOMD_0x030_FIQST    0x030 /* FIQ status */
//#define IOMD_0x034_FIQRQ    0x034 /* FIQ request */
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
#define IOMD_0x08C_VREFCR   0x08C /* VRAM and refresh control */

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
//#define IOMD_0x0CC_DMAEXT   0x0CC /* DMA external control     (IOMD) */
//#define IOMD_0x0CC_ASTCR    0x0CC /* Async I/O timing control (ARM7500/FE) */

#define IOMD_0x0D0_DRAMWID  0x0D0 /* DRAM width control      (ARM7500/FE) */
//#define IOMD_0x0D4_SELFREF  0x0D4 /* Force CAS/RAS lines low (ARM7500/FE) */


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


/* IOMD Chip Identifiers */
#define CHIP_ID_LOW_IOMD        0xe7     /* IOMD */
#define CHIP_ID_HIGH_IOMD       0xd4
#define CHIP_ID_LOW_ARM7500     0x98     /* ARM7500 */
#define CHIP_ID_HIGH_ARM7500    0x5b
//#define CHIP_ID_LOW_ARM7500FE   0x7c     /* ARM7500FE */
//#define CHIP_ID_HIGH_ARM7500FE  0xaa

struct iomd iomd;

int fdccallback = 0;
int motoron = 0;

int kcallback = 0, mcallback = 0;
int idecallback = 0;
uint32_t cinit = 0;

static int sndon = 0;
static int flyback=0;

void updateirqs(void)
{
        if ((iomd.irqa.status & iomd.irqa.mask) ||
            (iomd.irqb.status & iomd.irqb.mask) ||
            (iomd.irqd.status & iomd.irqd.mask) ||
            (iomd.irqdma.status & iomd.irqdma.mask))
        {
                armirq |= 1;
        } else {
                armirq &= ~1;
        }
        if (iomd.fiq.status & iomd.fiq.mask) {
                armirq |= 2;
        } else {
                armirq &= ~2;
        }
}

/**
 * Handle the regularly ticking interrupts, the two
 * IOMD timers, the sound interrupt and podule
 * interrupts
 *
 * Called (theoretically) 500 times a second IMPROVE.
 */
static void gentimerirq(void)
{
        if (!infocus) return;

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
                        updatesoundirq();
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
void writeiomd(uint32_t addr, uint32_t val)
{
        static int readinc = 0;

        switch (addr&0x1FC)
        {
        case IOMD_0x000_IOCR: /* I/O control */
                cmosi2cchange(val&2,val&1);
                iomd.ctrl=val;
                return;

        case IOMD_0x004_KBDDAT: /* Keyboard data */
                keyboard_data_write(val);
                return;
        case IOMD_0x008_KBDCR: /* Keyboard control */
                keyboard_control_write(val & 8);
                return;

        case IOMD_0x00C_IOLINES: /* General Purpose I/O lines (ARM7500/FE) */
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

        case IOMD_0x028_IRQMSKB: /* IRQB mask */
                iomd.irqb.mask = val;
                updateirqs();
                return;

        case IOMD_0x038_FIQMSK: /* FIQ mask */
                iomd.fiq.mask = val;
                return;

        case IOMD_0x03C_CLKCTL: /* Clock divider control (ARM7500/FE) */
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

        case IOMD_0x08C_VREFCR: /* VRAM refresh control */
                return;

        case IOMD_0x090_FSIZE: /* Flyback line size (IOMD) */
                return;

        case IOMD_0x0A8_MSEDAT: /* Mouse data (PS/2 - ARM7500/FE) */
                writems(val);
                return;
        case IOMD_0x0AC_MSECR: /* Mouse control (PS/2 - ARM7500/FE) */
                writemsenable(val);
                return;

        case IOMD_0x0C4_IOTCR: /* I/O timing control */
        case IOMD_0x0C8_ECTCR: /* I/O expansion timing control */
        case IOMD_0x0D0_DRAMWID: /* DRAM width control (ARM7500/FE) */
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
                iomd.sndstat&=1;
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
                        iomd.sndstat=6;
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
                iomd.vidcur=val&0x7FFFFF;
                // rpclog("Vidcur = %08X\n",val);
                return;
        case IOMD_0x1D4_VIDEND: /* Video DMA End */
                if (config.vrammask && config.model != CPUModel_ARM7500)
                        iomd.vidend = (val + 2048) & 0x7FFFF0;
                else
                        iomd.vidend = (val + 16) & 0x7FFFF0;
                return;
        case IOMD_0x1D8_VIDSTART: /* Video DMA Start */
                iomd.vidstart=val&0x7FFFFF;
                resetbuffer();
                return;
        case IOMD_0x1DC_VIDINIT: /* Video DMA Init */
                iomd.vidinit=val;
                resetbuffer();
                return;
        case IOMD_0x1E0_VIDCR: /* Video DMA Control */
                iomd.vidcr=val;
                resetbuffer();
                return;

        case IOMD_0x1F0_DMAST: /* DMA interupt status */
        case IOMD_0x1F4_DMARQ: /* DMA interupt request */
                return;
        case IOMD_0x1F8_DMAMSK: /* DMA interupt mask */
                iomd.irqdma.mask = val;
                return;

        default:
                UNIMPLEMENTED("IOMD Register write",
                              "Unknown register 0x%x", addr & 0x1fc);
                return;
                error("Bad IOMD write register %03X %08X\n",addr&0x1FC,val);
                dumpregs();
                exit(-1);
        }
}

/**
 * Handle reads from IOMD memory space
 *
 * @param addr Absolute memory address in IOMD space
 * @return Value associated with that memory address function
 */
uint32_t readiomd(uint32_t addr)
{
        switch (addr&0x1FC)
        {
        case IOMD_0x000_IOCR: /* I/O control */
                return ((i2cclock)?2:0)|((i2cdata)?1:0)|(iomd.ctrl&0x7C)|4|((flyback)?0x80:0);

        case IOMD_0x004_KBDDAT: /* Keyboard data */
                return keyboard_data_read();
        case IOMD_0x008_KBDCR: /* Keyboard control */
                return keyboard_status_read();

        case IOMD_0x00C_IOLINES: /* General Purpose I/O lines (ARM7500/FE) */
                return 0;

        case IOMD_0x010_IRQSTA: /* IRQA status */
                return iomd.irqa.status;
        case IOMD_0x014_IRQRQA: /* IRQA request/clear */
                return iomd.irqa.status & iomd.irqa.mask;
        case IOMD_0x018_IRQMSKA: /* IRQA mask */
                return iomd.irqa.mask;

        case IOMD_0x020_IRQSTB: /* IRQB status */
                return iomd.irqb.status;
        case IOMD_0x024_IRQRQB: /* IRQB request/clear */
                return iomd.irqb.status & iomd.irqb.mask;
        case IOMD_0x028_IRQMSKB: /* IRQB mask */
                return iomd.irqb.mask;

        case IOMD_0x038_FIQMSK: /* FIQ mask */
                return iomd.fiq.mask;

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

        case IOMD_0x094_ID0: /* Chip ID no. low byte */
                if (config.model == CPUModel_ARM7500)
                        return CHIP_ID_LOW_ARM7500;
                return CHIP_ID_LOW_IOMD;
        case IOMD_0x098_ID1: /* Chip ID no. high byte */
                if (config.model == CPUModel_ARM7500)
                        return CHIP_ID_HIGH_ARM7500;
                return CHIP_ID_HIGH_IOMD;
        case IOMD_0x09C_VERSION: /* Chip version number */
                return 0; /*Chip version*/

        case IOMD_0x0A0_MOUSEX: /* Mouse X position (Quadrature - IOMD) */
                /*rpclog("Read mousex %i\n",iomd.mousex);*/
                return iomd.mousex;
        case IOMD_0x0A4_MOUSEY: /* Mouse Y position (Quadrature - IOMD) */
                /*rpclog("Read mousey %i\n",-iomd.mousey);*/
                return -iomd.mousey;

        case IOMD_0x0A8_MSEDAT: /* Mouse data (PS/2 - ARM7500/FE) */
                return readmousedata();
        case IOMD_0x0AC_MSECR: /* Mouse control (PS/2 - ARM7500/FE) */
                return getmousestat();

        case IOMD_0x180_SD0CURA: /* Sound DMA 0 CurA */
        case IOMD_0x184_SD0ENDA: /* Sound DMA 0 EndA */
        case IOMD_0x188_SD0CURB: /* Sound DMA 0 CurB */
        case IOMD_0x18C_SD0ENDB: /* Sound DMA 0 EndB */
                return 0;
        case IOMD_0x194_SD0ST: /* Sound DMA 0 Status */
                return iomd.sndstat;

        case IOMD_0x1D4_VIDEND: /* Video DMA End */
                return iomd.vidend&~15;
        case IOMD_0x1D8_VIDSTART: /* Video DMA Start */
                return iomd.vidstart&~15;
        case IOMD_0x1E0_VIDCR: /* Video DMA Control */
                return iomd.vidcr|0x50;

        case IOMD_0x1F0_DMAST: /* DMA interupt status */
                return iomd.irqdma.status;
        case IOMD_0x1F4_DMARQ: /* DMA interupt request */
                return iomd.irqdma.status & iomd.irqdma.mask;
        case IOMD_0x1F8_DMAMSK: /* DMA interupt mask */
                return iomd.irqdma.mask;

        default:
                UNIMPLEMENTED("IOMD Register read",
                              "Unknown register 0x%x", addr & 0x1fc);

        }
        return 0;
        printf("Bad IOMD read register %03X\n",addr&0x1FC);
        dumpregs();
        exit(-1);
}

/**
 * Read the state of the Quadrature (bus) mouse
 * found on the RPC.
 *
 * Also contains the monitor ID bit IMPROVE
 */
uint8_t mouse_buttons_read(void)
{
        unsigned char temp = 0;

        /* 'mouse_b' and 'key' are Allegro variables containing
           the current host OS mouse and keyboard state */

        /* Left (select) */
        if (mouse_b & 1) {
               temp |= 0x40; // bit 7
        }
        /* Middle (menu) */
        if ((mouse_b & 4) || key[KEY_MENU] || key[KEY_ALTGR]) {
               temp |= 0x20; // bit 6
        }
        /* Right (adjust) */
        if ((mouse_b & 2)) {
               temp |= 0x10; // bit 5
        }

        return temp ^ 0x70; // bit 5 6 and 7
}

void dumpiomd(void)
{
        error("IOMD :\n"
              "STATA %02X STATB %02X STATC %02X STATD %02X\n"
              "MASKA %02X MASKB %02X MASKC %02X MASKD %02X\n",
              iomd.irqa.status,
              iomd.irqb.status,
              iomd.irqc.status,
              iomd.irqd.status,
              iomd.irqa.mask,
              iomd.irqb.mask,
              iomd.irqc.mask,
              iomd.irqd.mask);
}

/**
 * Initialise the power-on state of the IOMD chip
 *
 * Called on program startup and on program reset when
 * the configuration is changed.
 */
void resetiomd(void)
{
        remove_int(gentimerirq);
        iomd.romcr0=iomd.romcr1=0x40;
        iomd.sndstat=6;

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

        soundcount=100000;
        iomd.t0.counter = 0xffff;
        iomd.t1.counter = 0xffff;
        iomd.t0.in_latch = 0xffff;
        iomd.t1.in_latch = 0xffff;
        install_int_ex(gentimerirq, BPS_TO_TIMER(500)); /* 500 Hz */
}

/**
 * Called on program shutdown, free up any resources
 */
void endiomd(void)
{
        remove_int(gentimerirq);
}

void updateiomdtimers(void)
{
        if (iomd.t0.counter < 0)
        {
                iomd.t0.counter += iomd.t0.in_latch;
                iomd.irqa.status |= IOMD_IRQA_TIMER_0;
                updateirqs();
        }
        if (iomd.t1.counter < 0)
        {
                iomd.t1.counter += iomd.t1.in_latch;
                iomd.irqa.status |= IOMD_IRQA_TIMER_1;
                updateirqs();
        }
}

void iomdvsync(int vsync)
{
        if (vsync)
        {
//                rpclog("Vsync high\n");
                iomd.irqa.status |= IOMD_IRQA_FLYBACK;
                updateirqs();
                flyback=1;
        }
        else
        {
//                rpclog("Vsync low\n");
                flyback=0;
        }
}

void dumpiomdregs(void)
{
        printf("IRQ STAT A %02X B %02X D %02X F %02X\n",
               iomd.irqa.status, iomd.irqb.status,
               iomd.irqd.status, iomd.fiq.status);
        printf("IRQ MASK A %02X B %02X D %02X F %02X\n",
               iomd.irqa.mask, iomd.irqb.mask,
               iomd.irqd.mask, iomd.fiq.mask);
}
