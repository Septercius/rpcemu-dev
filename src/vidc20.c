/*RPCemu v0.5 by Tom Walker
  VIDC20 emulation*/
#include <allegro.h>
#include "rpcemu.h"

int fullscreen=0;
int readflash;
int palchange,curchange;
int blits=0;
float mips;
BITMAP *b;
int deskdepth;
int oldsx,oldsy;
RGB cursor[3];
RGB border;
int drawcode;
uint32_t vpal[260];

int thread_yh,thread_yl,thread_xs,thread_ys,thread_doublesize;
int blitready=0;
int inblit=0;
void blitterthread()
{
        int xs=thread_xs;
        int ys=thread_ys;
        int yh=thread_yh;
        int yl=thread_yl;
//        rpclog("Blitter thread - blitready %i\n",blitready);
        if (!blitready) return;
        inblit=1;
        switch (thread_doublesize)
        {
                case 0: //case 1: case 2: case 3:
  //              case 3:
        ys=yh-yl;
                if (fullscreen) blit(b,screen,0,yl,(SCREEN_W-xs)>>1,yl+((SCREEN_H-oldsy)>>1),xs,ys);
                else            blit(b,screen,0,yl,0,yl,xs,ys);
                break;
                case 1:
        ys=yh-yl;
                if (fullscreen) stretch_blit(b,screen, 0,yl,xs,ys, (SCREEN_W-(xs<<1))>>1,yl+((SCREEN_H-oldsy)>>1),xs<<1,ys);
                else            stretch_blit(b,screen, 0,yl,xs,ys, 0,                    yl,                      xs<<1,ys);
                break;
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
                break;
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
                break;
        }
        inblit=0;
        blitready=0;
}
     
void initvideo()
{
        int depth;
//        int tempo=0;
        depth=deskdepth=desktop_color_depth();
        if (depth==16 || depth==15)
        {
                set_color_depth(16);
                if (set_gfx_mode(GFX_AUTODETECT_WINDOWED,1024,768,0,0))
                {
                        set_color_depth(15);
                        depth=15;
                        set_gfx_mode(GFX_AUTODETECT_WINDOWED,1024,768,0,0);
                }
                drawcode=16;
        }
        else if (depth==32)
        {
                set_color_depth(depth);
                set_gfx_mode(GFX_AUTODETECT_WINDOWED,1024,768,0,0);
                drawcode=32;
        }
        else
        {
                error("Your desktop must be set to either 16-bit or 32-bit colour to run RPCemu");
                exit(0);
        }
//        set_color_depth(8);
//        if (depth!=15) set_color_depth(16);
//        else           set_color_depth(15);
#ifdef HARDWAREBLIT
        b=create_system_bitmap(1024,768);
        if (!b) /*Video bitmaps unavailable for some reason*/
#endif
           b=create_bitmap(1024,768);
//        b2=create_video_bitmap(1024,768);
//        if (!b2) /*Video bitmaps unavailable for some reason*/
//           b2=create_bitmap(1024,768);
        oldsx=64;
        oldsy=48;
}

int palindex;
struct
{
        uint32_t r,g,b;
} pal[256];
PALETTE pal2;

uint32_t hdsr,hcsr,hder;
uint32_t vdsr,vcsr,vcer,vder;
int bit8;

int getxs()
{
        return hder-hdsr;
}
int getys()
{
        return vder-vdsr;
}

int fullresolutions[][2]=
{
        {320,200},
        {320,240},
        {400,300},
        {512,384},
        {640,400},
        {640,480},
        {800,600},
        {1024,768},
        {1280,1024},
        {-1,-1}
};

