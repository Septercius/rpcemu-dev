/*RPCemu v0.5 by Tom Walker
  IOMD emulation*/

#include <allegro.h>
#include <stdio.h>
#include "rpcemu.h"

int curchange;

int sndon=0;
static int flyback=0;

void updateirqs(void)
{
        if ((iomd.stata&iomd.maska) || (iomd.statb&iomd.maskb) || (iomd.statd&iomd.maskd) || (iomd.state&iomd.maske))
           irq=1;
        else
           irq=0;
        if (iomd.statf&iomd.maskf) irq|=2;
        if (!irq) armirq=0;
}

void gentimerirq()
{
        if (!infocus) return;
        if (inscount==lastinscount)
        {
                delaygenirqleft++;
//                rpclog("Haven't moved! %i\n",inscount);
                return;
        }
//           rpclog("Haven't moved!\n");
        lastinscount=inscount;
//        rpclog("IRQ %i\n",inscount);
        iomd.t0c-=10000;
        while (iomd.t0c<0 && iomd.t0l)
        {
                iomd.t0c+=iomd.t0l;
                iomd.stata|=0x20;
                updateirqs();
        }
        iomd.t1c-=10000;
        while (iomd.t1c<0 && iomd.t1l)
        {
                iomd.t1c+=iomd.t1l;
                iomd.stata|=0x40;
                updateirqs();
        }
        if (soundinited && sndon)
        {
                soundcount-=10000;
                if (soundcount<0)
                {
                        updatesoundirq();
                        soundcount+=soundlatch;
                }
        }
}

void timerairq(void)
{
        iomd.stata|=0x20;
        updateirqs();
}

void settimera(int latch)
{
        latch++;
/*        if ((latch/2000)==0)
           install_int_ex(timerairq,latch/2);
        else
           install_int_ex(timerairq,MSEC_TO_TIMER(latch/2000));*/
}

void timerbirq(void)
{
        iomd.stata|=0x40;
        updateirqs();
}

void settimerb(int latch)
{
        latch++;
/*        if ((latch/2000)==0)
           install_int_ex(timerbirq,latch/2);
        else
           install_int_ex(timerbirq,MSEC_TO_TIMER(latch/2000));*/
}

int nextbuf;

