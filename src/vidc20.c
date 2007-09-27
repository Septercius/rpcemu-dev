/*RPCemu v0.6 by Tom Walker
  VIDC20 emulation*/
#include <stdint.h>
#include <allegro.h>
#include "rpcemu.h"
#include "vidc20.h"
#include "keyboard.h"
#include "sound.h"
#include "mem.h"
#include "iomd.h"

int fullscreen=0;
int readflash = 0;
static BITMAP *b = NULL,*bs = NULL,*bs2 = NULL,*bs3=NULL,*bs4=NULL;
static int currentbuffer=1;
static int deskdepth = 0;
static int oldsx = 0,oldsy = 0;
static int drawcode = 0;

struct vidc_state {
        uint32_t vidcpal[0x104];
        int palindex;
        uint32_t hdsr,hcsr,hder;
        uint32_t vdsr,vcsr,vcer,vder;
        uint32_t b0,b1;
        int bit8;
} vidc;


static struct {
        struct
        {
                uint32_t r,g,b;
        } pal[256];
        uint16_t pal16lookup[65536];
        int palchange;
        uint32_t vpal[260];
        int curchange;
} cached_state;

static int thread_yh = 0, thread_yl = 0,thread_xs = 0,thread_ys = 0,thread_doublesize = 0;
static int blitready=0;
static int inblit=0;
static int skipnextblit=0;
void blitterthread()
{
        int xs=thread_xs;
        int ys=thread_ys;
        int yh=thread_yh;
        int yl=thread_yl;
        BITMAP *backbuf;
//        rpclog("Blitter thread - blitready %i\n",blitready);
        if (skipnextblit)
        {
                skipnextblit--;
                blitready=0;
                return;
        }
        if (!blitready)
        {
                return;
        }
        inblit=1;
        if (fullscreen)
        {
                switch (currentbuffer)
                {
                        case 0: backbuf=bs4; break;
                        case 1: backbuf=bs2; break;
                        case 2: backbuf=bs3; break;
                }
        }
        switch (thread_doublesize)
        {
                case 0: //case 1: case 2: case 3:
  //              case 3:
        if (!(fullscreen && stretchmode)) ys=yh-yl;
                if (fullscreen)
                {
                        if (stretchmode) blit(b,backbuf,0,0,(SCREEN_W-xs)>>1,((SCREEN_H-oldsy)>>1),xs,ys);
                        else             blit(b,backbuf,0,yl,(SCREEN_W-xs)>>1,yl+((SCREEN_H-oldsy)>>1),xs,ys);
                }
                else            blit(b,screen,0,yl,0,yl,xs,ys);
                break;
                case 1:
        ys=yh-yl;
                if (fullscreen) stretch_blit(b,backbuf,0,yl,xs,ys, (SCREEN_W-(xs<<1))>>1,yl+((SCREEN_H-oldsy)>>1),xs<<1,ys);
                else            stretch_blit(b,screen, 0,yl,xs,ys, 0,                    yl,                      xs<<1,ys);
                break;
                case 2:
                if (stretchmode)
                {
                        if (fullscreen) stretch_blit(b,backbuf,0,0,xs,ys,0,0,xs,(ys<<1)-1);
                        else            stretch_blit(b,screen, 0,0,xs,ys,0,0,xs,(ys<<1)-1);
                }
                else
                {
                        ys=yh-yl;
                        if (fullscreen) stretch_blit(b,backbuf,0,yl,xs,ys, (SCREEN_W-xs)>>1,(yl<<1)+((SCREEN_H-oldsy)>>1),xs,(ys<<1)-1);
                        else            stretch_blit(b,screen, 0,yl,xs,ys, 0,               yl<<1,                        xs,(ys<<1)-1);
                }
                break;
                case 3:
                if (stretchmode)
                {
                        if (fullscreen) stretch_blit(b,backbuf,0,0,xs,ys,(SCREEN_W-(xs<<1))>>1,((SCREEN_H-oldsy)>>1),xs<<1,(ys<<1)-1);
                        else            stretch_blit(b,screen, 0,0,xs,ys,0,0,xs<<1,(ys<<1)-1);
                }
                else
                {
                        ys=yh-yl;
                        if (fullscreen) stretch_blit(b,backbuf,0,yl,xs,ys, (SCREEN_W-(xs<<1))>>1,(yl<<1)+((SCREEN_H-oldsy)>>1),xs<<1,(ys<<1)-1);
                        else            stretch_blit(b,screen, 0,yl,xs,ys, 0,                    yl<<1,                        xs<<1,(ys<<1)-1);
                }
                break;
        }
        if (fullscreen)
        {
                switch (currentbuffer)
                {
                        case 0: request_video_bitmap(bs4); currentbuffer=1; break;
                        case 1: request_video_bitmap(bs2); currentbuffer=2; break;
                        case 2: request_video_bitmap(bs3); currentbuffer=0; break;
                }
        }
        inblit=0;
        blitready=0;
//        sleep(1);
}

void closevideo();

#define DEFAULT_W 640
#define DEFAULT_H 480

void initvideo()
{
        int depth;

#ifdef FULLSCREENALWAYS
       depth=16;
       set_color_depth(16);
       if (set_gfx_mode(GFX_AUTODETECT_FULLSCREEN,DEFAULT_W,DEFAULT_H,0,0))
       {
                set_color_depth(15);
                depth=15;
                if (set_gfx_mode(GFX_AUTODETECT_FULLSCREEN,DEFAULT_W,DEFAULT_H,0,0))
                {
                        printf("Failed to set video mode 640x480x16\n");
                        exit(-1);
                }
       }
       drawcode=16;
#else
        depth=deskdepth=desktop_color_depth();
        if (depth==16 || depth==15)
        {
                set_color_depth(15);
                depth=15;
                if (set_gfx_mode(GFX_AUTODETECT_WINDOWED,DEFAULT_W,DEFAULT_H,0,0))
                {
                        set_color_depth(16);
                        depth=16;
                        set_gfx_mode(GFX_AUTODETECT_WINDOWED,DEFAULT_W,DEFAULT_H,0,0);
                }
                drawcode=16;
        }
        else if (depth==32)
        {
                set_color_depth(depth);
                set_gfx_mode(GFX_AUTODETECT_WINDOWED,DEFAULT_W,DEFAULT_H,0,0);
                drawcode=32;
        }
        else
        {
                error("Your desktop must be set to either 16-bit or 32-bit colour to run RPCemu");
                exit(0);
        }
#endif

        oldsx=oldsy=-1;
        memset(&cached_state, 0, sizeof(cached_state));

}


int getxs()
{
        return vidc.hder-vidc.hdsr;
}
int getys()
{
        return vidc.vder-vidc.vdsr;
}

static const int fullresolutions[][2]=
{
        {320,200},
        {320,240},
        {400,300},
        {512,384},
        {640,400},
        {640,480},
        {800,600},
        {1024,768},
        {1280,800},
        {1280,1024},
        {1600,1200},
        {1920,1200},
        {-1,-1}
};