int lastfullscreen=0;
void resizedisplay(int x, int y)
{
        int c;
        if (x<16) x=16;
        if (y<16) y=16;
        if (x>1024) x=1024;
        if (y>768) y=768;
        oldsx=x;
        oldsy=y;
        while (inblit || blitready) sleep(1);
        if (fullscreen)
        {
                destroy_bitmap(b);
                c=0;
                while (fullresolutions[c][0]!=-1)
                {
                        if (fullresolutions[c][0]>=x && fullresolutions[c][1]>=y)
                           break;
                        c++;
                }
                if (fullresolutions[c][0]==-1) c--;
                set_gfx_mode(GFX_AUTODETECT_FULLSCREEN,fullresolutions[c][0],fullresolutions[c][1],0,0);
                b=create_system_bitmap(x+16,y+16);
                if (!b) /*Video bitmaps unavailable for some reason*/
                   b=create_bitmap(x+16,y+16);
                lastfullscreen=1;
        }
        else
        {
                if (lastfullscreen) destroy_bitmap(b);
                if (lastfullscreen) set_gfx_mode(GFX_AUTODETECT_WINDOWED,1024,768,0,0);
                updatewindowsize(x,y);
                if (lastfullscreen) b=create_system_bitmap(1024,768);
                if (!b && lastfullscreen) /*Video bitmaps unavailable for some reason*/
                   b=create_bitmap(1024,768);
                lastfullscreen=0;
        }
}

void togglefullscreen(int fs)
{
        oldsx=oldsy=-1;
        memset(dirtybuffer,1,512);
}

void dblit(int xs, int yl, int yh)
{
//        int y;
//        for (y=yl;y<yh;y++)
//            blit(b,b2,0,y,0,y<<1,xs,1);
}

int ony,ocy;
int xdiff[8]={8192,4096,2048,1024,512,512,256,256};
int lastframeborder=0;
uint32_t curcrc=0;

unsigned short crc;
void calccrc(unsigned char byte)
{
        int i;
	for (i = 0; i < 8; i++) {
		if (crc & 0x8000) {
			crc <<= 1;
			if (!(byte & 0x80)) crc ^= 0x1021;
		} else {
			crc <<= 1;
			if (byte & 0x80) crc ^= 0x1021;
		}
		byte <<= 1;
	}
}

