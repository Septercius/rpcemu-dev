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
		if (podules[c].reset) {
                        podules[c].reset(&podules[c]);
                }
	}

	/* Blank all 8 podules */
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
 * @param broken
 * @return Pointer to entry in the podules array, or NULL on failure
 */
podule *
addpodule(void (*writel)(podule *p, int easi, uint32_t addr, uint32_t val),
              void (*writew)(podule *p, int easi, uint32_t addr, uint16_t val),
              void (*writeb)(podule *p, int easi, uint32_t addr, uint8_t val),
              uint32_t  (*readl)(podule *p, int easi, uint32_t addr),
              uint16_t (*readw)(podule *p, int easi, uint32_t addr),
              uint8_t  (*readb)(podule *p, int easi, uint32_t addr),
              int (*timercallback)(podule *p),
              void (*reset)(podule *p),
              int broken)
{
        if (freepodule==8) return NULL; /*All podules in use!*/
        podules[freepodule].readl=readl;
        podules[freepodule].readw=readw;
        podules[freepodule].readb=readb;
        podules[freepodule].writel=writel;
        podules[freepodule].writew=writew;
        podules[freepodule].writeb=writeb;
        podules[freepodule].timercallback=timercallback;
        podules[freepodule].reset=reset;
        podules[freepodule].broken=broken;
//        rpclog("Podule added at %i\n",freepodule);
        freepodule++;
        return &podules[freepodule-1];
}

/**
 * Raise interrupts if any podules have requested them.
 */
void
rethinkpoduleints(void)
{
        int c;
        iomd.irqb.status &= ~(IOMD_IRQB_PODULE | IOMD_IRQB_PODULE_FIQ_AS_IRQ);
        iomd.fiq.status  &= ~IOMD_FIQ_PODULE;
        for (c=0;c<8;c++)
        {
                if (podules[c].irq)
                {
//                        rpclog("Podule IRQ! %02X\n", iomd.irqb.mask);
                        iomd.irqb.status |= IOMD_IRQB_PODULE;
                }
                if (podules[c].fiq)
                {
                        iomd.irqb.status |= IOMD_IRQB_PODULE_FIQ_AS_IRQ;
                        iomd.fiq.status  |= IOMD_FIQ_PODULE;
                }
        }
        updateirqs();
}

/**
 * Handle a 32-bit write to the podules memory map
 *
 * @param num   Podule number (0-7)
 * @param easi  Write to EASI space (true) or regular IO space (false)
 * @param addr  Address to write to
 * @param val   Value to write
 */
void
writepodulel(int num, int easi, uint32_t addr, uint32_t val)
{
        int oldirq=podules[num].irq,oldfiq=podules[num].fiq;
        if (podules[num].writel)
           podules[num].writel(&podules[num], easi,addr,val);
        if (oldirq!=podules[num].irq || oldfiq!=podules[num].fiq) rethinkpoduleints();
}

/**
 * Handle a 16-bit write to the podules memory map
 *
 * @param num   Podule number (0-7)
 * @param easi  Write to EASI space (true) or regular IO space (false)
 * @param addr  Address to write to
 * @param val   Value to write
 */
void
writepodulew(int num, int easi, uint32_t addr, uint32_t val)
{
        int oldirq=podules[num].irq,oldfiq=podules[num].fiq;
        if (podules[num].writew)
        {
                if (podules[num].broken) podules[num].writel(&podules[num], easi,addr,val);
                else                     podules[num].writew(&podules[num], easi,addr,val>>16);
        }
        if (oldirq!=podules[num].irq || oldfiq!=podules[num].fiq) rethinkpoduleints();
}

/**
 * Handle an 8-bit write to the podules memory map
 *
 * @param num   Podule number (0-7)
 * @param easi  Write to EASI space (true) or regular IO space (false)
 * @param addr  Address to write to
 * @param val   Value to write
 */
