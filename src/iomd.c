/*RPCemu v0.3 by Tom Walker
  IOMD emulation*/

#include <allegro.h>
#include <stdio.h>
#include "rpc.h"

int samplefreq;
uint32_t soundaddr[4];

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
//        printf("Update IRQs - %02X %02X %02X %02X %i %08X %08X %02X %02X\n",iomd.stata,iomd.maska,iomd.statb,iomd.maskb,irq,armregs[15],armregs[16],iomd.statf,iomd.maskf);
}

FILE *sndfile;
int getbufferlen()
{
        int offset=(iomd.sndstat&1)<<1,start,end;        
        start=soundaddr[offset]&0xFF0;
        end=(soundaddr[offset+1]&0xFF0)+16;
        return end-start;
}

AUDIOSTREAM *as;
unsigned short *asp;
int bufferlen=100;
unsigned short sndbuffer[16384];
int sndsamples,sndpos=0,sndoffset=0;

void initsound()
{
        install_sound(DIGI_AUTODETECT,MIDI_NONE,0);
        as=play_audio_stream(4096,16,1,40960,0,128);
        sndsamples=0;
}

void restartsound()
{
        if (as) stop_audio_stream(as);
        as=play_audio_stream(bufferlen,16,1,samplefreq,255,128);
}

void transferbuffer()
{
        asp=NULL;
        while (asp==NULL)
        {
                asp=get_audio_stream_buffer(as);
                sleep(0);
        }
        if (asp!=NULL)
        {
                memcpy(asp,&sndbuffer[sndoffset],16384);
                free_audio_stream_buffer(as);
                sndoffset^=0x2000;
        }
}

int oldfreq=1;
int dumpsound()
{
        int c,d=0;
        int offset=(iomd.sndstat&1)<<1;
        unsigned long temp,page;
        int start,end;
        if (!sndon) return -1;
//        if (!sndfile) sndfile=fopen("sound.pcm","wb");
        page=soundaddr[offset]&0xFFFFF000;
        start=soundaddr[offset]&0xFF0;
        end=(soundaddr[offset+1]&0xFF0)+16;
        c=end-start;
        if (c!=(bufferlen<<2) || samplefreq!=oldfreq)
        {
//                rpclog("Size mismatch - c %i bufferlen %i %i %i\n",c,bufferlen,bufferlen<<2,c>>2);
                bufferlen=c>>2;
                oldfreq=samplefreq;
//                if (!bufferlen) bufferlen=256;
                restartsound();
                return -1;
        }
//        rpclog("Sound %08X %08X %08X %08X %i %08X %03X %03X\n",soundaddr[0],soundaddr[1],soundaddr[2],soundaddr[3],offset,page,start,end);
        /** Disabled: Causes segfault - No idea why or even what this does
        for (c=start;c<end;c+=4)
        {
                temp=ram[((c+page)&rammask)>>2];
                asp[d]=(temp&0xFFFF)^0x8000;
                asp[d+1]=(temp>>16)^0x8000;
                d+=2;
        }
        **/
        iomd.state|=0x10;
        updateirqs();
        iomd.sndstat^=1;
        iomd.sndstat|=6;
//        updatesndtime(end-start);
//        if (sndsamples>=4096)
//           transferbuffer();
        return end - start;
}

void checksound()
{
        asp=get_audio_stream_buffer(as);
        if (asp!=NULL)
        {
                if (sndon)
                   dumpsound();
                else
                   memset(asp,0,bufferlen<<2);
                free_audio_stream_buffer(as);
//                rpclog("Refilled buffer %i\n",sndon);
        }
}
       
int soundinited;

void timerairq(void)
{
        iomd.stata|=0x20;
        updateirqs();
}

void settimera(int latch)
{
//        return;
        latch++;
        if ((latch/2000)==0)
           install_int_ex(timerairq,latch/2);
        else
           install_int_ex(timerairq,MSEC_TO_TIMER(latch/2000));
        rpclog("Timer 0 %04X %i %i\n",latch,latch/2,latch/2000);
//        printf("Timer set to %i MSECs %i cycles %f seconds\n",latch/2000,latch,1193181.0f/(float)(latch/2));
}

void timerbirq(void)
{
        iomd.stata|=0x40;
        updateirqs();
}

void settimerb(int latch)
{
//        return;
        latch++;
        if ((latch/2000)==0)
           install_int_ex(timerbirq,latch/2);
        else
           install_int_ex(timerbirq,MSEC_TO_TIMER(latch/2000));
        rpclog("Timer 1 %04X %i %i\n",latch,latch/2,latch/2000);
//        printf("Timer set to %i MSECs %i cycles %f seconds\n",latch/2000,latch,1193181.0f/(float)(latch/2));
}