void drawscr()
{
        int x,y,xx,xxx;
        int xs=hder-hdsr;
        int ys=vder-vdsr;
        int addr;
        int cx=hcsr-hdsr;
        int cy=vcsr-vdsr,ny=vcer-vcsr;
        int ac=0;
        int drawit=0,olddrawit=0;
        int yl=-1,yh;
        int c;
        int doublesize=0,doublehigh=0;
        uint32_t oldaddr,temp;
        int cursorcol[3];
        unsigned char *ramp;
        unsigned short temp16,*ramw;
        uint32_t *vidp;
        unsigned short *vidp16;
        int firstblock,lastblock;
        #ifdef BLITTER_THREAD
        while (blitready)
        {
                if (soundbufferfull)
                {
                        updatesoundbuffer();
                }
                sleep(0);
//                blits++;
//                return;
        }
        #endif
        blits++;
//        rpclog("Draw screen\n");
        iomd.vidend=(iomd.vidend&0x1FFFFF)|(iomd.vidinit&~0x1FFFFF);
        iomd.vidstart=(iomd.vidstart&0x1FFFFF)|(iomd.vidinit&~0x1FFFFF);        
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
        if (ys>768) ys=768;
        if (xs>1024) xs=1024;
        if (ys!=oldsy || xs!=oldsx) resizedisplay(xs,ys);
        if (!(iomd.vidcr&0x20) || vdsr>vder)
        {
                lastframeborder=1;
                if (!dirtybuffer[0] && !palchange) return;
                dirtybuffer[0]=0;
                palchange=0;
                rectfill(b,0,0,xs,ys,vpal[0x100]);
                blit(b,screen,0,0,0,0,xs,ys);
//                set_palette(pal2);
                return;
        }
        if (doublesize&1) xs>>=1;
        if (doublesize&2) ys>>=1;
        if (lastframeborder)
        {
                lastframeborder=0;
                resetbuffer();
        }
        if (drawit) yl=0;
        if (palchange)
        {
                memset(dirtybuffer,1,512);
                palchange=0;
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
                x+=(xdiff[bit8]<<2);
                while (x>xs)
                {
                        x-=xs;
                        y++;
                }
        }
//        #if 0
        if (firstblock==-1 && !curchange) 
        {
                /*Not looking good for screen redraw - check to see if cursor data has changed*/
                if (cinit&0x4000000) ramp=(unsigned char *)ram2;
                else                 ramp=(unsigned char *)ram;
                addr=(cinit&rammask);//>>2;
                temp=0;
                crc=0xFFFF;
                for (c=0;c<(ny<<3);c++)
                    calccrc(ramp[addr++]);
                /*If cursor data matches then no point redrawing screen - return*/
                if (crc==curcrc)
                   return;
                curcrc=crc;
        }
//        blits++;
        if (iomd.vidinit&0x10000000) ramp=ramb;
        else                         ramp=vramb;
        ramw=(unsigned short *)ramp;
//        #endif
        addr=iomd.vidinit&0x1FFFFF;
        curchange=0;
//        rpclog("First block %i %08X last block %i %08X finished at %i %08X\n",firstblock,firstblock,lastblock,lastblock,c,c);
        x=y=0;
        drawit=dirtybuffer[addr>>12];
        if (drawit) dirtybuffer[addr>>12]--;
        if (drawit) yl=0;
        iomd.vidstart&=0x1FFFFF;
        iomd.vidend&=0x1FFFFF;        
        switch (drawcode)
        {
                case 16:
                switch (bit8)
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
                                                        vidp[x+xx]=  vpal[ramp[addr]&1]     |(vpal[(ramp[addr]>>1)&1]<<16);
                                                        vidp[x+xx+1]=vpal[(ramp[addr]>>2)&1]|(vpal[(ramp[addr]>>3)&1]<<16);
                                                        vidp[x+xx+2]=vpal[(ramp[addr]>>4)&1]|(vpal[(ramp[addr]>>5)&1]<<16);
                                                        vidp[x+xx+3]=vpal[(ramp[addr]>>6)&1]|(vpal[(ramp[addr]>>7)&1]<<16);
                                                        addr++;
                                                }
                                        }
                                        else
                                           addr+=16;
                                        if (addr==iomd.vidend) addr=iomd.vidstart;
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
                                                        vidp[x+xx]=vpal[ramp[addr]&3]|(vpal[(ramp[addr]>>2)&3]<<16);
                                                        vidp[x+xx+1]=vpal[(ramp[addr]>>4)&3]|(vpal[(ramp[addr]>>6)&3]<<16);
                                                        addr++;
                                                }
                                        }
                                        else
                                           addr+=16;
                                        if (addr==iomd.vidend) addr=iomd.vidstart;
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
                                                        vidp[x+xx]=vpal[ramp[addr]&0xF]|(vpal[ramp[addr]>>4]<<16);
                                                        vidp[x+xx+1]=vpal[ramp[addr+1]&0xF]|(vpal[ramp[addr+1]>>4]<<16);
                                                        vidp[x+xx+2]=vpal[ramp[addr+2]&0xF]|(vpal[ramp[addr+2]>>4]<<16);
                                                        vidp[x+xx+3]=vpal[ramp[addr+3]&0xF]|(vpal[ramp[addr+3]>>4]<<16);                                                                                                                                                                        
                                                        addr+=4;
                                                }
                                        }
                                        else
                                           addr+=16;
                                        if (addr==iomd.vidend) 
                                        {
                                                addr=iomd.vidstart;
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
//                        rpclog("Start %08X End %08X Init %08X\n",iomd.vidstart,iomd.vidend,addr);
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
                                                        vidp[x+xx]=vpal[ramp[addr]&0xFF]|(vpal[ramp[addr+1]&0xFF]<<16);
                                                        vidp[x+xx+1]=vpal[ramp[addr+2]&0xFF]|(vpal[ramp[addr+3]&0xFF]<<16);
                                                        addr+=4;
                                                }
                                        }
                                        else
                                           addr+=16;
                                        if (addr==iomd.vidend) 
                                           addr=iomd.vidstart;
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
                                                temp=pal[temp16&0xFF].r|pal[(temp16>>4)&0xFF].g|pal[temp16>>8].b;
                                                temp16=ramw[addr+1];
                                                temp|=(pal[temp16&0xFF].r|pal[(temp16>>4)&0xFF].g|pal[temp16>>8].b)<<16;
                                                vidp[x++]=temp;
                                                temp16=ramw[addr+2];
                                                temp=pal[temp16&0xFF].r|pal[(temp16>>4)&0xFF].g|pal[temp16>>8].b;
                                                temp16=ramw[addr+3];
                                                temp|=(pal[temp16&0xFF].r|pal[(temp16>>4)&0xFF].g|pal[temp16>>8].b)<<16;
                                                vidp[x++]=temp;
                                                temp16=ramw[addr+4];
                                                temp=pal[temp16&0xFF].r|pal[(temp16>>4)&0xFF].g|pal[temp16>>8].b;
                                                temp16=ramw[addr+5];
                                                temp|=(pal[temp16&0xFF].r|pal[(temp16>>4)&0xFF].g|pal[temp16>>8].b)<<16;
                                                vidp[x++]=temp;
                                                temp16=ramw[addr+6];
                                                temp=pal[temp16&0xFF].r|pal[(temp16>>4)&0xFF].g|pal[temp16>>8].b;
                                                temp16=ramw[addr+7];
                                                temp|=(pal[temp16&0xFF].r|pal[(temp16>>4)&0xFF].g|pal[temp16>>8].b)<<16;
                                                vidp[x++]=temp;
