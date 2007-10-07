/*RPCemu v0.6 by Tom Walker
  Main loop
  Compile using makefile.dj*/

#include <allegro.h>
#include "rpcemu.h"
#include "mem.h"
#include "vidc20.h"
#include "sound.h"
#include "iomd.h"
#include "gui.h"

int quited=0;


int mousecapture=0;
float mips;
int updatemips=0;

static uint32_t mipscount;
float mipstotal;

void wakeupsoundthread()
{
}

static void domips(void)
{
        mips=(float)inscount/1000000.0f;
	mipscount += 1;
	if (mipscount > 10)
	  mipstotal += mips;
        inscount=0;
        updatemips=1;
}

void error(const char *format, ...)
{
   char buf[256];

   va_list ap;
   va_start(ap, format);
   fprintf(stderr, "RPCemu error: ");
   vfprintf(stderr, format, ap);
   va_end(ap);
}

FILE *arclog;
void rpclog(const char *format, ...)
{
   char buf[256];
   return;
if (!arclog) arclog=fopen("rpclog.txt","wt");
   va_list ap;
   va_start(ap, format);
   vsprintf(buf, format, ap);
   va_end(ap);
   fputs(buf,arclog);
   fflush(arclog);
}

int drawscre=0,flyback;

int infocus;

void vblupdate()
{
        drawscre++;
}

void vidcstartthread(void)
{
}

void vidcendthread(void)
{
}

void vidcwakeupthread(void)
{
        vidcthread();
}

int vidctrymutex(void)
{
        return 1;
}

void vidcreleasemutex(void)
{
}

void sndint()
{
        iomd.state|=0x10;
        updateirqs();
        iomd.sndstat|=6;
        iomd.sndstat^=1;
}

void updatewindowsize(uint32_t x, uint32_t y)
{
  //printf("updatewindowsize: %u %u\n", x, y);

                        if (set_gfx_mode(GRAPHICS_TYPE, x, y, 0, 0))
                        {
                                error("Failed to set gfx mode : %s\n",allegro_error);
                                endrpcemu();
                                exit(-1);
                        }
}

void releasemousecapture()
{
}


int main (void) 
{
int col=0;
        char s[128];
        const char *p;
        char fn[512];
mousehackon=0;
        allegro_init();
        install_keyboard();
        install_timer();
        install_mouse();
infocus=0;
//        arclog=fopen("arclog.txt","wt");
        if (startrpcemu())
           return -1;
	//startblitthread();
        install_int_ex(domips,MSEC_TO_TIMER(1000));
        install_int_ex(vblupdate,BPS_TO_TIMER(refresh));
        if (soundenabled) initsound();
        else              install_int_ex(sndint,BPS_TO_TIMER(50));
        infocus=1;
        while (!quited)
        {
                if (infocus)
                {
//                printf("Starting frame %f\n",mips);
                   execrpcemu();
//                   rectfill(screen,0,0,15,15,col);
//                   col+=0x111;
                   }
                        if (updatemips)
                        {
			  textprintf(screen,font,0,0,makecol(255,255,255),"MIPS: %f (AVG: %f)", mips, mipstotal / (mipscount - 10));
			        //sprintf(s,"RPCemu v0.3 - %f MIPS - %s",mips,(mousecapture)?"Press CTRL-END to release mouse":"Click to capture mouse");
				//                                SetWindowText(ghwnd, s);
                                updatemips=0;
                        }
                if ((key[KEY_LCONTROL] || key[KEY_RCONTROL]) && key[KEY_END]) entergui();
                if ((key[KEY_LCONTROL] || key[KEY_RCONTROL]) && key[KEY_END] && mousecapture)
                {
		  //                        ClipCursor(&oldclip);
                        mousecapture=0;
                        updatemips=1;
                }
        }
        if (mousecapture)
        {
	  //                ClipCursor(&oldclip);
                mousecapture=0;
        }
        endrpcemu();
        return 0;
}

END_OF_MAIN();