void resizedisplay(int x, int y)
{
        int c;
//        rpclog("Change mode to %ix%i\n",x,y);
        #ifdef FULLSCREENALWAYS
        fullscreen=1;
        #endif
        if (x<16) x=16;
        if (y<16) y=16;
        oldsx=x;
        oldsy=y;
        while (inblit || blitready) sleep(1);
        closevideo();
        if (fullscreen)
        {
                c=0;
                tryagain:
                while (fullresolutions[c][0]!=-1)
                {
                        if (fullresolutions[c][0]>=x && fullresolutions[c][1]>=y)
                           break;
                        c++;
                }
                if (fullresolutions[c][0]==-1) c--;
//                rpclog("Trying %ix%i\n",fullresolutions[c][0],fullresolutions[c][1]);
                if (set_gfx_mode(GFX_AUTODETECT_FULLSCREEN,fullresolutions[c][0],fullresolutions[c][1],0,0))
                {
                        if (fullresolutions[c+1][0]==-1) /*Reached the end of resolutions, go for something safe*/
                        {
//                                rpclog("Failed - falling back on 640x480\n");
                                set_gfx_mode(GFX_AUTODETECT_FULLSCREEN,640,480,0,0);
                        }
                        else
                        {
                                c++;
                                goto tryagain;
                        }
                }
#ifdef HARDWAREBLIT                
//                rpclog("Mode set\n");
                bs=create_video_bitmap(fullresolutions[c][0],fullresolutions[c][1]);
                bs2=create_video_bitmap(fullresolutions[c][0],fullresolutions[c][1]);
                bs3=create_video_bitmap(fullresolutions[c][0],fullresolutions[c][1]);
                bs4=create_video_bitmap(fullresolutions[c][0],fullresolutions[c][1]);
//                rpclog("%08X %08X\n",bs,bs2);
                clear(bs);
                clear(bs2);
                clear(bs3);
                clear(bs4);
                show_video_bitmap(bs4);
                currentbuffer=1;
                b=create_video_bitmap(x+16,y+16);
                if (!b) /*Video bitmaps unavailable for some reason*/
#endif
                   b=create_bitmap(x+16,y+16);
        }
        else
        {
                set_gfx_mode(GFX_AUTODETECT_WINDOWED,x,y,0,0);
                updatewindowsize(x,y);
                bs=create_video_bitmap(x,y);
                b=create_video_bitmap(x,y);
                if (!b) /*Video bitmaps unavailable for some reason*/
                   b=create_bitmap(x,y);
        }
        resetbuffer();
}

void closevideo()
{
//        rpclog("Calling closevideo()\n");
        if (b) destroy_bitmap(b);
        if (bs) destroy_bitmap(bs);
        if (bs2) destroy_bitmap(bs2);
        if (bs3) destroy_bitmap(bs3);
        if (bs4) destroy_bitmap(bs4);
        b = NULL;
        bs = NULL;
        bs2 = NULL;
        bs3 = NULL;
        bs4 = NULL;
}
        
void togglefullscreen(int fs)
{
        fullscreen=fs;
        oldsx=oldsy=-1;
        memset(dirtybuffer,1,512*4);
}

static const int xdiff[8]={8192,4096,2048,1024,512,512,256,256};

static void calccrc(unsigned short *crc, unsigned char byte)
{
        int i;
        for (i = 0; i < 8; i++) {
                if (*crc & 0x8000) {
                        *crc <<= 1;
                        if (!(byte & 0x80)) *crc ^= 0x1021;
                } else {
                        *crc <<= 1;
                        if (byte & 0x80) *crc ^= 0x1021;
                }
                byte <<= 1;
        }
}