/*                                                vidp[x+1]=pal[temp16&0xFF].r|pal[(temp16>>4)&0xFF].g|pal[temp16>>8].b;
                                                temp16=ramw[addr+2];
                                                vidp[x+2]=pal[temp16&0xFF].r|pal[(temp16>>4)&0xFF].g|pal[temp16>>8].b;
                                                temp16=ramw[addr+3];
                                                vidp[x+3]=pal[temp16&0xFF].r|pal[(temp16>>4)&0xFF].g|pal[temp16>>8].b;
                                                temp16=ramw[addr+4];
                                                vidp[x+4]=pal[temp16&0xFF].r|pal[(temp16>>4)&0xFF].g|pal[temp16>>8].b;
                                                temp16=ramw[addr+5];
                                                vidp[x+5]=pal[temp16&0xFF].r|pal[(temp16>>4)&0xFF].g|pal[temp16>>8].b;
                                                temp16=ramw[addr+6];
                                                vidp[x+6]=pal[temp16&0xFF].r|pal[(temp16>>4)&0xFF].g|pal[temp16>>8].b;
                                                temp16=ramw[addr+7];
                                                vidp[x+7]=pal[temp16&0xFF].r|pal[(temp16>>4)&0xFF].g|pal[temp16>>8].b;*/
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
                                                temp16=ramw[addr];
                                                temp=pal[temp16&0xFF].r|pal[(temp16>>4)&0xFF].g|pal[temp16>>8].b;
                                                temp16=ramw[addr+1];
                                                vidp[x]=((pal[temp16&0xFF].r|pal[(temp16>>4)&0xFF].g|pal[temp16>>8].b)<<16)|temp;
                                                temp16=ramw[addr+2];
                                                temp=pal[temp16&0xFF].r|pal[(temp16>>4)&0xFF].g|pal[temp16>>8].b;
                                                temp16=ramw[addr+3];
                                                vidp[x+1]=((pal[temp16&0xFF].r|pal[(temp16>>4)&0xFF].g|pal[temp16>>8].b)<<16)|temp;
                                                temp16=ramw[addr+4];
                                                temp=pal[temp16&0xFF].r|pal[(temp16>>4)&0xFF].g|pal[temp16>>8].b;
                                                temp16=ramw[addr+5];
                                                vidp[x+2]=((pal[temp16&0xFF].r|pal[(temp16>>4)&0xFF].g|pal[temp16>>8].b)<<16)|temp;
                                                temp16=ramw[addr+6];
                                                temp=pal[temp16&0xFF].r|pal[(temp16>>4)&0xFF].g|pal[temp16>>8].b;
                                                temp16=ramw[addr+7];
                                                vidp[x+3]=((pal[temp16&0xFF].r|pal[(temp16>>4)&0xFF].g|pal[temp16>>8].b)<<16)|temp;
                                                addr<<=1;
                                                addr+=16;
                                        }
                                        else
                                           addr+=16;
                                        if (addr==iomd.vidend) addr=iomd.vidstart;
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
                                                        vidp16[x+xx]=pal[temp&0xFF].r|pal[(temp>>8)&0xFF].g|pal[(temp>>16)&0xFF].b;
                                                        addr+=4;
                                                }
                                        }
                                        else
                                           addr+=16;
                                        if (addr==iomd.vidend) addr=iomd.vidstart;
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
                        error("Bad BPP %i\n",bit8);
                        exit(-1);
                }
                break;
                case 32:
                switch (bit8)
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
                                                        vidp[x+xx]=vpal[ramp[addr]&1];
                                                        vidp[x+xx+1]=vpal[(ramp[addr]>>1)&1];
                                                        vidp[x+xx+2]=vpal[(ramp[addr]>>2)&1];
                                                        vidp[x+xx+3]=vpal[(ramp[addr]>>3)&1];
                                                        vidp[x+xx+4]=vpal[(ramp[addr]>>4)&1];
                                                        vidp[x+xx+5]=vpal[(ramp[addr]>>5)&1];
                                                        vidp[x+xx+6]=vpal[(ramp[addr]>>6)&1];
                                                        vidp[x+xx+7]=vpal[(ramp[addr]>>7)&1];
                                                        addr++;
                                                }
                                        }
                                        else
                                           addr+=16;
                                        if (addr==iomd.vidend) addr=iomd.vidstart;
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
                                                        vidp[x+xx]=vpal[ramp[addr]&3];
                                                        vidp[x+xx+1]=vpal[(ramp[addr]>>2)&3];
                                                        vidp[x+xx+2]=vpal[(ramp[addr]>>4)&3];
                                                        vidp[x+xx+3]=vpal[(ramp[addr]>>6)&3];
                                                        addr++;
                                                }
                                        }
                                        else
                                           addr+=16;
                                        if (addr==iomd.vidend) addr=iomd.vidstart;
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
                                                        vidp[x+xx]=vpal[ramp[addr]&0xF];
                                                        vidp[x+xx+1]=vpal[(ramp[addr]>>4)&0xF];
                                                        vidp[x+xx+2]=vpal[ramp[addr+1]&0xF];
                                                        vidp[x+xx+3]=vpal[(ramp[addr+1]>>4)&0xF];
                                                        vidp[x+xx+4]=vpal[ramp[addr+2]&0xF];
                                                        vidp[x+xx+5]=vpal[(ramp[addr+2]>>4)&0xF];
                                                        vidp[x+xx+6]=vpal[ramp[addr+3]&0xF];
                                                        vidp[x+xx+7]=vpal[(ramp[addr+3]>>4)&0xF];
                                                        addr+=4;
                                                }
                                        }
                                        else
                                           addr+=16;
                                        if (addr==iomd.vidend) addr=iomd.vidstart;
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
                                                        vidp[x+xx]=vpal[ramp[addr]&0xFF];
                                                        vidp[x+xx+1]=vpal[ramp[addr+1]&0xFF];                                                        
                                                        vidp[x+xx+2]=vpal[ramp[addr+2]&0xFF];                                                        
                                                        vidp[x+xx+3]=vpal[ramp[addr+3]&0xFF];                                                                                                                
                                                        addr+=4;
                                                }
                                        }
                                        else
                                           addr+=16;
                                        if (addr==iomd.vidend) addr=iomd.vidstart;
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
                                                        vidp[x+xx]=pal[temp16&0xFF].r|pal[(temp16>>4)&0xFF].g|pal[(temp16>>8)&0xFF].b;
                                                        temp16=ramp[addr+2]|(ramp[addr+3]<<8);
                                                        vidp[x+xx+1]=pal[temp16&0xFF].r|pal[(temp16>>4)&0xFF].g|pal[(temp16>>8)&0xFF].b;
                                                        addr+=4;
                                                }
                                        }
                                        else
                                           addr+=16;
                                        if (addr==iomd.vidend) addr=iomd.vidstart;
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
                                                        vidp[x+xx]=pal[ramp[addr]].r|pal[ramp[addr+1]].g|pal[ramp[addr+2]].b;
                                                        addr+=4;
                                                }
                                        }
                                        else
                                           addr+=16;
                                        if (addr==iomd.vidend) addr=iomd.vidstart;
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
                        error("Bad BPP %i\n",bit8);
                        exit(-1);
                }
        }
                if (soundbufferfull)
                {
                        updatesoundbuffer();
                }
        if (ny>1)
        {
                if (cinit&0x4000000) ramp=(unsigned char *)ram2;
                else                 ramp=(unsigned char *)ram;
                addr=cinit&rammask;
                switch (drawcode)
                {
                        case 16:
                        for (y=0;y<ny;y++)
                        {
                                if ((y+cy)>=ys) break;
                                vidp16=(unsigned short *)bmp_write_line(b,y+cy);
                                for (x=0;x<32;x+=4)
                                {
                                        if ((x+cx)>=0 && ramp[addr]&3)      vidp16[x+cx]=vpal[(ramp[addr]&3)|0x100];
                                        if ((x+cx)>=0 && (ramp[addr]>>2)&3) vidp16[x+cx+1]=vpal[((ramp[addr]>>2)&3)|0x100];
                                        if ((x+cx)>=0 && (ramp[addr]>>4)&3) vidp16[x+cx+2]=vpal[((ramp[addr]>>4)&3)|0x100];
                                        if ((x+cx)>=0 && (ramp[addr]>>6)&3) vidp16[x+cx+3]=vpal[((ramp[addr]>>6)&3)|0x100];
                                        addr++;
                                }
                        }
                        break;
                        case 32:
                        for (y=0;y<ny;y++)
                        {
                                if ((y+cy)>=ys) break;
                                vidp=(uint32_t *)bmp_write_line(b,y+cy);
                                for (x=0;x<32;x+=4)
                                {
                                        if ((x+cx)>=0 && ramp[addr]&3)      vidp[x+cx]=vpal[(ramp[addr]&3)|0x100];
                                        if ((x+cx)>=0 && (ramp[addr]>>2)&3) vidp[x+cx+1]=vpal[((ramp[addr]>>2)&3)|0x100];
                                        if ((x+cx)>=0 && (ramp[addr]>>4)&3) vidp[x+cx+2]=vpal[((ramp[addr]>>4)&3)|0x100];
                                        if ((x+cx)>=0 && (ramp[addr]>>6)&3) vidp[x+cx+3]=vpal[((ramp[addr]>>6)&3)|0x100];
                                        addr++;
                                }
                        }
                        break;
                }
                if (yl>cy) yl=cy;
                if (yl==-1) yl=cy;
                if (cy<0) yl=0;
                if (yh<(ny+cy)) yh=ny+cy;
        }
        bmp_unwrite_line(b); 
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
        ony=ny;
        ocy=cy;
