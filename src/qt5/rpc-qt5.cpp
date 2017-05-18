/*RPCemu v0.6 by Tom Walker
  Main loop
  Not just for Linux - works as a Win32 console app as well*/

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>

#include <iostream>

#include <QApplication>
#include <QtCore>

#include "main_window.h"
#include "rpc-qt5.h"
#include "iomdtimer.h"

#include <pthread.h>
#include <sys/types.h>

#include "rpcemu.h"
#include "mem.h"
#include "sound.h"
#include "vidc20.h"
#include "gui.h"
#include "iomd.h"
#include "keyboard.h"
#include "ide.h"
#include "cdrom-iso.h"

MainWindow *pMainWin;

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

//	if (gui_get_screen() != NULL) {
//		alert("RPCEmu error", buf, "", "&Continue", NULL, 'c', 0);
//	}
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

//	if (gui_get_screen() != NULL) {
//		alert("RPCEmu fatal error", buf, "", "&Exit", NULL, 'c', 0);
//	}

	exit(EXIT_FAILURE);
}


static void vblupdate(void)
{
        if (infocus) drawscre++;
}

void updatewindowsize(uint32_t x, uint32_t y)
{
	fprintf(stderr, "Win Size %u %u\n", x, y);
}

void releasemousecapture(void)
{
}

static pthread_t thread;
static pthread_cond_t vidccond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t vidcmutex = PTHREAD_MUTEX_INITIALIZER;

static void *vidcthreadrunner(void *threadid)
{
	NOT_USED(threadid);

        if (pthread_mutex_lock(&vidcmutex)) fatal("Cannot lock mutex");
	while (!quited)
	{
                if (pthread_cond_wait(&vidccond, &vidcmutex)) fatal("pthread_cond_wait failed");
		if (!quited) vidcthread();
	}
        pthread_mutex_unlock(&vidcmutex);
	return NULL;
}