void writeiomd(uint32_t addr, uint32_t val)
{
        switch (addr&0x1FC)
        {
                case 0: /*Control*/
                cmosi2cchange(val&2,val&1);
                iomd.ctrl=val;
                return;
                case 0x04: /*Keyboard data*/
                writekbd(val);
                return;
                case 0x08: /*Keyboard control*/
                writekbdenable(val&8);
                return;
                case 0x0C: /*IO port control*/
                return;
                case 0xA8: /*Mouse data*/
                writems(val);
                return;
                case 0xAC: /*Mouse control*/
                writemsenable(val);
                return;
                case 0x14: /*Int A clear*/
                iomd.stata&=~val;
                updateirqs();
                return;
                case 0x18: /*Int A mask*/
                iomd.maska=val;
                updateirqs();
                return;
                case 0x28: /*Int B mask*/
                iomd.maskb=val;
                updateirqs();
                return;
                case 0x38: /*Int F mask*/
                iomd.maskf=val;
                return;
                case 0x3C: /*Clock control*/
                return;
                case 0x40: iomd.t0l=(iomd.t0l&0xFF00)|(val&0xFF); break;
                case 0x44: iomd.t0l=(iomd.t0l&0xFF)|((val&0xFF)<<8); break;
                case 0x48: iomd.t0c=iomd.t0l-1; settimera(iomd.t0l); break;
                case 0x4C: iomd.t0r=iomd.t0c--; if (iomd.t0c<0) iomd.t0c=iomd.t0l; break;
                case 0x50: iomd.t1l=(iomd.t1l&0xFF00)|(val&0xFF); break;
                case 0x54: iomd.t1l=(iomd.t1l&0xFF)|((val&0xFF)<<8); break;
                case 0x58: iomd.t1c=iomd.t1l-1; settimerb(iomd.t1l); break;
                case 0x5C: iomd.t1r=iomd.t1c--; if (iomd.t1c<0) iomd.t1c=iomd.t1l; break;
                case 0x68: /*Int C mask*/
                iomd.maskc=val;
                return;
                case 0x74:
                return;
                case 0x78: /*Int D mask*/
                iomd.maskd=val;
                return;
                case 0x80: iomd.romcr0=val; return; /*ROM control*/
                case 0x84: iomd.romcr1=val; return;
                case 0x6C: /*VIDMUX*/
                case 0x8C: /*VRAM refresh control*/
                return;
                case 0x90: /*Flyback line size*/
                case 0xC4: /*IO timing control*/
                case 0xC8: /*IO expansion timing control*/
                case 0xD0: /*DRAM width*/
                return;
                case 0x180: /*Sound DMA current A*/
                case 0x184: /*Sound DMA end A*/
                case 0x188: /*Sound DMA current B*/
                case 0x18C: /*Sound DMA end B*/
//                rpclog("Write %08X %02X\n",addr,val);
                iomd.sndstat&=1;
                iomd.state&=~0x10;
                updateirqs();
                soundaddr[(addr>>2)&3]=val;
                nextbuf=1;
//                rpclog("Buffer A start %08X len %08X\nBuffer B start %08X len %08X\n",soundaddr[0],(soundaddr[1]-soundaddr[0])&0xFFC,soundaddr[2],(soundaddr[3]-soundaddr[2])&0xFFC);
                return;
                case 0x190: /*Sound DMA control*/
//                rpclog("Write %08X %02X\n",addr,val);
                if (val&0x80)
                {
                        iomd.sndstat=6;
                        soundinited=1;
                        iomd.state|=0x10;
                        updateirqs();
                }
                sndon=val&0x20;
                return;
                case 0x1C0: /*Cursor DMA*/
                return;
                case 0x1C4:
//                if (cinit!=val) curchange=1;
                cinit=val;
//                curchange=1;
                return;
                case 0x1D0: /*Video DMA current*/
                iomd.vidcur=val&0x1FFFFF;                
//                rpclog("Vidcur = %08X\n",val);
                return;
                case 0x1D4: /*Video DMA end*/
                if (vrammask && model) iomd.vidend=(val+2048)&0x1FFFF0;
                else                   iomd.vidend=(val+16)&0x1FFFF0;
                return;
                case 0x1D8: /*Video DMA start*/
                iomd.vidstart=val&0x1FFFFF;
                resetbuffer();
                return;
                case 0x1DC: /*Video DMA init*/
                iomd.vidinit=val;
                resetbuffer();
                return;
                case 0x1E0: /*Video DMA control*/
                iomd.vidcr=val;
                resetbuffer();
                return;
                case 0x1F0: case 0x1F4:
                return;
                case 0x1F8: /*Sound interrupt status*/
                iomd.maske=val;
                return;
                default:
                return;
                error("Bad IOMD write register %03X %08X\n",addr&0x1FC,val);
                dumpregs();
                exit(-1);
        }
}