//        rpclog("%i %02X\n",drawcode,bit8);        
//        sleep(2);
//        rpclog("Blitting from 0,%i size %i,%i\n",yl,xs,ys);
//        blits++;
                if (soundbufferfull)
                {
                        updatesoundbuffer();
                }
        #ifdef BLITTER_THREAD
                blitready=1;
                thread_xs=xs;
                thread_ys=ys;
                thread_yl=yl;
                thread_yh=yh;
                thread_doublesize=doublesize;
                return;
        #endif
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

int samplefreq;
uint32_t vidcpal[0x104];
uint32_t b0,b1;
void writevidc20(uint32_t val)
{
        float f;
//        rpclog("Write VIDC %08X %07X\n",val,PC);
        switch (val>>24)
        {
                case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
                case 8: case 9: case 0xA: case 0xB: case 0xC: case 0xD: case 0xE:
                        case 0xF:
//                rpclog("Write palette index %i %08X\n",palindex,val);
                if (val!=vidcpal[palindex])
                {
                        pal[palindex].r=makecol(val&0xFF,0,0);
                        pal[palindex].g=makecol(0,(val>>8)&0xFF,0);
                        pal[palindex].b=makecol(0,0,(val>>16)&0xFF);
                        vpal[palindex]=makecol(val&0xFF,(val>>8)&0xFF,(val>>16)&0xFF);
                        vidcpal[palindex]=val;
                        palchange=1;
                }
                palindex++;
                palindex&=255;
                break;
                case 0x10:
                palindex=val&255;
                break;
                case 0x40: case 0x41: case 0x42: case 0x43:
                case 0x44: case 0x45: case 0x46: case 0x47:
                case 0x48: case 0x49: case 0x4A: case 0x4B:
                case 0x4C: case 0x4D: case 0x4E: case 0x4F:
                if (val!=vidcpal[0x100])
                {
                        pal2[0].r=(val&0xFF)>>2;
                        pal2[0].g=((val>>8)&0xFF)>>2;
                        pal2[0].b=((val>>16)&0xFF)>>2;
                        vpal[0x100]=makecol(val&0xFF,(val>>8)&0xFF,(val>>16)&0xFF);
                        palchange=1;
                        vidcpal[0x100]=val;
//                        rpclog("Change border colour %08X\n",val);
                }
//                rpclog("Border now %06X\n",val&0xFFFFFF);
                break;
                case 0x50: case 0x51: case 0x52: case 0x53:
                case 0x54: case 0x55: case 0x56: case 0x57:
                case 0x58: case 0x59: case 0x5A: case 0x5B:
                case 0x5C: case 0x5D: case 0x5E: case 0x5F:
                if (val!=vidcpal[0x101])
                {
                        cursor[0].r=val&0xFF;
                        cursor[0].g=(val>>8)&0xFF;
                        cursor[0].b=(val>>16)&0xFF;
                        vpal[0x101]=makecol(val&0xFF,(val>>8)&0xFF,(val>>16)&0xFF);
                        palchange=1;
                        vidcpal[0x101]=val;
                        curchange=1;                        
                }
                break;
                case 0x60: case 0x61: case 0x62: case 0x63:
                case 0x64: case 0x65: case 0x66: case 0x67:
                case 0x68: case 0x69: case 0x6A: case 0x6B:
                case 0x6C: case 0x6D: case 0x6E: case 0x6F:
                if (val!=vidcpal[0x102])
                {
                        cursor[1].r=val&0xFF;
                        cursor[1].g=(val>>8)&0xFF;
                        cursor[1].b=(val>>16)&0xFF;
                        vpal[0x102]=makecol(val&0xFF,(val>>8)&0xFF,(val>>16)&0xFF);
                        palchange=1;                        
                        vidcpal[0x102]=val;
                        curchange=1;                        
                }
                break;
                case 0x70: case 0x71: case 0x72: case 0x73:
                case 0x74: case 0x75: case 0x76: case 0x77:
                case 0x78: case 0x79: case 0x7A: case 0x7B:
                case 0x7C: case 0x7D: case 0x7E: case 0x7F:
                if (val!=vidcpal[0x103])
                {
                        cursor[2].r=val&0xFF;
                        cursor[2].g=(val>>8)&0xFF;
                        cursor[2].b=(val>>16)&0xFF;
                        vpal[0x103]=makecol(val&0xFF,(val>>8)&0xFF,(val>>16)&0xFF);
                        palchange=1;
                        vidcpal[0x103]=val;
                        curchange=1;
                }
                break;
                case 0x83:
                hdsr=val&0xFFE;
                break;
                case 0x84:
                hder=val&0xFFE;
                break;
                case 0x86:
                if (hcsr != (val&0xFFE)) curchange=1;
                hcsr=val&0xFFE;
                break;
                case 0x93:
                vdsr=val&0xFFF;
                palchange=1;
                break;
                case 0x94:
                vder=val&0xFFF;
                palchange=1;
                break;
                case 0x96:
                if (vcsr != (val&0xFFF)) curchange=1;
                vcsr=val&0xFFF;
                break;
                case 0x97:
                if (vcer != (val&0xFFF)) curchange=1;
                vcer=val&0xFFF;
                break;
                case 0xB0:
                b0=val;
//                rpclog("Write B0 %08X %08X\n",val,PC);
                val=(b0&0xFF)+2;
                if (b1&1) f=(1000000.0f/(float)val)/4.0f;
                else      f=(705600.0f/(float)val)/4.0f;
                samplefreq=(int)f;
                changesamplefreq();
//                rpclog("Sample rate : %i ns %f hz\n",val,f);
                break;
                case 0xB1:
//                rpclog("Write B1 %08X %08X\n",val,PC);
                b1=val;
                val=(b0&0xFF)+2;
                if (b1&1) f=(1000000.0f/(float)val)/4.0f;
                else      f=(705600.0f/(float)val)/4.0f;
                samplefreq=(int)f;
                changesamplefreq();
//                rpclog("Sample rate : %i ns %f hz\n",val,f);
                break;
                case 0xE0:
                if (((val>>5)&7)!=bit8)
                {
//                        rpclog("Change mode - %08X %i\n",val,(val>>5)&7);
                        bit8=(val>>5)&7;
                        resetbuffer();
                        palchange=1;
                }
                break;
        }
}

void resetbuffer()
{
        memset(dirtybuffer,1,512);
//        rpclog("Reset buffer\n");
}

void closevideo()
{
        destroy_bitmap(b);
        allegro_exit();
}