void drawscr()
{
        static int ony,ocy;
        static int lastframeborder=0;
        int x,y,xx;
        int xs=vidc.hder-vidc.hdsr;
        int ys=vidc.vder-vidc.vdsr;
        int addr;
        int cx=vidc.hcsr-vidc.hdsr;
        int cy=vidc.vcsr-vidc.vdsr,ny=vidc.vcer-vidc.vcsr;
        int drawit=0,olddrawit=0;
        int yl=-1,yh=-1;
        int c;
        int doublesize=0;
        uint32_t temp;
        unsigned char *ramp;
        unsigned short temp16,*ramw;
        uint32_t *vidp=NULL;
        unsigned short *vidp16=NULL;
        int firstblock,lastblock;


        if (cached_state.palchange)
        {
                int i;
                for (i=0; i < 0x100; i++)
                {
                        cached_state.pal[i].r=makecol(vidc.vidcpal[i]&0xFF,0,0);
                        cached_state.pal[i].g=makecol(0,(vidc.vidcpal[i]>>8)&0xFF,0);
                        cached_state.pal[i].b=makecol(0,0,(vidc.vidcpal[i]>>16)&0xFF);
                }
                for (i=0; i < 0x104; i++)
                {
                        cached_state.vpal[i]=makecol(vidc.vidcpal[i]&0xFF,(vidc.vidcpal[i]>>8)&0xFF,(vidc.vidcpal[i]>>16)&0xFF);
                }
                if ((vidc.bit8 == 4) && (drawcode == 16))
                {
                        int i;
                        for (i=0;i<65536;i++)
                        {
                                cached_state.pal16lookup[i]=cached_state.pal[i&0xFF].r|cached_state.pal[(i>>4)&0xFF].g|cached_state.pal[i>>8].b;
                        }
                }
        }


        #ifdef BLITTER_THREAD
        while (blitready)
        {
/*                if (soundbufferfull)
                {
                        updatesoundbuffer();
                }*/
                sleep(0);
//                return;
        }
        #endif
//        rpclog("Draw screen\n");
        uint32_t iomd_vidstart = iomd.vidstart;
        uint32_t iomd_vidend = iomd.vidend;
        uint32_t iomd_vidinit = iomd.vidinit;
        unsigned char iomd_vidcr = iomd.vidcr;
//        rpclog("XS %i YS %i\n",xs,ys);
        if (xs<2) xs=2;
        if (ys<1) ys=480;
        #ifdef HARDWAREBLIT
        if (xs<=448 || (xs<=480 && ys<=352))
        {
                xs<<=1;
                doublesize=1;
        }
        if (ys<=352)
        {
                ys<<=1;
                doublesize|=2;
        }
        #endif
        if (ys!=oldsy || xs!=oldsx) resizedisplay(xs,ys);
        if (!(iomd_vidcr&0x20) || vidc.vdsr>vidc.vder)
        {
//return;
                lastframeborder=1;
                if (!dirtybuffer[0] && !cached_state.palchange) return;
                dirtybuffer[0]=0;
                cached_state.palchange=0;
                rectfill(b,0,0,xs,ys,cached_state.vpal[0x100]);
//                      printf("%i %i\n",xs,ys);
                blit(b,screen,0,0,0,0,xs,ys);
                return;
        }
        if (cached_state.palchange)
        {
                memset(dirtybuffer,1,512*4);
                cached_state.palchange=0;
        }

        if (doublesize&1) xs>>=1;
        if (doublesize&2) ys>>=1;
        if (lastframeborder)
        {
                lastframeborder=0;
                resetbuffer();
        }
        if (drawit) yl=0;
        
        if (mousehack)
        {
                static int ovcsr=0,ohcsr=0;
                getmousepos(&cx,&cy);
                if (cy!=ovcsr || cx!=ohcsr) cached_state.curchange=1;
                ovcsr=cy;
                ohcsr=cx;
//                printf("Mouse at %i,%i %i\n",cx,cy,ny);
        }
        
        x=y=c=0;
        firstblock=lastblock=-1;
        while (y<ys)
        {
                if (dirtybuffer[c++])
                {
                        lastblock=c;
                        if (firstblock==-1) firstblock=c;
                }
                x+=(xdiff[vidc.bit8]<<2);
                while (x>xs)
                {
                        x-=xs;
                        y++;
                }
        }
//        #if 0
        if (firstblock==-1 && !cached_state.curchange) 
        {
                unsigned short crc=0xFFFF;
                static uint32_t curcrc=0;
                /*Not looking good for screen redraw - check to see if cursor data has changed*/
                if (cinit&0x4000000) ramp=(unsigned char *)ram2;
                else                 ramp=(unsigned char *)ram;
                addr=(cinit&rammask);//>>2;
                temp=0;
                for (c=0;c<(ny<<3);c++)
                    calccrc(&crc, ramp[addr++]);
                /*If cursor data matches then no point redrawing screen - return*/
                if (crc==curcrc && skipblits)
                {
                        skipnextblit++;
                #ifdef BLITTER_THREAD
                        wakeupblitterthread();
                #endif
                        return;
                }
                curcrc=crc;
        }
        if (iomd_vidinit&0x10000000) ramp=ramb;
        else                         ramp=vramb;
        ramw=(unsigned short *)ramp;
//        #endif
        addr=iomd_vidinit&0x7FFFFF;
        cached_state.curchange=0;
//        rpclog("First block %i %08X last block %i %08X finished at %i %08X\n",firstblock,firstblock,lastblock,lastblock,c,c);
        x=y=0;
        drawit=dirtybuffer[addr>>12];
        if (drawit) dirtybuffer[addr>>12]--;
        if (drawit) yl=0;
        switch (drawcode)
        {
                case 16:
                switch (vidc.bit8)
                {
                        case 0: /*1 bpp*/
                        xs>>=1;
                        for (;y<ys;y++)
                        {
                                if (y<(ony+ocy) && (y>=(ocy-2)))
                                {
                                        drawit=1;
//                                        yh=y+8;
                                        if (yl==-1)
                                           yl=y;
                                }
                                if (drawit) 
                                {
                                        vidp=(uint32_t *)bmp_write_line(b,y);
                                        yh=y+1;
                                }
                                for (;x<xs;x+=64)
                                {
                                        if (drawit)
                                        {
                                                for (xx=0;xx<64;xx+=4)
                                                {
                                                                                                #ifdef _RPCEMU_BIG_ENDIAN
                                                        vidp[x+xx+3]=cached_state.vpal[ramp[addr]&1]     |(cached_state.vpal[(ramp[addr]>>1)&1]<<16);
                                                        vidp[x+xx+2]=cached_state.vpal[(ramp[addr]>>2)&1]|(cached_state.vpal[(ramp[addr]>>3)&1]<<16);
                                                        vidp[x+xx+1]=cached_state.vpal[(ramp[addr]>>4)&1]|(cached_state.vpal[(ramp[addr]>>5)&1]<<16);
                                                        vidp[x+xx]=  cached_state.vpal[(ramp[addr]>>6)&1]|(cached_state.vpal[(ramp[addr]>>7)&1]<<16);
                                                                                                #else
                                                        vidp[x+xx]=  cached_state.vpal[ramp[addr]&1]     |(cached_state.vpal[(ramp[addr]>>1)&1]<<16);
                                                        vidp[x+xx+1]=cached_state.vpal[(ramp[addr]>>2)&1]|(cached_state.vpal[(ramp[addr]>>3)&1]<<16);
                                                        vidp[x+xx+2]=cached_state.vpal[(ramp[addr]>>4)&1]|(cached_state.vpal[(ramp[addr]>>5)&1]<<16);
                                                        vidp[x+xx+3]=cached_state.vpal[(ramp[addr]>>6)&1]|(cached_state.vpal[(ramp[addr]>>7)&1]<<16);
                                                                                                #endif
                                                        addr++;
                                                }
                                        }
                                        else
                                           addr+=16;
                                        if (addr==(int)iomd_vidend) addr=iomd_vidstart;
                                        if (!(addr&0xFFF))
                                        {
                                                if (!drawit && dirtybuffer[(addr>>12)]) vidp=(uint32_t *)bmp_write_line(b,y);
                                                drawit=dirtybuffer[(addr>>12)];
                                                if (drawit) dirtybuffer[(addr>>12)]--;
                                                if (y<(ony+ocy) && (y>=(ocy-2))) drawit=1;
                                                if (drawit) yh=y+8;
                                                if (yl==-1 && drawit)
                                                   yl=y;
                                        }
                                }
                                x=0;
                        }
                        xs<<=1;
                        break;
                        case 1: /*2 bpp*/
                        xs>>=1;
                        for (y=0;y<ys;y++)
                        {
                                if (y<(ony+ocy) && (y>=(ocy-2)))
                                {
                                        drawit=1;
//                                        yh=y+8;
                                        if (yl==-1)
                                           yl=y;
                                }
                                if (drawit) 
                                {
                                        vidp=(uint32_t *)bmp_write_line(b,y);
                                        yh=y+1;
                                }
                                for (x=0;x<xs;x+=32)
                                {
                                        if (drawit)
                                        {
                                                for (xx=0;xx<32;xx+=2)
                                                {
                                                        vidp[x+xx]=cached_state.vpal[ramp[addr]&3]|(cached_state.vpal[(ramp[addr]>>2)&3]<<16);
                                                        vidp[x+xx+1]=cached_state.vpal[(ramp[addr]>>4)&3]|(cached_state.vpal[(ramp[addr]>>6)&3]<<16);
                                                        addr++;
                                                }
                                        }
                                        else
                                           addr+=16;
                                        if (addr==(int)iomd_vidend) addr=iomd_vidstart;
                                        if (!(addr&0xFFF))
                                        {
                                                if (!drawit && dirtybuffer[(addr>>12)]) vidp=(uint32_t *)bmp_write_line(b,y);
                                                drawit=dirtybuffer[(addr>>12)];
                                                if (drawit) dirtybuffer[(addr>>12)]--;
                                                if (y<(ony+ocy) && (y>=(ocy-2))) drawit=1;
                                                if (drawit) yh=y+8;
                                                if (yl==-1 && drawit)
                                                   yl=y;
                                        }
                                }
                        }
                        xs<<=1;
                        break;
                        case 2: /*4 bpp*/
                        xs>>=1;
                        for (y=0;y<ys;y++)
                        {
                                if (y<(ony+ocy) && (y>=(ocy-2)))                                
                                {
                                        drawit=1;
//                                        yh=y+8;
                                        if (yl==-1)
                                           yl=y;
                                }
                                if (drawit) 
                                {
                                        vidp=(uint32_t *)bmp_write_line(b,y);
                                        yh=y+1;
                                }
//                                rpclog("Line %i drawit %i addr %06X\n",y,drawit,addr);
                                for (x=0;x<xs;x+=16)
                                {
                                        if (drawit)
                                        {
                                                for (xx=0;xx<16;xx+=4)
                                                {
                                                                                                #ifdef _RPCEMU_BIG_ENDIAN
//                                                                                                              if (!x && !y) printf("%02X %04X\n",ramp[addr],vpal[ramp[addr&0xF]]);
                                                        vidp[x+xx+3]=cached_state.vpal[ramp[addr]>>4]|(cached_state.vpal[ramp[addr]&0xF]<<16);
                                                        vidp[x+xx+2]=cached_state.vpal[ramp[addr+1]>>4]|(cached_state.vpal[ramp[addr+1]&0xF]<<16);
                                                        vidp[x+xx+1]=cached_state.vpal[ramp[addr+2]>>4]|(cached_state.vpal[ramp[addr+2]&0xF]<<16);
                                                        vidp[x+xx]=cached_state.vpal[ramp[addr+3]>>4]|(cached_state.vpal[ramp[addr+3]&0xF]<<16);
                                                                                                #else
                                                        vidp[x+xx]=cached_state.vpal[ramp[addr]&0xF]|(cached_state.vpal[ramp[addr]>>4]<<16);
                                                        vidp[x+xx+1]=cached_state.vpal[ramp[addr+1]&0xF]|(cached_state.vpal[ramp[addr+1]>>4]<<16);
                                                        vidp[x+xx+2]=cached_state.vpal[ramp[addr+2]&0xF]|(cached_state.vpal[ramp[addr+2]>>4]<<16);
                                                        vidp[x+xx+3]=cached_state.vpal[ramp[addr+3]&0xF]|(cached_state.vpal[ramp[addr+3]>>4]<<16);                                                                                                                                                                       
                                                                                                #endif
                                                        addr+=4;
                                                }
                                        }
                                        else
                                           addr+=16;
                                        if (addr==(int)iomd_vidend)
                                        {
                                                addr=iomd_vidstart;
                                        }
                                        if (!(addr&0xFFF))
                                        {
                                                if (!drawit && dirtybuffer[(addr>>12)]) vidp=(uint32_t *)bmp_write_line(b,y);
                                                drawit=dirtybuffer[(addr>>12)];
                                                if (drawit) dirtybuffer[(addr>>12)]--;
//                                                rpclog("Hit 4k boundary %06X %i,%i %i\n",addr,x,y,drawit);
                                                if (y<(ony+ocy) && (y>=(ocy-2))) drawit=1;
                                                if (drawit) yh=y+8;
                                                if (yl==-1 && drawit)
                                                   yl=y;
                                        }
                                }
                        }
                        xs<<=1;
                        break;
                        case 3: /*8 bpp*/
                        xs>>=1;
//                        rpclog("Start %08X End %08X Init %08X\n",iomd_vidstart,iomd_vidend,addr);
                        for (;y<ys;y++)
                        {
                                if (y<(ony+ocy) && (y>=(ocy-1)))
                                {
                                        drawit=1;
//                                        yh=y+8;
                                        if (yl==-1)
                                           yl=y;
                                }
                                if (drawit)
                                {
                                        vidp=(uint32_t *)bmp_write_line(b,y);
                                        yh=y+1;
                                }
                                for (;x<xs;x+=8)
                                {
                                        if (drawit)
                                        {
                                                for (xx=0;xx<8;xx+=2)
                                                {
#ifdef _RPCEMU_BIG_ENDIAN
                                                        vidp[x+xx+1]=cached_state.vpal[ramp[addr+1]&0xFF]|(cached_state.vpal[ramp[addr]&0xFF]<<16);
                                                        vidp[x+xx]=cached_state.vpal[ramp[addr+3]&0xFF]|(cached_state.vpal[ramp[addr+2]&0xFF]<<16);
#else
                                                        vidp[x+xx]=cached_state.vpal[ramp[addr]&0xFF]|(cached_state.vpal[ramp[addr+1]&0xFF]<<16);
                                                        vidp[x+xx+1]=cached_state.vpal[ramp[addr+2]&0xFF]|(cached_state.vpal[ramp[addr+3]&0xFF]<<16);
#endif
                                                        addr+=4;
                                                }
                                        }
                                        else
                                           addr+=16;
                                        if (addr==(int)iomd_vidend)
                                           addr=iomd_vidstart;
                                        if (!(addr&0xFFF))
                                        {
                                                olddrawit=drawit;
                                                drawit=dirtybuffer[(addr>>12)];
                                                if (drawit)
                                                {
                                                        dirtybuffer[(addr>>12)]--;
                                                        yh=y+1;
                                                        if (yl==-1) yl=y;
                                                }
                                                if (y<(ony+ocy) && (y>=(ocy-1))) drawit=1;
                                                if (drawit && !olddrawit) vidp=(uint32_t *)bmp_write_line(b,y);
                                        }
/*                                        if (!(addr&0xFFF))
                                        {
                                                if (!drawit && dirtybuffer[(addr>>12)]) vidp=bmp_write_line(b,y);
                                                drawit=dirtybuffer[(addr>>12)];
                                                if (drawit) dirtybuffer[(addr>>12)]--;
                                                if (y<(ony+ocy) && (y>=(ocy-2))) drawit=1;
                                                if (drawit) yh=y+8;
                                                if (yl==-1 && drawit)
                                                   yl=y;
                                        }*/
                                }
                                x=0;
                        }
                        xs<<=1;
  //                      rpclog("Yl %i Yh %i\n",yl,yh);
                        break;
                        case 4: /*16 bpp*/
                #if 0
                        xs>>=1;
                        y=x=0;
                        addr>>=1;
                        xxx=addr&0x7FF;
                        while (y<ys)
                        {
                                drawit=dirtybuffer[addr>>11];
                                if (y<(ony+ocy) && (y>=(ocy-2)))
                                   drawit=1;
                                if (drawit)
                                {
                                        dirtybuffer[addr>>11]=0;
                                        vidp=bmp_write_line(b,y);
                                        if (yl==-1) yl=y;
                                        for (c=(xxx>>1);c<2048;c+=8)
                                        {
                                                temp16=ramw[addr];
                                                temp=cached_state.pal[temp16&0xFF].r|cached_state.pal[(temp16>>4)&0xFF].g|cached_state.pal[temp16>>8].b;
                                                temp16=ramw[addr+1];
                                                temp|=(cached_state.pal[temp16&0xFF].r|cached_state.pal[(temp16>>4)&0xFF].g|cached_state.pal[temp16>>8].b)<<16;
                                                vidp[x++]=temp;
                                                temp16=ramw[addr+2];
                                                temp=cached_state.pal[temp16&0xFF].r|cached_state.pal[(temp16>>4)&0xFF].g|cached_state.pal[temp16>>8].b;
                                                temp16=ramw[addr+3];
                                                temp|=(cached_state.pal[temp16&0xFF].r|cached_state.pal[(temp16>>4)&0xFF].g|cached_state.pal[temp16>>8].b)<<16;
                                                vidp[x++]=temp;
                                                temp16=ramw[addr+4];
                                                temp=cached_state.pal[temp16&0xFF].r|cached_state.pal[(temp16>>4)&0xFF].g|cached_state.pal[temp16>>8].b;
                                                temp16=ramw[addr+5];
                                                temp|=(cached_state.pal[temp16&0xFF].r|cached_state.pal[(temp16>>4)&0xFF].g|cached_state.pal[temp16>>8].b)<<16;
                                                vidp[x++]=temp;
                                                temp16=ramw[addr+6];
                                                temp=cached_state.pal[temp16&0xFF].r|cached_state.pal[(temp16>>4)&0xFF].g|cached_state.pal[temp16>>8].b;
                                                temp16=ramw[addr+7];
                                                temp|=(cached_state.pal[temp16&0xFF].r|cached_state.pal[(temp16>>4)&0xFF].g|cached_state.pal[temp16>>8].b)<<16;
                                                vidp[x++]=temp;
/*                                                vidp[x+1]=cached_state.pal[temp16&0xFF].r|cached_state.pal[(temp16>>4)&0xFF].g|cached_state.pal[temp16>>8].b;
                                                temp16=ramw[addr+2];
                                                vidp[x+2]=cached_state.pal[temp16&0xFF].r|cached_state.pal[(temp16>>4)&0xFF].g|cached_state.pal[temp16>>8].b;
                                                temp16=ramw[addr+3];
                                                vidp[x+3]=cached_state.pal[temp16&0xFF].r|cached_state.pal[(temp16>>4)&0xFF].g|cached_state.pal[temp16>>8].b;
                                                temp16=ramw[addr+4];
                                                vidp[x+4]=cached_state.pal[temp16&0xFF].r|cached_state.pal[(temp16>>4)&0xFF].g|cached_state.pal[temp16>>8].b;
                                                temp16=ramw[addr+5];
                                                vidp[x+5]=cached_state.pal[temp16&0xFF].r|cached_state.pal[(temp16>>4)&0xFF].g|cached_state.pal[temp16>>8].b;
                                                temp16=ramw[addr+6];
                                                vidp[x+6]=cached_state.pal[temp16&0xFF].r|cached_state.pal[(temp16>>4)&0xFF].g|cached_state.pal[temp16>>8].b;
                                                temp16=ramw[addr+7];
                                                vidp[x+7]=cached_state.pal[temp16&0xFF].r|cached_state.pal[(temp16>>4)&0xFF].g|cached_state.pal[temp16>>8].b;*/
                                                addr+=8;
//                                                x+=8;
                                                if (x>=xs)
                                                {
                                                        x=0;
                                                        y++;
                                                        vidp=bmp_write_line(b,y);
                                                }
                                        }
                                        xxx=0;
                                        yh=y+1;
                                }
                                else
                                {
                                        if (xxx) x+=((2048-(xxx>>1))>>1);
                                        else     x+=(2048>>1);
                                        xxx=0;
                                        while (x>xs)
                                        {
                                                x-=xs;
                                                y++;
                                        }
                                        addr+=2048;
                                }
                        }
                        xs<<=1;
                #endif
                        xs>>=1;
                        for (y=0;y<ys;y++)
                        {
                                if (y<(ony+ocy) && (y>=(ocy-1)))
                                {
                                        drawit=1;
//                                        yh=y+8;
                                        if (yl==-1)
                                           yl=y;
                                }
                                if (drawit) 
                                {
                                        vidp=(uint32_t *)bmp_write_line(b,y);
                                        yh=y+1;
                                }
                                for (x=0;x<xs;x+=4)
                                {
                                        if (drawit)
                                        {
                                                addr>>=1;
                                                vidp[x]=cached_state.pal16lookup[ramw[addr]]|(cached_state.pal16lookup[ramw[addr+1]]<<16);
                                                vidp[x+1]=cached_state.pal16lookup[ramw[addr+2]]|(cached_state.pal16lookup[ramw[addr+3]]<<16);
                                                vidp[x+2]=cached_state.pal16lookup[ramw[addr+4]]|(cached_state.pal16lookup[ramw[addr+5]]<<16);
                                                vidp[x+3]=cached_state.pal16lookup[ramw[addr+6]]|(cached_state.pal16lookup[ramw[addr+7]]<<16);
/*                                                temp16=ramw[addr];
                                                temp=cached_state.pal[temp16&0xFF].r|cached_state.pal[(temp16>>4)&0xFF].g|cached_state.pal[temp16>>8].b;
                                                temp16=ramw[addr+1];
                                                vidp[x]=((cached_state.pal[temp16&0xFF].r|cached_state.pal[(temp16>>4)&0xFF].g|cached_state.pal[temp16>>8].b)<<16)|temp;
                                                temp16=ramw[addr+2];
                                                temp=cached_state.pal[temp16&0xFF].r|cached_state.pal[(temp16>>4)&0xFF].g|cached_state.pal[temp16>>8].b;
                                                temp16=ramw[addr+3];
                                                vidp[x+1]=((cached_state.pal[temp16&0xFF].r|cached_state.pal[(temp16>>4)&0xFF].g|cached_state.pal[temp16>>8].b)<<16)|temp;
                                                temp16=ramw[addr+4];
                                                temp=cached_state.pal[temp16&0xFF].r|cached_state.pal[(temp16>>4)&0xFF].g|cached_state.pal[temp16>>8].b;
                                                temp16=ramw[addr+5];
                                                vidp[x+2]=((cached_state.pal[temp16&0xFF].r|cached_state.pal[(temp16>>4)&0xFF].g|cached_state.pal[temp16>>8].b)<<16)|temp;
                                                temp16=ramw[addr+6];
                                                temp=cached_state.pal[temp16&0xFF].r|cached_state.pal[(temp16>>4)&0xFF].g|cached_state.pal[temp16>>8].b;
                                                temp16=ramw[addr+7];
                                                vidp[x+3]=((cached_state.pal[temp16&0xFF].r|cached_state.pal[(temp16>>4)&0xFF].g|cached_state.pal[temp16>>8].b)<<16)|temp;*/
                                                addr<<=1;
                                                addr+=16;
                                        }
                                        else
                                           addr+=16;
                                        if (addr==(int)iomd_vidend) addr=iomd_vidstart;
                                        if (!(addr&0xFFF))
                                        {
                                                olddrawit=drawit;
                                                drawit=dirtybuffer[(addr>>12)];
                                                if (drawit) 
                                                {
                                                        dirtybuffer[(addr>>12)]--;
                                                        yh=y+1;
                                                        if (yl==-1) yl=y;
                                                }
                                                if (y<(ony+ocy) && (y>=(ocy-1))) drawit=1;                                                
                                                if (drawit && !olddrawit) vidp=(uint32_t *)bmp_write_line(b,y);
                                                if ((addr>>12)==lastblock && y>(ony+ocy))
                                                   y=x=65536;
                                        }
                                }
                        }
                        xs<<=1;
                        break;
                        case 6: /*32 bpp*/
//                        textprintf(b,font,0,8,makecol(255,255,255),"%i %i %i %i  ",drawit,addr>>10,xs,ys);
//                        textprintf(screen,font,0,8,makecol(255,255,255),"%i %i %i %i  ",drawit,addr>>10,xs,ys);
                        for (y=0;y<ys;y++)
                        {
                                if (y<(ony+ocy) && (y>=(ocy-2)))
                                {
                                        drawit=1;
//                                        yh=y+8;
                                        if (yl==-1)
                                           yl=y;
                                }
                                if (drawit) 
                                {
                                        vidp16=(unsigned short *)bmp_write_line(b,y);
                                        yh=y+1;
                                }
                                for (x=0;x<xs;x+=4)
                                {
                                        if (drawit)
                                        {
                                                for (xx=0;xx<4;xx++)
                                                {
                                                        //VIDC20 pixel format is  xxxx xxxx BBBB BBBB GGGG GGGG RRRR RRRR
                                                        //Windows pixel format is                     RRRR RGGG GGGB BBBB
                                                        temp=ramp[addr]|(ramp[addr+1]<<8)|(ramp[addr+2]<<16)|(ramp[addr+3]<<24);
                                                        vidp16[x+xx]=cached_state.pal[temp&0xFF].r|cached_state.pal[(temp>>8)&0xFF].g|cached_state.pal[(temp>>16)&0xFF].b;
                                                        addr+=4;
                                                }
                                        }
                                        else
                                           addr+=16;
                                        if (addr==(int)iomd_vidend) addr=iomd_vidstart;
                                        if (!(addr&0xFFF))
                                        {
                                                if (!drawit && dirtybuffer[(addr>>12)]) vidp16=(unsigned short *)bmp_write_line(b,y);
                                                drawit=dirtybuffer[(addr>>12)];
//                                                drawit=1;
                                                if (y<(ony+ocy) && (y>=(ocy-2))) drawit=1;
                                                dirtybuffer[(addr>>12)]--;
                                                if (drawit) yh=y+8;
                                                if (yl==-1 && drawit)
                                                   yl=y;
                                        }
                                }
                        }
//                        textprintf(b,font,0,16,makecol(255,255,255),"%i %i   ",yl,yh);
//                        textprintf(screen,font,0,16,makecol(255,255,255),"%i %i   ",yl,yh);
                        break;
                        default:
                        error("Bad BPP %i\n",vidc.bit8);
                        exit(-1);
                }
                break;
                case 32:
                switch (vidc.bit8)
                {
                        case 0: /*1 bpp*/
                        for (y=0;y<ys;y++)
                        {
                                if (y<(ony+ocy) && (y>=(ocy-2)))
                                {
                                        drawit=1;
                                        yh=y+8;
                                        if (yl==-1)
                                           yl=y;
                                }
                                if (drawit) 
                                {
                                        vidp=(uint32_t *)bmp_write_line(b,y);
                                        yh=y+1;
                                }
                                for (x=0;x<xs;x+=128)
                                {
                                        if (drawit)
                                        {
                                                for (xx=0;xx<128;xx+=8)
                                                {
                                                        vidp[x+xx]=cached_state.vpal[ramp[addr]&1];
                                                        vidp[x+xx+1]=cached_state.vpal[(ramp[addr]>>1)&1];
                                                        vidp[x+xx+2]=cached_state.vpal[(ramp[addr]>>2)&1];
                                                        vidp[x+xx+3]=cached_state.vpal[(ramp[addr]>>3)&1];
                                                        vidp[x+xx+4]=cached_state.vpal[(ramp[addr]>>4)&1];
                                                        vidp[x+xx+5]=cached_state.vpal[(ramp[addr]>>5)&1];
                                                        vidp[x+xx+6]=cached_state.vpal[(ramp[addr]>>6)&1];
                                                        vidp[x+xx+7]=cached_state.vpal[(ramp[addr]>>7)&1];
                                                        addr++;
                                                }
                                        }
                                        else
                                           addr+=16;
                                        if (addr==(int)iomd_vidend) addr=iomd_vidstart;
                                        if (!(addr&0xFFF))
                                        {
                                                if (!drawit && dirtybuffer[(addr>>12)]) vidp=(uint32_t *)bmp_write_line(b,y);
                                                drawit=dirtybuffer[(addr>>12)];
                                                if (y<(ony+ocy) && (y>=(ocy-2))) drawit=1;
                                                dirtybuffer[(addr>>12)]=0;
                                                if (drawit) yh=y+8;
                                                if (yl==-1 && drawit)
                                                   yl=y;
                                        }
                                }
                        }
                        break;
                        case 1: /*2 bpp*/
                        for (y=0;y<ys;y++)
                        {
                                if (y<(ony+ocy) && (y>=(ocy-2)))
                                {
                                        drawit=1;
                                        yh=y+8;
                                        if (yl==-1)
                                           yl=y;
                                }
                                if (drawit) 
                                {
                                        vidp=(uint32_t *)bmp_write_line(b,y);
                                        yh=y+1;
                                }
                                for (x=0;x<xs;x+=64)
                                {
                                        if (drawit)
                                        {
                                                for (xx=0;xx<64;xx+=4)
                                                {
                                                        vidp[x+xx]=cached_state.vpal[ramp[addr]&3];
                                                        vidp[x+xx+1]=cached_state.vpal[(ramp[addr]>>2)&3];
                                                        vidp[x+xx+2]=cached_state.vpal[(ramp[addr]>>4)&3];
                                                        vidp[x+xx+3]=cached_state.vpal[(ramp[addr]>>6)&3];
                                                        addr++;
                                                }
                                        }
                                        else
                                           addr+=16;
                                        if (addr==(int)iomd_vidend) addr=iomd_vidstart;
                                        if (!(addr&0xFFF))
                                        {
                                                if (!drawit && dirtybuffer[(addr>>12)]) vidp=(uint32_t *)bmp_write_line(b,y);
                                                drawit=dirtybuffer[(addr>>12)];
                                                if (y<(ony+ocy) && (y>=(ocy-2))) drawit=1;
                                                dirtybuffer[(addr>>12)]=0;
                                                if (drawit) yh=y+8;
                                                if (yl==-1 && drawit)
                                                   yl=y;
                                        }
                                }
                        }
                        break;
                        case 2: /*4 bpp*/
                        for (y=0;y<ys;y++)
                        {
                                if (y<(ony+ocy) && (y>=(ocy-2)))
                                {
                                        drawit=1;
                                        yh=y+8;
                                        if (yl==-1)
                                           yl=y;
                                }
                                if (drawit) 
                                {
                                        vidp=(uint32_t *)bmp_write_line(b,y);
                                        yh=y+1;
                                }
                                for (x=0;x<xs;x+=32)
                                {
                                        if (drawit)
                                        {
                                                for (xx=0;xx<32;xx+=8)
                                                {
                                                                                                #ifdef _RPCEMU_BIG_ENDIAN
                                                        vidp[x+xx]=cached_state.vpal[ramp[addr+3]&0xF];
                                                        vidp[x+xx+1]=cached_state.vpal[(ramp[addr+3]>>4)&0xF];
                                                        vidp[x+xx+2]=cached_state.vpal[ramp[addr+2]&0xF];
                                                        vidp[x+xx+3]=cached_state.vpal[(ramp[addr+2]>>4)&0xF];
                                                        vidp[x+xx+4]=cached_state.vpal[ramp[addr+1]&0xF];
                                                        vidp[x+xx+5]=cached_state.vpal[(ramp[addr+1]>>4)&0xF];
                                                        vidp[x+xx+6]=cached_state.vpal[ramp[addr]&0xF];
                                                        vidp[x+xx+7]=cached_state.vpal[(ramp[addr]>>4)&0xF];
                                                                                                #else
                                                        vidp[x+xx]=cached_state.vpal[ramp[addr]&0xF];
                                                        vidp[x+xx+1]=cached_state.vpal[(ramp[addr]>>4)&0xF];
                                                        vidp[x+xx+2]=cached_state.vpal[ramp[addr+1]&0xF];
                                                        vidp[x+xx+3]=cached_state.vpal[(ramp[addr+1]>>4)&0xF];
                                                        vidp[x+xx+4]=cached_state.vpal[ramp[addr+2]&0xF];
                                                        vidp[x+xx+5]=cached_state.vpal[(ramp[addr+2]>>4)&0xF];
                                                        vidp[x+xx+6]=cached_state.vpal[ramp[addr+3]&0xF];
                                                        vidp[x+xx+7]=cached_state.vpal[(ramp[addr+3]>>4)&0xF];
                                                                                                #endif
                                                        addr+=4;
                                                }
                                        }
                                        else
                                           addr+=16;
                                        if (addr==(int)iomd_vidend) addr=iomd_vidstart;
                                        if (!(addr&0xFFF))
                                        {
                                                if (!drawit && dirtybuffer[(addr>>12)]) vidp=(uint32_t *)bmp_write_line(b,y);
                                                drawit=dirtybuffer[(addr>>12)];
                                                if (y<(ony+ocy) && (y>=(ocy-2))) drawit=1;
                                                dirtybuffer[(addr>>12)]=0;
                                                if (drawit) yh=y+8;
                                                if (yl==-1 && drawit)
                                                   yl=y;
                                        }
                                }
                        }
                        break;
                        case 3: /*8 bpp*/
                        for (y=0;y<ys;y++)
                        {
                                if (y<(ony+ocy) && (y>=(ocy-2)))
                                {
                                        drawit=1;
                                        yh=y+8;
                                        if (yl==-1)
                                           yl=y;
                                }
                                if (drawit) 
                                {
                                        vidp=(uint32_t *)bmp_write_line(b,y);
                                        yh=y+1;
                                }
                                for (x=0;x<xs;x+=16)
                                {
                                        if (drawit)
                                        {
                                                for (xx=0;xx<16;xx+=4)
                                                {
                                                        vidp[x+xx]=cached_state.vpal[ramp[addr]&0xFF];
                                                        vidp[x+xx+1]=cached_state.vpal[ramp[addr+1]&0xFF];                                                        
                                                        vidp[x+xx+2]=cached_state.vpal[ramp[addr+2]&0xFF];                                                        
                                                        vidp[x+xx+3]=cached_state.vpal[ramp[addr+3]&0xFF];                                                                                                                
                                                        addr+=4;
                                                }
                                        }
                                        else
                                           addr+=16;
                                        if (addr==(int)iomd_vidend) addr=iomd_vidstart;
                                        if (!(addr&0xFFF))
                                        {
                                                if (!drawit && dirtybuffer[(addr>>12)]) vidp=(uint32_t *)bmp_write_line(b,y);
                                                drawit=dirtybuffer[(addr>>12)];
                                                if (y<(ony+ocy) && (y>=(ocy-2))) drawit=1;
                                                dirtybuffer[(addr>>12)]=0;
                                                if (drawit) yh=y+8;
                                                if (yl==-1 && drawit)
                                                   yl=y;
                                        }
                                }
                        }
                        break;
                        case 4: /*16 bpp*/
                        for (y=0;y<ys;y++)
                        {
                                if (y<(ony+ocy) && (y>=(ocy-2)))
                                {
                                        drawit=1;
                                        yh=y+8;
                                        if (yl==-1)
                                           yl=y;
                                }
                                if (drawit) 
                                {
                                        vidp=(uint32_t *)bmp_write_line(b,y);
                                        yh=y+1;
                                }
                                for (x=0;x<xs;x+=8)
                                {
                                        if (drawit)
                                        {
                                                for (xx=0;xx<8;xx+=2)
                                                {
                                                        /*VIDC20 format :                      xBBB BBGG GGGR RRRR
                                                          Windows format : xxxx xxxx RRRR RRRR GGGG GGGG BBBB BBBB*/
                                                        temp16=ramp[addr]|(ramp[addr+1]<<8);
                                                        vidp[x+xx]=cached_state.pal[temp16&0xFF].r|cached_state.pal[(temp16>>4)&0xFF].g|cached_state.pal[(temp16>>8)&0xFF].b;
                                                        temp16=ramp[addr+2]|(ramp[addr+3]<<8);
                                                        vidp[x+xx+1]=cached_state.pal[temp16&0xFF].r|cached_state.pal[(temp16>>4)&0xFF].g|cached_state.pal[(temp16>>8)&0xFF].b;
                                                        addr+=4;
                                                }
                                        }
                                        else
                                           addr+=16;
                                        if (addr==(int)iomd_vidend) addr=iomd_vidstart;
                                        if (!(addr&0xFFF))
                                        {
                                                if (!drawit && dirtybuffer[(addr>>12)]) vidp=(uint32_t *)bmp_write_line(b,y);
                                                drawit=dirtybuffer[(addr>>12)];
                                                if (y<(ony+ocy) && (y>=(ocy-2))) drawit=1;
                                                dirtybuffer[(addr>>12)]=0;
                                                if (drawit) yh=y+8;
                                                if (yl==-1 && drawit)
                                                   yl=y;
                                        }
                                }
                        }
                        break;
                        case 6: /*32 bpp*/
                        for (y=0;y<ys;y++)
                        {
                                if (y<(ony+ocy) && (y>=(ocy-2)))
                                {
                                        drawit=1;
                                        yh=y+8;
                                        if (yl==-1)
                                           yl=y;
                                }
                                if (drawit) 
                                {
                                        vidp=(uint32_t *)bmp_write_line(b,y);
                                        yh=y+1;
                                }
                                for (x=0;x<xs;x+=4)
                                {
                                        if (drawit)
                                        {
                                                for (xx=0;xx<4;xx++)
                                                {
                                                        vidp[x+xx]=cached_state.pal[ramp[addr]].r|cached_state.pal[ramp[addr+1]].g|cached_state.pal[ramp[addr+2]].b;
                                                        addr+=4;
                                                }
                                        }
                                        else
                                           addr+=16;
                                        if (addr==(int)iomd_vidend) addr=iomd_vidstart;
                                        if (!(addr&0xFFF))
                                        {
                                                if (!drawit && dirtybuffer[(addr>>12)]) vidp=(uint32_t *)bmp_write_line(b,y);
                                                drawit=dirtybuffer[(addr>>12)];
                                                if (y<(ony+ocy) && (y>=(ocy-2))) drawit=1;
                                                dirtybuffer[(addr>>12)]=0;
                                                if (drawit) yh=y+8;
                                                if (yl==-1 && drawit)
                                                   yl=y;
                                        }
                                }
                        }
                        break;
                        default:
                        error("Bad BPP %i\n",vidc.bit8);
                        exit(-1);
                }
        }
/*                if (soundbufferfull)
                {
                        updatesoundbuffer();
                }*/
        if (ny>1)
        {
                if (cinit&0x4000000) ramp=(unsigned char *)ram2;
                else                 ramp=(unsigned char *)ram;
                addr=cinit&rammask;
//                printf("Mouse now at %i,%i\n",cx,cy);
                switch (drawcode)
                {
                        case 16:
                        for (y=0;y<ny;y++)
                        {
                                if ((y+cy)>=ys) break;
                                if ((y+cy)>=0)
                                {
                                        vidp16=(unsigned short *)bmp_write_line(b,y+cy);
                                        for (x=0;x<32;x+=4)
                                        {
                                                                                #ifdef _RPCEMU_BIG_ENDIAN
                                                                                                addr^=3;
                                                                                #endif
                                                if ((x+cx)>=0   && ramp[addr]&3)      vidp16[x+cx]=cached_state.vpal[(ramp[addr]&3)|0x100];
                                                if ((x+cx+1)>=0 && (ramp[addr]>>2)&3) vidp16[x+cx+1]=cached_state.vpal[((ramp[addr]>>2)&3)|0x100];
                                                if ((x+cx+2)>=0 && (ramp[addr]>>4)&3) vidp16[x+cx+2]=cached_state.vpal[((ramp[addr]>>4)&3)|0x100];
                                                if ((x+cx+3)>=0 && (ramp[addr]>>6)&3) vidp16[x+cx+3]=cached_state.vpal[((ramp[addr]>>6)&3)|0x100];
                                                                                #ifdef _RPCEMU_BIG_ENDIAN
                                                                                                addr^=3;
                                                                                #endif
                                                addr++;
                                        }
                                }
                        }
                        break;
                        case 32:
                        for (y=0;y<ny;y++)
                        {
                                if ((y+cy)>=ys) break;
                                if ((y+cy)>=0)
                                {
                                        vidp=(uint32_t *)bmp_write_line(b,y+cy);
                                        for (x=0;x<32;x+=4)
                                        {
                                                                                #ifdef _RPCEMU_BIG_ENDIAN
                                                                                                addr^=3;
                                                                                #endif
                                                if ((x+cx)>=0   && ramp[addr]&3)      vidp[x+cx]=cached_state.vpal[(ramp[addr]&3)|0x100];
                                                if ((x+cx+1)>=0 && (ramp[addr]>>2)&3) vidp[x+cx+1]=cached_state.vpal[((ramp[addr]>>2)&3)|0x100];
                                                if ((x+cx+2)>=0 && (ramp[addr]>>4)&3) vidp[x+cx+2]=cached_state.vpal[((ramp[addr]>>4)&3)|0x100];
                                                if ((x+cx+3)>=0 && (ramp[addr]>>6)&3) vidp[x+cx+3]=cached_state.vpal[((ramp[addr]>>6)&3)|0x100];
                                                                                #ifdef _RPCEMU_BIG_ENDIAN
                                                                                                addr^=3;
                                                                                #endif
                                                addr++;
                                        }
                                }
                        }
                        break;
                }
                if (yl>cy) yl=cy;
                if (yl==-1) yl=cy;
                if (cy<0) yl=0;
                if (yh<(ny+cy)) yh=ny+cy;
        }
        ony=ny;
        ocy=cy;

/*
cx=mouse_x;
cy=mouse_y;
        if (ny>1)
        {
                if (cinit&0x4000000) ramp=(unsigned char *)ram2;
                else                 ramp=(unsigned char *)ram;
                addr=cinit&rammask;
//                printf("Mouse now at %i,%i\n",cx,cy);
                switch (drawcode)
                {
                        case 16:
                        for (y=0;y<ny;y++)
                        {
                                if ((y+cy)>=ys) break;
                                if ((y+cy)>=0)
                                {
                                        vidp16=(unsigned short *)bmp_write_line(b,y+cy);
                                        for (x=0;x<32;x+=4)
                                        {
                                                if ((x+cx)>=0   && ramp[addr]&3)      vidp16[x+cx]=cached_state.vpal[(ramp[addr]&3)|0x100];
                                                if ((x+cx+1)>=0 && (ramp[addr]>>2)&3) vidp16[x+cx+1]=cached_state.vpal[((ramp[addr]>>2)&3)|0x100];
                                                if ((x+cx+2)>=0 && (ramp[addr]>>4)&3) vidp16[x+cx+2]=cached_state.vpal[((ramp[addr]>>4)&3)|0x100];
                                                if ((x+cx+3)>=0 && (ramp[addr]>>6)&3) vidp16[x+cx+3]=cached_state.vpal[((ramp[addr]>>6)&3)|0x100];
                                                addr++;
                                        }
                                }
                        }
                        break;
                        case 32:
                        for (y=0;y<ny;y++)
                        {
                                if ((y+cy)>=ys) break;
                                if ((y+cy)>=0)
                                {
                                        vidp=(uint32_t *)bmp_write_line(b,y+cy);
                                        for (x=0;x<32;x+=4)
                                        {
                                                if ((x+cx)>=0   && ramp[addr]&3)      vidp[x+cx]=cached_state.vpal[(ramp[addr]&3)|0x100];
                                                if ((x+cx+1)>=0 && (ramp[addr]>>2)&3) vidp[x+cx+1]=cached_state.vpal[((ramp[addr]>>2)&3)|0x100];
                                                if ((x+cx+2)>=0 && (ramp[addr]>>4)&3) vidp[x+cx+2]=cached_state.vpal[((ramp[addr]>>4)&3)|0x100];
                                                if ((x+cx+3)>=0 && (ramp[addr]>>6)&3) vidp[x+cx+3]=cached_state.vpal[((ramp[addr]>>6)&3)|0x100];
                                                addr++;
                                        }
                                }
                        }
                        break;
                }
                if (yl>cy) yl=cy;
                if (yl==-1) yl=cy;
                if (cy<0) yl=0;
                if (yh<(ny+cy)) yh=ny+cy;
        }*/

        bmp_unwrite_line(b); 
        for (c=0;c<1024;c++)
        {
//                rpclog("%03i %08X %08X %08X\n",c,vraddrphys[c],(vraddrphys[c]&0x1F000000),vraddrls[c]<<12);
                if ((vwaddrphys[c]&0x1F000000)==0x02000000)
                {
//                        rpclog("Invalidated %08X\n",vraddrls[c]);
                        vwaddrl[vwaddrls[c]]=0xFFFFFFFF;
                        vwaddrphys[c]=vwaddrls[c]=0xFFFFFFFF;
                }
        }
        
/*        if (readflash)
        {
                rectfill(screen,xs-40,4,xs-8,11,readflash);
                readflash=0;
        }*/
//        else
//           blit(b,screen,xs-40,4,xs-40,4,xs-8,11);
//        rpclog("YL %i YH %i\n",yl,yh);
        if (yh>ys) yh=ys;
        if (yl==-1 && yh==-1) return;
        if (yl==-1) yl=0;
//        printf("Cursor %i %i %i\n",cx,cy,ny);
//        rpclog("%i %02X\n",drawcode,bit8);        
//        sleep(2);
//        rpclog("Blitting from 0,%i size %i,%i\n",yl,xs,ys);
/*                if (soundbufferfull)
                {
                        updatesoundbuffer();
                }*/
        #ifdef BLITTER_THREAD
                blitready=1;
                thread_xs=xs;
                thread_ys=ys;
                thread_yl=yl;
                thread_yh=yh;
                thread_doublesize=doublesize;
                wakeupblitterthread();
                return;
        #endif
//        rpclog("Blit %i %i %i %i\n",xs,yl,yh,yh-yl);
        switch (doublesize)
        {
                case 0:// case 1: case 2: case 3:
  //              case 3:
        ys=yh-yl;
                if (fullscreen) blit(b,screen,0,yl,(SCREEN_W-xs)>>1,yl+((SCREEN_H-oldsy)>>1),xs,ys);
                else            blit(b,screen,0,yl,0,yl,xs,ys);
                return;
                case 1:
        ys=yh-yl;
                if (fullscreen) stretch_blit(b,screen, 0,yl,xs,ys, (SCREEN_W-(xs<<1))>>1,yl+((SCREEN_H-oldsy)>>1),xs<<1,ys);
                else            stretch_blit(b,screen, 0,yl,xs,ys, 0,                    yl,                      xs<<1,ys);
                return;
                case 2:
                if (stretchmode)
                {
                        if (fullscreen) stretch_blit(b,screen,0,0,xs,ys,0,0,xs,ys<<1);
                        else            stretch_blit(b,screen,0,0,xs,ys,0,0,xs,ys<<1);
                }
                else
                {
                        ys=yh-yl;
                        if (fullscreen) stretch_blit(b,screen, 0,yl,xs,ys, (SCREEN_W-xs)>>1,(yl<<1)+((SCREEN_H-oldsy)>>1),xs,ys<<1);
                        else            stretch_blit(b,screen, 0,yl,xs,ys, 0,               yl<<1,                        xs,ys<<1);
                }
                return;
                case 3:
                if (stretchmode)
                {
                        if (fullscreen) stretch_blit(b,screen,0,0,xs,ys,0,0,xs<<1,ys<<1);
                        else            stretch_blit(b,screen,0,0,xs,ys,0,0,xs<<1,ys<<1);
                }
                else
                {
                        ys=yh-yl;
                        if (fullscreen) stretch_blit(b,screen, 0,yl,xs,ys, (SCREEN_W-(xs<<1))>>1,(yl<<1)+((SCREEN_H-oldsy)>>1),xs<<1,ys<<1);
                        else            stretch_blit(b,screen, 0,yl,xs,ys, 0,                    yl<<1,                        xs<<1,ys<<1);
                }
                return;
        }
//        printf("%i %i %i - ",hdsr,hder,hder-hdsr);
//        printf("%i %i %i\n",vdsr,vder,vder-vdsr);
}