void writeiomd(uint32_t addr, uint32_t val)
{
        switch (addr&0x1FC)
        {
                case 0: /*Control*/
                cmosi2cchange(val&2,val&1);
                iomd.ctrl=val;
//                printf("Write control %i %i %08X\n",val&2,val&1,armregs[3]);
                return;
                case 0x04: /*Keyboard data*/
//                printf("Keyboard data write %02X %07X\n",val,PC);
                writekbd(val);
                return;
                case 0x08: /*Keyboard control*/
//                printf("Keyboard control write %02X %07X\n",val,PC);
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
//                printf("IRQ A clear %02X\n",val);
//-                if (val==4) output=0;
                iomd.stata&=~val;
                updateirqs();
                return;
                case 0x18: /*Int A mask*/
//                printf("INT A mask now %02X %07X\n",val,PC);
                iomd.maska=val;
                updateirqs();
                return;
                case 0x28: /*Int B mask*/
//                printf("INT B mask now %02X %07X\n",val,PC);
                iomd.maskb=val;
                updateirqs();
                return;
                case 0x38: /*Int F mask*/
//                printf("FIQ mask %02X\n",val);
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
//                printf("Write mask D %02X\n",val);
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
//                rpclog("Write Sound %08X %08X\n",addr,val);
                iomd.sndstat&=1;
                iomd.state&=~0x10;
                updateirqs();
                soundaddr[(addr>>2)&3]=val;
                return;
                case 0x190: /*Sound DMA control*/
//                rpclog("Write Sound control %08X %08X\n",addr,val);
                if (val&0x80)
                {
                        iomd.sndstat=6;
                        soundinited=1;
                        iomd.state|=0x10;
                        updateirqs();
                }
//                if (!sndon && val&0x20)
//                   updatesndtime((soundaddr[0]&0xFF0)-((soundaddr[1]+16)&0xFF0));
                sndon=val&0x20;
                return;
                case 0x1C0: /*Cursor DMA*/
                return;
                case 0x1C4:
                cinit=val;
                return;
                case 0x1D0: /*Video DMA current*/
                iomd.vidcur=val&0x1FFFFF;
                return;
                case 0x1D4: /*Video DMA end*/
                if (vrammask) iomd.vidend=(val+2048)&0x1FFFFF;
                else          iomd.vidend=(val+8)&0x1FFFFF;
                return;
                case 0x1D8: /*Video DMA start*/
                iomd.vidstart=val&0x1FFFFF;
                resetbuffer();
                return;
                case 0x1DC: /*Video DMA init*/
                iomd.vidinit=val;
                resetbuffer();
//                rpclog("VIDinit %08X\n",val);
                return;
                case 0x1E0: /*Video DMA control*/
                iomd.vidcr=val;
                resetbuffer();
//                rpclog("VIDCR %08X\n",val);
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
//        int temp;
//        rpclog("Read IOMD %08X %08X\n",addr,PC);
        switch (addr&0x1FC)
        {
                case 0x00: /*printf("Read control %i %i %07X %08X\n",i2cclock,i2cdata,PC,armregs[3]);*/
                return ((i2cclock)?2:0)|((i2cdata)?1:0)|(iomd.ctrl&0x7C)|4|((flyback)?0x80:0);/*Control*/
                case 0x04: return readkeyboarddata(); /*Keyboard data*/
                case 0x08: return getkeyboardstat(); /*Keyboard control*/
                case 0x0C: return 0;
                case 0x10: /*IRQ status A*/
//                if (iomd.stata&0x10) rpclog("IOMD STATA %02X %08X\n",iomd.stata,PC);
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
//                printf("Read D stat %02X\n",iomd.statd);
                return iomd.statd|0x18;
                case 0x74: /*Int D pending*/
//                printf("Read D pending %02X %02X\n",iomd.statd&iomd.maskd,iomd.statd);
                return (iomd.statd|0x18)&iomd.maskd;
                case 0x78: /*Int D mask*/
//                printf("Read D mask %02X\n",iomd.maskd);
                return iomd.maskd;
                case 0x80: return iomd.romcr0; /*ROM control*/
                case 0x84: return iomd.romcr1;
                case 0x94: /*Chip ID registers*/
//                rpclog("Read Chip ID0 %02X %02X\n",model,(model)?0xE7:0x98);
                if (model==0) return 0x98; /*ARM7500*/
                return 0xE7;               /*IOMD*/
                case 0x98: /*Chip ID registers*/
//                rpclog("Read Chip ID1 %02X %02X\n",model,(model)?0x5B:0xD4);
                if (model==0) return 0x5B; /*ARM7500*/
                return 0xD4;               /*IOMD*/
                case 0x9C: return 0; /*Chip version*/
                case 0xA0: return iomd.mousex;
                case 0xA4: return -iomd.mousey;
                case 0xA8: return readmousedata(); /*Mouse data*/
                case 0xAC: return getmousestat(); /*Mouse control*/
                case 0x180: case 0x184: case 0x188: case 0x18C: return 0;
                case 0x194: /*Sound DMA stat*/
//                rpclog("Read Sound stat %02X\n",iomd.sndstat);
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
//        rpclog("Readmb %02X %08X\n",temp,PC);
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
//        flyback=0x80;
}

void dumpiomdregs()
{
        printf("IRQ STAT A %02X B %02X D %02X F %02X\n",iomd.stata,iomd.statb,iomd.statd,iomd.statf);
        printf("IRQ MASK A %02X B %02X D %02X F %02X\n",iomd.maska,iomd.maskb,iomd.maskd,iomd.maskf);
}
