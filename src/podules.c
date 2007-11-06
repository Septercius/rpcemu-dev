#include "rpcemu.h"
#include "iomd.h"
#include "podules.h"

/*Podules -
  0 is reserved for extension ROMs
  1 is for additional IDE interface
  2-3 are free
  4-7 are not implemented (yet)*/
podule podules[8];
int freepodule;

void initpodules()
{
        int c;
        for (c=0;c<8;c++)
        {
                podules[c].readw=podules[c].readb=podules[c].writew=podules[c].writeb=NULL;
                podules[c].irq=podules[c].fiq=0;
        }
        freepodule=0;
}
  
int addpodule(void (*writel)(podule *p, uint32_t addr, uint32_t val),
              void (*writew)(podule *p, uint32_t addr, uint16_t val),
              void (*writeb)(podule *p, uint32_t addr, uint8_t val),
              uint32_t  (*readl)(podule *p, uint32_t addr),
              uint16_t (*readw)(podule *p, uint32_t addr),
              uint8_t  (*readb)(podule *p, uint32_t addr))
{
        if (freepodule==8) return -1; /*All podules in use!*/
        podules[freepodule].readl=readl;
        podules[freepodule].readw=readw;
        podules[freepodule].readb=readb;
        podules[freepodule].writel=writel;
        podules[freepodule].writew=writew;
        podules[freepodule].writeb=writeb;
        rpclog("Podule added at %i\n",freepodule);
        freepodule++;
}

void rethinkpoduleints()
{
        int c;
        iomd.statb&=~0x21; /*1 is FIQ downgrade, 0x20 is IRQ*/
        iomd.statf&=~0x40; /*0x40 is FIQ*/
        for (c=0;c<8;c++)
        {
                if (podules[c].irq) iomd.statb|=0x20;
                if (podules[c].fiq)
                {
                        iomd.statb|=1;
                        iomd.statf|=0x40;
                }
        }
        updateirqs();
}

void writepodulel(int num, uint32_t addr, uint32_t val)
{
        int oldirq=podules[num].irq,oldfiq=podules[num].fiq;
        if (podules[num].writel)
           podules[num].writel(&podules[num],addr,val);
        if (oldirq!=podules[num].irq || oldfiq!=podules[num].fiq) rethinkpoduleints();
}

void writepodulew(int num, uint32_t addr, uint16_t val)
{
        int oldirq=podules[num].irq,oldfiq=podules[num].fiq;
        if (podules[num].writew)
           podules[num].writew(&podules[num],addr,val);
        if (oldirq!=podules[num].irq || oldfiq!=podules[num].fiq) rethinkpoduleints();
}

void writepoduleb(int num, uint32_t addr, uint8_t val)
{
        int oldirq=podules[num].irq,oldfiq=podules[num].fiq;
        if (podules[num].writeb)
           podules[num].writeb(&podules[num],addr,val);
        if (oldirq!=podules[num].irq || oldfiq!=podules[num].fiq) rethinkpoduleints();
}

uint32_t readpodulel(int num, uint32_t addr)
{
        int oldirq=podules[num].irq,oldfiq=podules[num].fiq;
        uint32_t temp;
        if (podules[num].readl)
        {
//                if (num==2) rpclog("READ PODULEl 2 %08X\n",addr);
                temp=podules[num].readl(&podules[num],addr);
//                if (num==2) printf("%08X\n",temp);
                if (oldirq!=podules[num].irq || oldfiq!=podules[num].fiq) rethinkpoduleints();
                return temp;
        }
        return 0xFFFFFFFF;
}

uint16_t readpodulew(int num, uint32_t addr)
{
        int oldirq=podules[num].irq,oldfiq=podules[num].fiq;
        uint16_t temp;
        if (podules[num].readw)
        {
//                if (num==2) rpclog("READ PODULEw 2 %08X\n",addr);
                temp=podules[num].readw(&podules[num],addr);
                if (oldirq!=podules[num].irq || oldfiq!=podules[num].fiq) rethinkpoduleints();
                return temp;
        }
        return 0xFFFFFFFF;
}

uint8_t readpoduleb(int num, uint32_t addr)
{
        int oldirq=podules[num].irq,oldfiq=podules[num].fiq;
        uint8_t temp;
        if (podules[num].readb)
        {
                temp=podules[num].readb(&podules[num],addr);
//                rpclog("READ PODULE %i %08X %02X\n",num,addr,temp);
//                printf("%02X\n",temp);
                if (oldirq!=podules[num].irq || oldfiq!=podules[num].fiq) rethinkpoduleints();
                return temp;
        }
        return 0xFFFFFFFF;
}
