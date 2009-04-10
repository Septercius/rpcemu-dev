/*RPCemu v0.6 by Tom Walker
  Main loop
  Not just for Linux - works as a Win32 console app as well*/

#include "config.h"

#include <allegro.h>
#include <pthread.h>
#include "rpcemu.h"
#include "mem.h"
#include "sound.h"
#include "vidc20.h"
#include "gui.h"
#include "network-linux.h"


static float mips;
static int updatemips = 0;
static pthread_t sound_thread;
static pthread_cond_t sound_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t sound_mutex = PTHREAD_MUTEX_INITIALIZER;

static uint32_t mipscount;
static float mipstotal;

static void *_soundthread(void *p)
{
	int c;

        if (pthread_mutex_lock(&sound_mutex))
	{
		fatal("Cannot lock mutex");
	}
	while (!quited)
	{
                if (pthread_cond_wait(&sound_cond, &sound_mutex))
		{
			fatal("pthread_cond_wait failed");
		}
		if (!quited)
		{
			do {
				c = updatesoundbuffer();
			} while(c);
		}
	}
        pthread_mutex_unlock(&sound_mutex);

	return NULL;
}

void wakeupsoundthread(void)
{
    if (pthread_cond_signal(&sound_cond))
    {
        fatal("Couldn't signal vidc thread");
    }
}

void closesoundthread(void)
{
	wakeupsoundthread();
	pthread_join(sound_thread, NULL);
}

static void startsoundthread(void)
{
    if (pthread_create(&sound_thread,NULL,_soundthread,NULL))
    {
        fatal("Couldn't create vidc thread");
    }
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
   va_list ap;
   va_start(ap, format);
   fprintf(stderr, "RPCemu error: ");
   vfprintf(stderr, format, ap);
   fprintf(stderr,"\n");
   va_end(ap);
}

// Similar to error() but aborts the program. 
//
// Because of varargs it's not straightforward to just call error()
// from here.  So some code is duplicated.
//
void fatal(const char *format, ...)
{
   va_list ap;
   va_start(ap, format);
   fprintf(stderr, "RPCemu error: ");
   vfprintf(stderr, format, ap);
   fprintf(stderr,"\n");
   va_end(ap);

   abort();
}

static FILE *arclog;
void rpclog(const char *format, ...)
{
   char buf[256];
//   return;
if (!arclog) arclog=fopen("rpclog.txt","wt");
   va_list ap;
   va_start(ap, format);
   vsprintf(buf, format, ap);
   va_end(ap);
   fputs(buf,arclog);
}

static void vblupdate(void)
{
        if (infocus) drawscre++;
}

void updatewindowsize(uint32_t x, uint32_t y)
{
}

void releasemousecapture(void)
{
}

#ifdef VIDC_THREAD
static pthread_t thread;
static pthread_cond_t vidccond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t vidcmutex = PTHREAD_MUTEX_INITIALIZER;

static void *vidcthreadrunner(void *threadid)
{
        if (pthread_mutex_lock(&vidcmutex)) fatal("Cannot lock mutex");
	while (!quited)
	{
                if (pthread_cond_wait(&vidccond, &vidcmutex)) fatal("pthread_cond_wait failed");
		if (!quited) vidcthread();
	}
        pthread_mutex_unlock(&vidcmutex);
	return NULL;
}
#endif



void vidcstartthread(void)
{
#ifdef VIDC_THREAD
    if (pthread_create(&thread,NULL,vidcthreadrunner,NULL)) fatal("Couldn't create vidc thread");
#endif
}

void vidcendthread(void)
{
#ifdef VIDC_THREAD
	quited=1;
        if (pthread_cond_signal(&vidccond)) fatal("Couldn't signal vidc thread");
	pthread_join(thread, NULL);
#endif
}

void vidcwakeupthread(void)
{
#ifdef VIDC_THREAD
        if (pthread_cond_signal(&vidccond)) fatal("Couldn't signal vidc thread");
#else
        vidcthread();
#endif
}

int vidctrymutex(void)
{
#ifdef VIDC_THREAD
    int ret = pthread_mutex_trylock(&vidcmutex);
    if (ret == EBUSY) return 0;
    if (ret) fatal("Getting vidc mutex failed");
#endif
    return 1;
}

void vidcreleasemutex(void)
{
#ifdef VIDC_THREAD
    if (pthread_mutex_unlock(&vidcmutex)) fatal("Releasing vidc mutex failed");
#endif
}

static void close_button_handler(void)
{
  quited = TRUE;
}
END_OF_FUNCTION(close_button_handler)


int main (int argc, char ** argv) 
{ 
	if (argc != 1)
	{
		fprintf(stderr, "No command line options supported.\n");
		return 1;
	}

        infocus=1;
        allegro_init();

        set_window_title("RPCEmu v" VERSION);

	LOCK_FUNCTION(close_button_handler);
	set_close_button_callback(close_button_handler);

        install_keyboard();
        install_timer();
        install_mouse();

        if (startrpcemu())
           return -1;

        initnetwork();

        install_int_ex(domips,MSEC_TO_TIMER(1000));
        install_int_ex(vblupdate,BPS_TO_TIMER(refresh));
	startsoundthread();
        if (soundenabled) initsound();
        infocus=1;
        mousehackon=1;

        while (!quited)
        {
                if (infocus)
                        execrpcemu();
                        if (updatemips)
                        {                           
                                printf("MIPS: %f (AVG: %f) %i\n", mips, mipstotal / (mipscount - 10),mousehack);
                                updatemips=0;
                        }
                if ((key[KEY_LCONTROL] || key[KEY_RCONTROL]) && key[KEY_END]) entergui();
                if ((key[KEY_LCONTROL] || key[KEY_RCONTROL]) && key[KEY_END] && mousecapture)
                {
                        mousecapture=0;
                        updatemips=1;
                }
        }
        if (mousecapture)
        {
                mousecapture=0;
        }
        endrpcemu();
        return 0;
}

END_OF_MAIN();