extern "C" {


void vidcstartthread(void)
{
    if (pthread_create(&thread,NULL,vidcthreadrunner,NULL)) {
      fatal("Couldn't create vidc thread");
    }

#ifdef _GNU_SOURCE
    if(0 != pthread_setname_np(thread, "rpcemu: vidc")) {
      fatal("Couldn't set vidc thread name");
    }
#endif /* _GNU_SOURCE */
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
} /* extern "C" */

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

//static void close_button_handler(void)
//{
//  quited = TRUE;
//}
//END_OF_FUNCTION(close_button_handler)


int main (int argc, char ** argv) 
{ 
//	if (argc != 1)
//	{
//		fprintf(stderr, "No command line options supported.\n");
//		return 1;
//	}

//	install_sigchld_handler();


	QApplication app(argc, argv);

	//qRegisterMetaType<uint32_t>();
	//qRegisterMetaType<QKeyEvent>("foo");


	if (startrpcemu()) {
		fatal("startrpcemu() failed");
	}

	/* TIMER CALLBACKS like this CAN NOT BE IN A SUB THREAD */
	/* I have no idea why, but it can't be in EmuThread */
// HACKCORE
//        install_int_ex(vblupdate, BPS_TO_TIMER(config.refresh));
	VLBUpdateTimer timer;
	timer.start(1000 / config.refresh);

	IOMDTimer iomdtimer;
	iomdtimer.start(2); /* 2ms = 500Hz */

	// Create Emulator Thread and Object
	QThread *emu_thread = new QThread;
	Emulator *emulator = new Emulator;
	emulator->moveToThread(emu_thread);
	QThread::connect(emu_thread, &QThread::started,
	                 emulator, &Emulator::mainemuloop);

	// Create Main Window
	MainWindow main_window(*emulator);
	pMainWin = &main_window;

	// Start Emulator Thread
	emu_thread->start();

	// Show Main Window
	main_window.show();

	return app.exec();
}

VLBUpdateTimer::VLBUpdateTimer(QObject *parent)
    : QTimer(parent)
{
	connect(this, SIGNAL(timeout()), this, SLOT(VLBUpdate()));
}

void
VLBUpdateTimer::VLBUpdate()
{
//	fprintf(stderr, "V");
	vblupdate();
}

IOMDTimer::IOMDTimer(QObject *parent)
    : QTimer(parent)
{
	connect(this, SIGNAL(timeout()), this, SLOT(IOMDUpdate()));
}

void
IOMDTimer::IOMDUpdate()
{
//	fprintf(stderr, "I");
	gentimerirq();
}

Emulator::Emulator()
{
	// Signals from the main GUI window to provide emulated machine input
	connect(this, &Emulator::key_press_signal,
	        this, &Emulator::key_press);

	connect(this, &Emulator::key_release_signal,
	        this, &Emulator::key_release);

	connect(this, &Emulator::mouse_move_signal, this, &Emulator::mouse_move);
	connect(this, &Emulator::mouse_press_signal, this, &Emulator::mouse_press);
	connect(this, &Emulator::mouse_release_signal, this, &Emulator::mouse_release);

	// Signals from user GUI interactions to control parts of the emulator
	connect(this, &Emulator::reset_signal, this, &Emulator::reset);
	connect(this, &Emulator::load_disc_0_signal, this, &Emulator::load_disc_0);
	connect(this, &Emulator::load_disc_1_signal, this, &Emulator::load_disc_1);
	connect(this, &Emulator::cpu_idle_signal, this, &Emulator::cpu_idle);
	connect(this, &Emulator::cdrom_disabled_signal, this, &Emulator::cdrom_disabled);
	connect(this, &Emulator::cdrom_empty_signal, this, &Emulator::cdrom_empty);
	connect(this, &Emulator::cdrom_load_iso_signal, this, &Emulator::cdrom_load_iso);
	connect(this, &Emulator::mouse_hack_signal, this, &Emulator::mouse_hack);
	connect(this, &Emulator::mouse_capture_signal, this, &Emulator::mouse_capture);
	connect(this, &Emulator::mouse_twobutton_signal, this, &Emulator::mouse_twobutton);
}

/**
 * Main thread function of the Emulator thread
 */
void
Emulator::mainemuloop()
{
	infocus = 1;

	while (!quited) {

		QCoreApplication::processEvents();


		if (infocus) {
			execrpcemu();
		}
		/*
		if (updatemips) {
			char title[128];

			sprintf(title, "RPCEmu v" VERSION " - MIPS: %.1f, AVG: %.1f",
				perf.mips, perf.mips_total / perf.mips_count);
			updatemips = 0;
		}*/
	}
	if (mousecapture)
	{
		mousecapture=0;
	}
	endrpcemu();
}

void
Emulator::key_press(unsigned scan_code)
{
	const uint8_t *scan_codes = keyboard_map_key(scan_code);
	keyboard_key_press(scan_codes);
}

void
Emulator::key_release(unsigned scan_code)
{
	const uint8_t *scan_codes = keyboard_map_key(scan_code);
	keyboard_key_release(scan_codes);
}

void
Emulator::mouse_move(int x, int y)
{
	mouse_mouse_move(x, y);
}

void
Emulator::mouse_press(int buttons)
{
	mouse_mouse_press(buttons);
}

void
Emulator::mouse_release(int buttons)
{
	mouse_mouse_release(buttons);
}

/**
 * User hit reset on GUI menu
 */
void
Emulator::reset()
{
	resetrpc();
}

/**
 * GUI wants to change disc image in floppy drive 0
 *
 * @param discname filename of disc image
 */
void
Emulator::load_disc_0(const QString &discname)
{
	const char *p;
	QByteArray ba;

	ba = discname.toUtf8();
	p = ba.data();

	rpcemu_floppy_load(0, p);
}

/**
 * GUI wants to change disc image in floppy drive 1
 *
 * @param discname filename of disc image
 */
void
Emulator::load_disc_1(const QString &discname)
{
	const char *p;
	QByteArray ba;

	ba = discname.toUtf8();
	p = ba.data();

	rpcemu_floppy_load(1, p);
}

/**
 * GUI is the CPU idling feature
 */
void
Emulator::cpu_idle()
{
	config.cpu_idle ^= 1;
	resetrpc();
}

/**
 * GUI wants to disable cdrom drive
 */
void
Emulator::cdrom_disabled()
{
	std::cout << "CDROM disabled clicked" << std::endl;
}

/**
 * GUI wants to empty/eject cdrom drive
 */
void
Emulator::cdrom_empty()
{
	std::cout << "CDROM empty clicked" << std::endl;
}

/**
 * GUI wants to change iso image in cdrom drive
 * 
 * @param discname filename of iso image
 */
void
Emulator::cdrom_load_iso(const QString &discname)
{
	const char *p;
	QByteArray ba;

	ba = discname.toUtf8();
	p = ba.data();

	strcpy(config.isoname, p);
	atapi->exit();
	iso_open(config.isoname);
}

/**
 * GUI is toggling mousehack (follows host mouse)
 */
void
Emulator::mouse_hack()
{
	config.mousehackon ^= 1;
}

/**
 * GUI is toggling capture mouse mode
 */
void
Emulator::mouse_capture()
{
	std::cout << "mouse capture clicked" << std::endl;
}

/**
 * GUI is toggling two button mouse mode
 */
void
Emulator::mouse_twobutton()
{
	config.mousetwobutton ^= 1;
}