void writevidc20(uint32_t val)
{
        float f;
//        rpclog("Write VIDC %08X %07X\n",val,PC);
        switch (val>>24)
        {
                case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
                case 8: case 9: case 0xA: case 0xB: case 0xC: case 0xD: case 0xE:
                        case 0xF: /* Video palette data */
//                printf("Write palette index %i %08X\n",palindex,val);
                if (val!=vidc.vidcpal[vidc.palindex])
                {
                        vidc.vidcpal[vidc.palindex]=val;
                        cached_state.palchange=1;
                }
                vidc.palindex++;
                vidc.palindex&=255;
                break;
                case 0x10: /* Video palette address pointer */
                vidc.palindex=val&255;
                break;
                case 0x40: case 0x41: case 0x42: case 0x43:
                case 0x44: case 0x45: case 0x46: case 0x47:
                case 0x48: case 0x49: case 0x4A: case 0x4B:
                case 0x4C: case 0x4D: case 0x4E: case 0x4F:
                /* Border colour register */
                if (val!=vidc.vidcpal[0x100])
                {
                        cached_state.palchange=1;
                        vidc.vidcpal[0x100]=val;
//                        rpclog("Change border colour %08X\n",val);
                }
//                rpclog("Border now %06X\n",val&0xFFFFFF);
                break;
                case 0x50: case 0x51: case 0x52: case 0x53:
                case 0x54: case 0x55: case 0x56: case 0x57:
                case 0x58: case 0x59: case 0x5A: case 0x5B:
                case 0x5C: case 0x5D: case 0x5E: case 0x5F:
                /* Cursor palette (colour 1) */
                if (val!=vidc.vidcpal[0x101])
                {
                        vidc.vidcpal[0x101]=val;
                        cached_state.palchange=1;
                        cached_state.curchange=1;                        
                }
                break;
                case 0x60: case 0x61: case 0x62: case 0x63:
                case 0x64: case 0x65: case 0x66: case 0x67:
                case 0x68: case 0x69: case 0x6A: case 0x6B:
                case 0x6C: case 0x6D: case 0x6E: case 0x6F:
                /* Cursor palette (colour 2) */
                if (val!=vidc.vidcpal[0x102])
                {
                        vidc.vidcpal[0x102]=val;
                        cached_state.palchange=1;                        
                        cached_state.curchange=1;                        
                }
                break;
                case 0x70: case 0x71: case 0x72: case 0x73:
                case 0x74: case 0x75: case 0x76: case 0x77:
                case 0x78: case 0x79: case 0x7A: case 0x7B:
                case 0x7C: case 0x7D: case 0x7E: case 0x7F:
                /* Cursor palette (colour 3) */
                if (val!=vidc.vidcpal[0x103])
                {
                        vidc.vidcpal[0x103]=val;
                        cached_state.palchange=1;
                        cached_state.curchange=1;
                }
                break;
                case 0x83: /* Horizontal Display Start Register */
                vidc.hdsr=val&0xFFE;
                break;
                case 0x84: /* Horizontal Display End Register */
                vidc.hder=val&0xFFE;
                break;
                case 0x86: /* Horizontal Cursor Start Register */
//                printf("HCSR write %03X\n",val&0xFFE);
                if (vidc.hcsr != (val&0xFFE)) cached_state.curchange=1;
                vidc.hcsr=val&0xFFE;
                break;
                case 0x93: /* Vertical Display Start Register */
                vidc.vdsr=val&0xFFF;
                cached_state.palchange=1;
                break;
                case 0x94: /* Vertical Display End Register */
                vidc.vder=val&0xFFF;
                cached_state.palchange=1;
                break;
                case 0x96: /* Vertical Cursor Start Register */
//                printf("VCSR write %03X\n",val&0xFFF);
                if (vidc.vcsr != (val&0xFFF)) cached_state.curchange=1;
                vidc.vcsr=val&0xFFF;
                break;
                case 0x97: /* Vertical Cursor End Register */
//                printf("VCER write %03X\n",val&0xFFF);
                if (vidc.vcer != (val&0xFFF)) cached_state.curchange=1;
                vidc.vcer=val&0xFFF;
                break;
                case 0xB0: /* Sound Frequency Register */
                vidc.b0=val;
//                rpclog("Write B0 %08X %08X\n",val,PC);
                val=(vidc.b0&0xFF)+2;
                if (vidc.b1&1) f=(1000000.0f/(float)val)/4.0f;
                else      f=(705600.0f/(float)val)/4.0f;
                changesamplefreq((int)f);
//                rpclog("Sample rate : %i ns %f hz\n",val,f);
                break;
                case 0xB1: /* Sound Control Register */
//                rpclog("Write B1 %08X %08X\n",val,PC);
                vidc.b1=val;
                val=(vidc.b0&0xFF)+2;
                if (vidc.b1&1) f=(1000000.0f/(float)val)/4.0f;
                else      f=(705600.0f/(float)val)/4.0f;
                changesamplefreq((int)f);
//                rpclog("Sample rate : %i ns %f hz\n",val,f);
                break;
                case 0xE0: /* Control Register */
                if (((val>>5)&7)!=(uint32_t)vidc.bit8)
                {
//                        printf("Change mode - %08X %i\n",val,(val>>5)&7);
                        vidc.bit8=(val>>5)&7;
                        resetbuffer();
                        cached_state.palchange=1;
                }
                break;
        }
}

void resetbuffer(void)
{
        memset(dirtybuffer,1,512*4);
//        rpclog("Reset buffer\n");
}
