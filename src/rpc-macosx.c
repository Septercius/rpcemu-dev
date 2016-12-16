/*
  RPCEmu - An Acorn system emulator

  Copyright (C) 2005-2010 Sarah Walker

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Main loop for Mac OS X */

#include <errno.h>
#include <string.h>

#include <pthread.h>
#include <sys/utsname.h>

#include <allegro.h>

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

/**
 * Report a non-fatal error to the user.
 * The user then clicks the continue button to carry on using the program.
 *
 * @param format varargs format
 * @param ... varargs arguments
 */
void error(const char *format, ...)
{
	char buf[4096];
	va_list ap;

	va_start(ap, format);
	vsprintf(buf, format, ap);
	va_end(ap);
	rpclog("ERROR: %s\n", buf);
	fprintf(stderr, "RPCEmu error: %s\n", buf);

	if (gui_get_screen() != NULL) {
		alert("RPCEmu error", buf, "", "&Continue", NULL, 'c', 0);
	}
}

/**
 * Report a fatal error to the user.
 * After the user is informed and clicks the 'Exit' button the program exits.
 *
 * @param format varargs format
 * @param ... varargs arguments
 */
void fatal(const char *format, ...)
{
	char buf[4096];
	va_list ap;

	va_start(ap, format);
	vsprintf(buf, format, ap);
	va_end(ap);
	rpclog("FATAL: %s\n", buf);
	fprintf(stderr, "RPCEmu fatal error: %s\n", buf);

	if (gui_get_screen() != NULL) {
		alert("RPCEmu fatal error", buf, "", "&Exit", NULL, 'c', 0);
	}

	exit(EXIT_FAILURE);
}

/**
 * Log details about the current Operating System version.
 *
 * This function should work on all Unix and Unix-like systems.
 *
 * Called during program start-up.
 */
void
rpcemu_log_os(void)
{
	struct utsname u;

	if (uname(&u) == -1) {
		rpclog("OS: Could not determine: %s\n", strerror(errno));
		return;
	}

	rpclog("OS: SysName = %s\n", u.sysname);
	rpclog("OS: Release = %s\n", u.release);
	rpclog("OS: Version = %s\n", u.version);
	rpclog("OS: Machine = %s\n", u.machine);
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



void vidcstartthread(void)
{
    if (pthread_create(&thread,NULL,vidcthreadrunner,NULL)) fatal("Couldn't create vidc thread");
}

void vidcendthread(void)
{
	quited=1;
        if (pthread_cond_signal(&vidccond)) fatal("Couldn't signal vidc thread");
	pthread_join(thread, NULL);
}

void vidcwakeupthread(void)
{
        if (pthread_cond_signal(&vidccond)) fatal("Couldn't signal vidc thread");
}

int vidctrymutex(void)
{
    int ret = pthread_mutex_trylock(&vidcmutex);
    if (ret == EBUSY) return 0;
    if (ret) fatal("Getting vidc mutex failed");
    return 1;
}

void vidcreleasemutex(void)
{
    if (pthread_mutex_unlock(&vidcmutex)) fatal("Releasing vidc mutex failed");
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
                                        perf.mips, perf.mips_total / perf.mips_count);
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
