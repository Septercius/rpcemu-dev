/*RPCemu v0.3 by Tom Walker
  VIDC20 emulation*/
#include <allegro.h>
#include "rpc.h"

int readflash;
int palchange;
float mips;
BITMAP *b;
int deskdepth;
int oldsx,oldsy;
RGB cursor[3];
RGB border;
int drawcode;
unsigned long vpal[260];
void initvideo()
{
        int depth;
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
        b=create_system_bitmap(1024,768);
        if (!b) /*Video bitmaps unavailable for some reason*/
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
        unsigned long r,g,b;
} pal[256];
PALETTE pal2;

unsigned long hdsr,hcsr,hder;
unsigned long vdsr,vcsr,vcer,vder;
int bit8;

int getxs()
{
        return hder-hdsr;
}
int getys()
{
        return vder-vdsr;
}


void resizedisplay(int x, int y)
{
        if (x<16) x=16;
        if (y<16) y=16;
        if (x>1024) x=1024;
        if (y>768) y=768;
        oldsx=x;
        oldsy=y;
        updatewindowsize(x,y);
}

void dblit(int xs, int yl, int yh)
{
//        int y;
//        for (y=yl;y<yh;y++)
//            blit(b,b2,0,y,0,y<<1,xs,1);
}

int ony,ocy;
int xdiff[8]={8192,4096,2048,1024,512,512,256,256};
void drawscr()
{
        int x,y,xx,xxx;
        int xs=hder-hdsr;
        int ys=vder-vdsr;
        int addr=iomd.vidinit&0x1FFFFF;
        int cx=hcsr-hdsr;
        int cy=vcsr-vdsr,ny=vcer-vcsr;
        int ac=0;
        int drawit=0;
        int yl=-1,yh;
        int c;
        int doublesize=0,doublehigh=0;
        unsigned long oldaddr,temp;
        int cursorcol[3];
        unsigned char *ramp;
        unsigned short temp16;
        unsigned long *vidp;
        iomd.vidend=(iomd.vidend&0x1FFFFF)|(iomd.vidinit&~0x1FFFFF);
        iomd.vidstart=(iomd.vidstart&0x1FFFFF)|(iomd.vidinit&~0x1FFFFF);        
//        rpclog("XS %i YS %i\n",xs,ys);
        if (xs<2) xs=2;
        if (ys<1) ys=480;
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
        if (ys>768) ys=768;
        if (xs>1024) xs=1024;
        if (ys!=oldsy || xs!=oldsx) resizedisplay(xs,ys);
        if (doublesize&1) xs>>=1;
        if (doublesize&2) ys>>=1;
        if (!(iomd.vidcr&0x20) || vdsr>vder)
        {
                if (!dirtybuffer[0] && !palchange) return;
                dirtybuffer[0]=0;
                palchange=0;
                rectfill(b,0,0,xs,ys,vpal[0x100]);
                blit(b,screen,0,0,0,0,xs,ys);
//                set_palette(pal2);
                return;
        }
        if (drawit) yl=0;
        if (palchange)
        {
                memset(dirtybuffer,1,131072);
                palchange=0;
        }
        if (iomd.vidinit&0x10000000) ramp=ramb;
        else                         ramp=vramb;
/*        c=addr>>10;
        rpclog("Start at %06X %03X ",addr,c);
        x=y=0;
        while (!dirtybuffer[c] && (c<1024) && (y<ys))
        {
                x+=xdiff[bit8];
                while (x>=xs)
                {
                        x-=xs;
                        y++;
                }
                addr+=1024;
                c++;
                rpclog("Addr %06X x %i y %i\n",addr,x,y);
        }
        if (c==1024) x=y=0;
        rpclog("found changed data at %06X %i : %i,%i  %i %i %i %i\n",addr,c,x,y,xs,bit8,xdiff[bit8],addr>>10);*/
        x=y=0;
        drawit=dirtybuffer[addr>>12];
        dirtybuffer[addr>>12]=0;
        if (drawit) yl=0;
        iomd.vidstart&=0x1FFFFF;
        iomd.vidend&=0x1FFFFF;        
        switch (drawcode)
        {
                case 16:
                switch (bit8)
                {
                        case 0: /*1 bpp*/
                        for (;y<ys;y++)
                        {
                                if (y<(ony+ocy) && (y>=(ocy-2)))
                                {
                                        drawit=1;
                                        yh=y+8;
                                        if (yl==-1)
                                           yl=y;
                                }
                                for (;x<xs;x+=128)
                                {
                                        if (drawit)
                                        {
                                                for (xx=0;xx<128;xx+=8)
                                                {
                                                        ((unsigned short *)b->line[y])[x+xx]=vpal[ramp[addr]&1];
                                                        ((unsigned short *)b->line[y])[x+xx+1]=vpal[(ramp[addr]>>1)&1];
                                                        ((unsigned short *)b->line[y])[x+xx+2]=vpal[(ramp[addr]>>2)&1];
                                                        ((unsigned short *)b->line[y])[x+xx+3]=vpal[(ramp[addr]>>3)&1];
                                                        ((unsigned short *)b->line[y])[x+xx+4]=vpal[(ramp[addr]>>4)&1];
                                                        ((unsigned short *)b->line[y])[x+xx+5]=vpal[(ramp[addr]>>5)&1];
                                                        ((unsigned short *)b->line[y])[x+xx+6]=vpal[(ramp[addr]>>6)&1];
                                                        ((unsigned short *)b->line[y])[x+xx+7]=vpal[(ramp[addr]>>7)&1];
                                                        addr++;
                                                }
                                        }
                                        else
                                           addr+=16;
                                        if (addr==iomd.vidend) addr=iomd.vidstart;
                                        if (!(addr&0xFFF))
                                        {
                                                drawit=dirtybuffer[(addr>>12)];
                                                if (y<(ony+ocy) && (y>=(ocy-2))) drawit=1;
                                                dirtybuffer[(addr>>12)]=0;
                                                if (drawit) yh=y+8;
                                                if (yl==-1 && drawit)
                                                   yl=y;
                                        }
                                }
                                x=0;
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
                                for (x=0;x<xs;x+=64)
                                {
                                        if (drawit)
                                        {
                                                for (xx=0;xx<64;xx+=4)
                                                {
                                                        ((unsigned short *)b->line[y])[x+xx]=vpal[ramp[addr]&3];
                                                        ((unsigned short *)b->line[y])[x+xx+1]=vpal[(ramp[addr]>>2)&3];
                                                        ((unsigned short *)b->line[y])[x+xx+2]=vpal[(ramp[addr]>>4)&3];
                                                        ((unsigned short *)b->line[y])[x+xx+3]=vpal[(ramp[addr]>>6)&3];
                                                        addr++;
                                                }
                                        }
                                        else
                                           addr+=16;
                                        if (addr==iomd.vidend) addr=iomd.vidstart;
                                        if (!(addr&0xFFF))
                                        {
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
                                for (x=0;x<xs;x+=32)
                                {
                                        if (drawit)
                                        {
                                                for (xx=0;xx<32;xx+=8)
                                                {
                                                        ((unsigned short *)b->line[y])[x+xx]=vpal[ramp[addr]&0xF];
                                                        ((unsigned short *)b->line[y])[x+xx+1]=vpal[ramp[addr]>>4];
                                                        ((unsigned short *)b->line[y])[x+xx+2]=vpal[ramp[addr+1]&0xF];
                                                        ((unsigned short *)b->line[y])[x+xx+3]=vpal[ramp[addr+1]>>4];
                                                        ((unsigned short *)b->line[y])[x+xx+4]=vpal[ramp[addr+2]&0xF];
                                                        ((unsigned short *)b->line[y])[x+xx+5]=vpal[ramp[addr+2]>>4];
                                                        ((unsigned short *)b->line[y])[x+xx+6]=vpal[ramp[addr+3]&0xF];
                                                        ((unsigned short *)b->line[y])[x+xx+7]=vpal[ramp[addr+3]>>4];
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
                        xs>>=1;
//                        rpclog("Start %08X End %08X Init %08X\n",iomd.vidstart,iomd.vidend,addr);
                        for (;y<ys;y++)
                        {
                                if (y<(ony+ocy) && (y>=(ocy-2)))
                                {
                                        drawit=1;
                                        yh=y+8;
                                        if (yl==-1)
                                           yl=y;
                                }
                                if (drawit) vidp=bmp_write_line(b,y);
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
                                                if (!drawit && dirtybuffer[(addr>>12)]) vidp=bmp_write_line(b,y);
                                                drawit=dirtybuffer[(addr>>12)];
                                                if (y<(ony+ocy) && (y>=(ocy-2))) drawit=1;
                                                dirtybuffer[(addr>>12)]=0;
                                                if (drawit) yh=y+8;
                                                if (yl==-1 && drawit)
                                                   yl=y;
                                        }
                                }
                                x=0;
                        }
                        xs<<=1;
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
                                for (x=0;x<xs;x+=8)
                                {
                                        if (drawit)
                                        {
                                                for (xx=0;xx<8;xx+=2)
                                                {
                                                        //VIDC20 pixel format is  xBBB BBGG GGGR RRRR
                                                        //Windows pixel format is RRRR RGGG GGGB BBBB
                                                        temp16=ramp[addr]|(ramp[addr+1]<<8);
                                                        ((unsigned short *)b->line[y])[x+xx]=pal[temp16&0xFF].r|pal[(temp16>>4)&0xFF].g|pal[(temp16>>8)&0xFF].b;
                                                        temp16=ramp[addr+2]|(ramp[addr+3]<<8);
                                                        ((unsigned short *)b->line[y])[x+xx+1]=pal[temp16&0xFF].r|pal[(temp16>>4)&0xFF].g|pal[(temp16>>8)&0xFF].b;
                                                        addr+=4;
                                                }
                                        }
                                        else
                                           addr+=16;
                                        if (addr==iomd.vidend) addr=iomd.vidstart;
                                        if (!(addr&0xFFF))
                                        {
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
//                        textprintf(b,font,0,8,makecol(255,255,255),"%i %i %i %i  ",drawit,addr>>10,xs,ys);
//                        textprintf(screen,font,0,8,makecol(255,255,255),"%i %i %i %i  ",drawit,addr>>10,xs,ys);
                        for (y=0;y<ys;y++)
                        {
                                if (y<(ony+ocy) && (y>=(ocy-2)))
                                {
                                        drawit=1;
                                        yh=y+8;
                                        if (yl==-1)
                                           yl=y;
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
                                                        ((unsigned short *)b->line[y])[x+xx]=pal[temp&0xFF].r|pal[(temp>>8)&0xFF].g|pal[(temp>>16)&0xFF].b;
                                                        addr+=4;
                                                }
                                        }
                                        else
                                           addr+=16;
                                        if (addr==iomd.vidend) addr=iomd.vidstart;
                                        if (!(addr&0xFFF))
                                        {
                                                drawit=dirtybuffer[(addr>>12)];
//                                                drawit=1;
                                                if (y<(ony+ocy) && (y>=(ocy-2))) drawit=1;
                                                dirtybuffer[(addr>>12)]=0;
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
                                if (drawit) vidp=bmp_write_line(b,y);
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
                                                if (!drawit && dirtybuffer[(addr>>12)]) vidp=bmp_write_line(b,y);
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
                                if (drawit) vidp=bmp_write_line(b,y);
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
                                                if (!drawit && dirtybuffer[(addr>>12)]) vidp=bmp_write_line(b,y);
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
                                if (drawit) vidp=bmp_write_line(b,y);
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
                                                if (!drawit && dirtybuffer[(addr>>12)]) vidp=bmp_write_line(b,y);
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
                                if (drawit) vidp=bmp_write_line(b,y);
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
                                                if (!drawit && dirtybuffer[(addr>>12)]) vidp=bmp_write_line(b,y);
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
                                if (drawit) vidp=bmp_write_line(b,y);
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
                                                if (!drawit && dirtybuffer[(addr>>12)]) vidp=bmp_write_line(b,y);
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
                                if (drawit) vidp=bmp_write_line(b,y);
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
                                                if (!drawit && dirtybuffer[(addr>>12)]) vidp=bmp_write_line(b,y);
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
        if (ny>1)
        {
                if (cinit&0x4000000) ramp=ram2;
                else                 ramp=ram;
                addr=cinit&rammask;
                switch (drawcode)
                {
                        case 16:
                        for (y=0;y<ny;y++)
                        {
                                if (y>=ys) break;
                                vidp=bmp_write_line(b,y+cy);
                                for (x=0;x<32;x+=4)
                                {
                                        if (ramp[addr]&3)      ((unsigned short *)vidp)[x+cx]=vpal[(ramp[addr]&3)|0x100];
                                        if ((ramp[addr]>>2)&3) ((unsigned short *)vidp)[x+cx+1]=vpal[((ramp[addr]>>2)&3)|0x100];
                                        if ((ramp[addr]>>4)&3) ((unsigned short *)vidp)[x+cx+2]=vpal[((ramp[addr]>>4)&3)|0x100];
                                        if ((ramp[addr]>>6)&3) ((unsigned short *)vidp)[x+cx+3]=vpal[((ramp[addr]>>6)&3)|0x100];
                                        addr++;
                                }
                        }
                        break;
                        case 32:
                        for (y=0;y<ny;y++)
                        {
                                if (y>=ys) break;
                                vidp=bmp_write_line(b,y+cy);
                                for (x=0;x<32;x+=4)
                                {
                                        if (ramp[addr]&3)      vidp[x+cx]=vpal[(ramp[addr]&3)|0x100];
                                        if ((ramp[addr]>>2)&3) vidp[x+cx+1]=vpal[((ramp[addr]>>2)&3)|0x100];
                                        if ((ramp[addr]>>4)&3) vidp[x+cx+2]=vpal[((ramp[addr]>>4)&3)|0x100];
                                        if ((ramp[addr]>>6)&3) vidp[x+cx+3]=vpal[((ramp[addr]>>6)&3)|0x100];
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
        if (yh>ys) yh=ys;
        if (yl==-1) yl=0;
//        printf("Cursor %i %i %i\n",cx,cy,ny);
        ony=ny;
        ocy=cy;
        ys=yh-yl;
//        rpclog("%i %02X\n",drawcode,bit8);        
//        sleep(2);
        switch (doublesize)
        {
                case 0:// case 1: case 2: case 3:
                blit(b,screen,0,yl,0,yl,xs,ys);
                return;
                case 1:
                stretch_blit(b,screen,
                             0,yl,xs,ys,
                             0,yl,xs<<1,ys);
                return;
                case 2:
//                dblit(xs,yl,yh);
//                blit(b2,screen,0,yl,0,yl,xs,ys);
//                textprintf(screen,font,0,120,0xFFFF,"%i %i %i   ",yl,yh,ys);
//                blit(b2,screen,0,yl<<1,0,yl<<1,xs,ys<<1);
                stretch_blit(b,screen,
                             0,yl,xs,ys,
                             0,yl<<1,xs,ys<<1);
                return;
                case 3:
//                dblit(xs,yl,yh);                        
                stretch_blit(b,screen,
                             0,yl,xs,ys,
                             0,yl<<1,xs<<1,ys<<1);
                return;
        }
//        printf("%i %i %i - ",hdsr,hder,hder-hdsr);
//        printf("%i %i %i\n",vdsr,vder,vder-vdsr);
}

int samplefreq;
unsigned long vidcpal[0x104];
void writevidc20(uint32_t val)
{
        float f;
        switch (val>>24)
        {
                case 0:
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
                }
                break;
                case 0x83:
                hdsr=val&0xFFE;
                break;
                case 0x84:
                hder=val&0xFFE;
                break;
                case 0x86:
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
                vcsr=val&0xFFF;
                break;
                case 0x97:
                vcer=val&0xFFF;
                break;
                case 0xB0:
                val=(val&0xFF)+2;
                f=(1000000.0f/(float)val)/4.0f;
                samplefreq=(int)f;
//                rpclog("Sample rate : %i ns %f hz\n",val,f);
                break;
                case 0xE0:
                if (((val>>5)&7)!=bit8)
                {
                        bit8=(val>>5)&7;
                        resetbuffer();
                        palchange=1;
                }
                break;
        }
}

void resetbuffer()
{
        memset(dirtybuffer,1,131072);
}

void dumpscreen()
{
        save_pcx("scrshot.pcx",b,pal);
}

