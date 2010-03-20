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

static pthread_t sound_thread;
static pthread_cond_t sound_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t sound_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Function called in sound thread to block
 * on waiting for sound data and trigger copying
 * it to Allegro's sound output buffer
 */
static void *sound_thread_function(void *p)
{
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
			sound_buffer_update();
		}
	}
	pthread_mutex_unlock(&sound_mutex);

	return NULL;
}

/**
 * A signal sent to the sound thread to let it
 * know that more data is available to be put in the
 * output buffer
 */
void sound_thread_wakeup(void)
{
    if (pthread_cond_signal(&sound_cond))
    {
        fatal("Couldn't signal vidc thread");
    }
}

/**
 * Called on program shutdown to tidy up the sound thread
 */
void sound_thread_close(void)
{
	sound_thread_wakeup();
	pthread_join(sound_thread, NULL);
}

/**
 * Called on program startup. Create a thread for copying sound
 * data into Allegro's sound output buffer
 */
void sound_thread_start(void)
{
    if (pthread_create(&sound_thread, NULL, sound_thread_function, NULL))
    {
        fatal("Couldn't create vidc thread");
    }
}

void error(const char *format, ...)
{
	char buf[4096];
	va_list ap;

	va_start(ap, format);
	vsprintf(buf, format, ap);
	va_end(ap);
	rpclog("ERROR: %s\n", buf);
	fprintf(stderr, "RPCemu error: %s\n", buf);
}

// Similar to error() but aborts the program. 
//
// Because of varargs it's not straightforward to just call error()
// from here.  So some code is duplicated.
//
void fatal(const char *format, ...)
{
	char buf[4096];
	va_list ap;

	va_start(ap, format);
	vsprintf(buf, format, ap);
	va_end(ap);
	rpclog("FATAL: %s\n", buf);
	fprintf(stderr, "RPCemu error: %s\n", buf);

	abort();
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

        if (startrpcemu())
           return -1;

        install_int_ex(vblupdate, BPS_TO_TIMER(config.refresh));

        infocus=1;

        while (!quited)
        {
                if (infocus)
                        execrpcemu();
                        if (updatemips)
                        {                           
                                char title[128];

                                sprintf(title, "RPCEmu v" VERSION " - MIPS: %.1f, AVG: %.1f",
                                        mips, mipstotal / mipscount);
                                set_window_title(title);
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
