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

        if (configmode!=2)
        {
                if ((addr == 0x3F0) && (val==0x55))
                {
                        configmode++;
                        return;
                }
                else
                   configmode=0;
        }
        if (configmode==2 && addr == 0x3F0 && val==0xAA)
        {
                configmode=0;
//                printf("Cleared config mode\n");
                return;
        }
        if (configmode==2)
        {
                if (addr == 0x3F0)
                {
                        configreg=val&15;
//                        printf("Register CR%01X selected\n",configreg);
                        return;
                }
                else
                {
                        configregs[configreg]=val;
//                        printf("CR%01X = %02X\n",configreg,val);
                        return;
                }
        }

//        if (addr >= 0x278 && addr <= 0x27A)
//           rpclog("Write SuperIO %03X %08X %07X %08X\n",addr,val,PC,armregs[12]);
        if ((addr >= 0x3F0) && (addr <= 0x3F7)) writefdc(addr, val);
        if ((addr == 0x27A) && ((val&0x10) || ((printstat^val)&1 && !(val&1))))
        {
//                rpclog("Printer interrupt %02X\n",iomd.maska);
                iomd.stata|=1;
                updateirqs();
        }
        if (addr == 0x27A) printstat=val;
        if ((addr == 0x3F9) && (val&2))
        {
//                printf("Serial transmit empty interrupt\n");
                iomd.statf|=0x10;
                updateirqs();
        }
        if (addr == 0x3FB) linectrl=val;
        if (addr == 0x3FE) scratch=val;
        if ((addr >= 0x1F0 && addr <= 0x1F7) || addr == 0x3F6)
        {
                ideboard=0;
                writeide(addr, val);
                return;
        }
}



uint8_t superio_read(uint32_t addr)
{
        /* Convert memory-mapped address to IO port */
        addr = (addr >> 2) & 0x3ff;

        if (configmode==2 && addr == 0x3F1)
        {
//                printf("Read CR%01X %02X\n",configreg,configregs[configreg]);
                return configregs[configreg];
        }

        if (addr == 0x279) return 0x90;
        if ((addr >= 0x1F0 && addr <= 0x1F7) || addr == 0x3F6)
        {
                ideboard=0;
                return readide(addr);
        }
        if ((addr >= 0x3F0) && (addr <= 0x3F7)) return readfdc(addr);
        if (addr == 0x3FA)
        {
                iomd.statf&=~0x10;
                updateirqs();
                return 2;
        }
        if (addr == 0x3FB) return linectrl;
        if (addr == 0x3FE) return scratch;

/*        if (addr == 0x3F6)
        {
                ide.atastat+=0x40;
                return ide.atastat&0xC0;
        }*/
        return 0;
}