void
writepoduleb(int num, int easi, uint32_t addr, uint8_t val)
{
        int oldirq=podules[num].irq,oldfiq=podules[num].fiq;
        if (podules[num].writeb)
           podules[num].writeb(&podules[num], easi,addr,val);
        if (oldirq!=podules[num].irq || oldfiq!=podules[num].fiq) rethinkpoduleints();
}

/**
 * Handle a 32-bit read from the podules memory map
 *
 * @param num   Podule number (0-7)
 * @param easi  Read from EASI space (true) or regular IO space (false)
 * @param addr  Address to read from
 * @return Value at memory address
 */
uint32_t
readpodulel(int num, int easi, uint32_t addr)
{
        int oldirq=podules[num].irq,oldfiq=podules[num].fiq;
        uint32_t temp;
        if (podules[num].readl)
        {
//                if (num==2) rpclog("READ PODULEl 2 %08X\n",addr);
                temp=podules[num].readl(&podules[num], easi, addr);
//                if (num==2) printf("%08X\n",temp);
                if (oldirq!=podules[num].irq || oldfiq!=podules[num].fiq) rethinkpoduleints();
                return temp;
        }
        return 0xFFFFFFFF;
}

/**
 * Handle a 16-bit read from the podules memory map
 *
 * @param num   Podule number (0-7)
 * @param easi  Read from EASI space (true) or regular IO space (false)
 * @param addr  Address to read from
 * @return Value at memory address
 */
uint32_t
readpodulew(int num, int easi, uint32_t addr)
{
        int oldirq=podules[num].irq,oldfiq=podules[num].fiq;
        uint32_t temp;
        if (podules[num].readw)
        {
//                if (num==2) rpclog("READ PODULEw 2 %08X\n",addr);
                if (podules[num].broken) temp=podules[num].readl(&podules[num],easi, addr);
                else                     temp=podules[num].readw(&podules[num],easi, addr);
                if (oldirq!=podules[num].irq || oldfiq!=podules[num].fiq) rethinkpoduleints();
                return temp;
        }
        return 0xFFFF;
}

/**
 * Handle an 8-bit read from the podules memory map
 *
 * @param num   Podule number (0-7)
 * @param easi  Read from EASI space (true) or regular IO space (false)
 * @param addr  Address to read from
 * @return Value at memory address
 */
uint8_t
readpoduleb(int num, int easi, uint32_t addr)
{
        int oldirq=podules[num].irq,oldfiq=podules[num].fiq;
        uint8_t temp;
//        rpclog("READ PODULE %i %08X %02X %i\n",num,addr,temp,easi);
        if (podules[num].readb)
        {
                temp=podules[num].readb(&podules[num], easi, addr);
//                rpclog("READ PODULE %i %08X %02X\n",num,addr,temp);
//                printf("%02X\n",temp);
                if (oldirq!=podules[num].irq || oldfiq!=podules[num].fiq) rethinkpoduleints();
                return temp;
        }
        return 0xFF;
}

/**
 * AT MOMENT NO PODULE REGISTERS A timercallback() SO THIS FUNCTION IS SUPERFLUOUS
 *
 * @param t
 */
void
runpoduletimers(int t)
{
        int c,d;

        /* Loop through podules, ignoring 0 (extn rom) */
        /* This should really make use of the 'freepodule' variable to prevent
           looping over podules that aren't registered */
        for (c=1;c<8;c++)
        {
                if (podules[c].timercallback && podules[c].msectimer)
                {
                        podules[c].msectimer-=t;
                        d=1;
                        while (podules[c].msectimer<=0 && d)
                        {
                                int oldirq=podules[c].irq,oldfiq=podules[c].fiq;
//                                rpclog("Callback! podule %i  %i %i  ",c,podules[c].irq,podules[c].fiq);
                                d=podules[c].timercallback(&podules[c]);
                                if (!d)
                                {
                                        podules[c].msectimer=0;
                                }
                                else podules[c].msectimer+=d;
                                if (oldirq!=podules[c].irq || oldfiq!=podules[c].fiq)
                                {
//                                        rpclog("Now rethinking podule ints...\n");
                                        rethinkpoduleints();
                                }
                        }
                }
        }
}