int keytemp=0;
uint32_t readiomd(uint32_t addr)
{
        switch (addr&0x1FC)
        {
                case 0x00: 
                return ((i2cclock)?2:0)|((i2cdata)?1:0)|(iomd.ctrl&0x7C)|4|((flyback)?0x80:0);/*Control*/
                case 0x04: return readkeyboarddata(); /*Keyboard data*/
                case 0x08: return getkeyboardstat(); /*Keyboard control*/
                case 0x0C: return 0;
                case 0x10: /*IRQ status A*/
                return iomd.stata;
                case 0x14: /*Int A pending*/
                return iomd.stata&iomd.maska;
                case 0x18: /*Int A mask*/
                return iomd.maska;
                case 0x20: /*IRQ status B*/
                return iomd.statb;
                case 0x24: /*Int B pending*/
                return iomd.statb&iomd.maskb;
                case 0x28: /*Int B mask*/
                return iomd.maskb;
                case 0x38: /*Int F mask*/
                return iomd.maskf;
                case 0x40: return iomd.t0r&0xFF;
                case 0x44: return iomd.t0r>>8;
                case 0x50: return iomd.t1r&0xFF;
                case 0x54: return iomd.t1r>>8;
                case 0x60: return iomd.statc;
                case 0x64: /*Int C pending*/
                return iomd.statc&iomd.maskc;
                case 0x68: /*Int C mask*/
                return iomd.maskc;
                case 0x70: /*Int D status*/
                return iomd.statd|0x18;
                case 0x74: /*Int D pending*/
                return (iomd.statd|0x18)&iomd.maskd;
                case 0x78: /*Int D mask*/
                return iomd.maskd;
                case 0x80: return iomd.romcr0; /*ROM control*/
                case 0x84: return iomd.romcr1;
                case 0x94: /*Chip ID registers*/
                if (model==0) return 0x98; /*ARM7500*/
                return 0xE7;               /*IOMD*/
                case 0x98: /*Chip ID registers*/
                if (model==0) return 0x5B; /*ARM7500*/
                return 0xD4;               /*IOMD*/
                case 0x9C: return 0; /*Chip version*/
                case 0xA0: return iomd.mousex;
                case 0xA4: return -iomd.mousey;
                case 0xA8: return readmousedata(); /*Mouse data*/
                case 0xAC: return getmousestat(); /*Mouse control*/
                case 0x180: case 0x184: case 0x188: case 0x18C: return 0;
                case 0x194: /*Sound DMA stat*/
                return iomd.sndstat;
                case 0x1D4: /*Video DMA end*/
                return iomd.vidend&~15;
                case 0x1D8: /*Video DMA start*/
                return iomd.vidstart&~15;
                case 0x1E0: /*Video DMA control*/
                return iomd.vidcr|0x50;
                case 0x1F0:
                return iomd.state;
                case 0x1F4:
                return iomd.state&iomd.maske;
                case 0x1F8:
                return iomd.maske;
        }
        return 0;
        printf("Bad IOMD read register %03X\n",addr&0x1FC);
        dumpregs();
        exit(-1);
}

uint8_t readmb(void)
{
        unsigned char temp=0;
        if (mouse_b&1) temp|=0x40;
        if ((mouse_b&4) || key[KEY_MENU]) temp|=0x20;
        if (mouse_b&2) temp|=0x10;
        return temp^0x70;
}

void dumpiomd(void)
{
        error("IOMD :\nSTATA %02X STATB %02X STATC %02X STATD %02X\nMASKA %02X MASKB %02X MASKC %02X MASKD %02X\n",iomd.stata,iomd.statb,iomd.statc,iomd.statd,iomd.maska,iomd.maskb,iomd.maskc,iomd.maskd);
}

void resetiomd(void)
{
        iomd.stata=0x10;
        iomd.romcr0=iomd.romcr1=0x40;
        iomd.sndstat=6;
        iomd.statb=iomd.statc=iomd.statd=0;
        iomd.maska=iomd.maskb=iomd.maskc=iomd.maskd=iomd.maske=0;
        remove_int(timerairq);
        remove_int(timerbirq);
        install_int_ex(gentimerirq,BPS_TO_TIMER(200));
}

void updateiomdtimers()
{
        if (iomd.t0c<0)
        {
                iomd.t0c+=iomd.t0l;
                iomd.stata|=0x20;
                updateirqs();
        }
        if (iomd.t1c<0)
        {
                iomd.t1c+=iomd.t1l;
                iomd.stata|=0x40;
                updateirqs();
        }
}

void iomdvsync()
{
        iomd.stata|=8;
        updateirqs();
        flyback=20;
}

void dumpiomdregs()
{
        printf("IRQ STAT A %02X B %02X D %02X F %02X\n",iomd.stata,iomd.statb,iomd.statd,iomd.statf);
        printf("IRQ MASK A %02X B %02X D %02X F %02X\n",iomd.maska,iomd.maskb,iomd.maskd,iomd.maskf);
}
