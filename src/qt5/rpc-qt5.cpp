/*
  RPCEmu - An Acorn system emulator

  Copyright (C) 2016-2017 Peter Howkins

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
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>

#include <iostream>

#include <QApplication>
#include <QScreen>
#include <QtCore>

#include "main_window.h"
#include "rpc-qt5.h"

#include <pthread.h>
#include <sys/types.h>

#include "rpcemu.h"
#include "mem.h"
#include "sound.h"
#include "vidc20.h"
#include "iomd.h"
#include "keyboard.h"
#include "ide.h"
#include "cdrom-iso.h"

#if defined(Q_OS_WIN32)
#include "cdrom-ioctl.h"
#endif /* win32 */

#if defined(Q_OS_LINUX)
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
extern void ioctl_init(void);
#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
#endif /* linux */

static MainWindow *pMainWin = NULL; ///< Reference to main GUI window
static QThread *gui_thread = NULL; ///< copy of reference to GUI thread

QAtomicInt instruction_count; ///< Instruction counter shared between Emulator and GUI threads
QAtomicInt iomd_timer_count;  ///< IOMD timer  counter shared between Emulator and GUI threads
QAtomicInt video_timer_count; ///< Video timer counter shared between Emulator and GUI threads

static pthread_t sound_thread;
static pthread_cond_t sound_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t sound_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_t video_thread;
static pthread_cond_t video_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t video_mutex = PTHREAD_MUTEX_INITIALIZER;

int mouse_captured = 0;		///< Have we captured the mouse in mouse capture mode
Config *pconfig_copy = NULL;	///< Pointer to frontend copy of config

static Emulator *emulator = NULL;

/**
 * Function called in sound thread to block
 * on waiting for sound data and trigger copying
 * it to Allegro's sound output buffer
 */
static void *
sound_thread_function(void *p)
{
	NOT_USED(p);

	if (pthread_mutex_lock(&sound_mutex)) {
		fatal("Cannot lock mutex");
	}

	while (!quited)	{
		if (pthread_cond_wait(&sound_cond, &sound_mutex)) {
			fatal("pthread_cond_wait failed");
		}
		if (!quited) {
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
void
sound_thread_wakeup(void)
{
	if (pthread_cond_signal(&sound_cond)) {
		fatal("Couldn't signal vidc thread");
	}
}

/**
 * Called on program shutdown to tidy up the sound thread
 */
void
sound_thread_close(void)
{
	sound_thread_wakeup();
	pthread_join(sound_thread, NULL);
}

/**
 * Called on program startup. Create a thread for copying sound
 * data into Allegro's sound output buffer
 */
void
sound_thread_start(void)
{
	if (pthread_create(&sound_thread, NULL, sound_thread_function, NULL)) {
		fatal("Couldn't create sound thread");
	}

#ifdef _GNU_SOURCE
	if (0 != pthread_setname_np(sound_thread, "rpcemu: sound")) {
		fatal("Couldn't set sound thread name");
	}
#endif // _GNU_SOURCE
}

/**
 * Report a non-fatal error to the user.
 * The user then clicks the continue button to carry on using the program.
 *
 * @param format varargs format
 * @param ... varargs arguments
 */
void
error(const char *format, ...)
{
	char buf[4096];
	va_list ap;

	va_start(ap, format);
	vsprintf(buf, format, ap);
	va_end(ap);
	rpclog("ERROR: %s\n", buf);
	fprintf(stderr, "RPCEmu error: %s\n", buf);

	// Handle displaying error to user in GUI
	if (gui_thread != NULL) {
		if (QThread::currentThread() == gui_thread) {
			// We're in the GUI thread display error here
			pMainWin->error(buf);
		} else {
			// We're in a background thread, throw message to GUI thread
			emit pMainWin->error_signal(buf);
		}
	}
}

/**
 * Report a fatal error to the user.
 * After the user is informed and clicks the 'Exit' button the program exits.
 *
 * @param format varargs format
 * @param ... varargs arguments
 */
void
fatal(const char *format, ...)
{
	char buf[4096];
	va_list ap;

	va_start(ap, format);
	vsprintf(buf, format, ap);
	va_end(ap);
	rpclog("FATAL: %s\n", buf);

	fprintf(stderr, "RPCEmu fatal error: %s\n", buf);

	// If there is not a gui running, no more work to do
	if (gui_thread == NULL) {
		exit(EXIT_FAILURE);
	}

	// Handle displaying error to user in GUI
	if (QThread::currentThread() == gui_thread) {
		// We're in the GUI thread display error here
		pMainWin->fatal(buf);
		// This will exit the program in that function
	} else {
		// We're in a background thread, throw message to GUI thread
		emit pMainWin->fatal_signal(buf);
		// This will exit the program in the GUI thread
	}

	// This function cannot return within causing issues with
	// emu thread processor events, use this to block here
	gui_thread->wait();

	// This is a fallback to prevent a warning about returning 
	// from a noreturn function
	exit(EXIT_FAILURE);
}


/**
 * Callback on video refresh rate timer
 */
static void
vblupdate(void)
{
	drawscre++;
}

/**
 * Function called in video thread to block
 * on waiting for video data and trigger copying
 * it from VRAM to video buffer
 */
static void *
vidcthreadrunner(void *threadid)
{
	NOT_USED(threadid);

	if (pthread_mutex_lock(&video_mutex)) {
		fatal("Cannot lock mutex");
	}

	while (!quited) {
		if (pthread_cond_wait(&video_cond, &video_mutex)) {
			fatal("pthread_cond_wait failed");
		}
		if (!quited) {
			vidcthread();
		}
	}

	pthread_mutex_unlock(&video_mutex);

	return NULL;
}


extern "C" {


/**
 * Called on program startup. Create a thread for copying video
 * data from VRAM into a video buffer
 */
void
vidcstartthread(void)
{
	if (pthread_create(&video_thread, NULL, vidcthreadrunner, NULL)) {
		fatal("Couldn't create vidc thread");
	}

#ifdef _GNU_SOURCE
	if (0 != pthread_setname_np(video_thread, "rpcemu: vidc")) {
		fatal("Couldn't set vidc thread name");
	}
#endif // _GNU_SOURCE
}

/**
 * Called on program shutdown to tidy up video thread
 */
void
vidcendthread(void)
{
//	quited = 1;
//	if (pthread_cond_signal(&video_cond)) {
//		fatal("Couldn't signal vidc thread");
//	}
//	pthread_join(video_thread, NULL);
}

/**
 * A signal sent to the video thread to let it
 * know that more data is available to be put in the
 * output video buffer
 */
void
vidcwakeupthread(void)
{
	if (pthread_cond_signal(&video_cond)) {
		fatal("Couldn't signal vidc thread");
	}
}

int
vidctrymutex(void)
{
	int ret = pthread_mutex_trylock(&video_mutex);
	if (ret == EBUSY) {
		return 0;
	}
	if (ret) {
		fatal("Getting vidc mutex failed");
	}
	return 1;
}

void
vidcreleasemutex(void)
{
	if(pthread_mutex_unlock(&video_mutex)) {
		fatal("Releasing vidc mutex failed");
	}
}

/**
 * Prepare and send a video update message to the GUI.
 *
 * @param buffer      Pointer to image buffer
 * @param xsize       X size of buffer
 * @param ysize       Y size of buffer
 * @param yl          Y low range of area to update
 * @param yh          Y high range of area to update
 * @param double_size Current state of doubling X/Y values
 * @param host_xsize  X pixel size of display including any double_size doubling
 * @param host_ysize  Y pixel size of display including any double_size doubling
 */
void
rpcemu_video_update(const uint32_t *buffer, int xsize, int ysize,
                    int yl, int yh, int double_size, int host_xsize, int host_ysize)
{
	VideoUpdate video_update;

	// Prepare update message
	//   Wrap the buffer in a QImage container:
	video_update.image = QImage((uchar *) buffer,
	    xsize, ysize, QImage::Format_RGB32);
	video_update.yl = yl;
	video_update.yh = yh;
	video_update.double_size = double_size;
	video_update.host_xsize = host_xsize;
	video_update.host_ysize = host_ysize;

	// Send update message to GUI
	emit pMainWin->main_display_signal(video_update);
}

/**
 * Helper function to call the idle_process_events() method on the
 * Emulator object from C.
 */
void
rpcemu_idle_process_events(void)
{
	emulator->idle_process_events();
}

} // extern "C"

/**
 * Program entry point
 *
 * @param argc command line arguments
 * @param argv command line arguments
 */ 
int main (int argc, char ** argv) 
{ 
//	if (argc != 1) {
//		fprintf(stderr, "No command line options supported.\n");
//		return 1;
//	}

	// Initialise QT app
	QApplication app(argc, argv);

	// Add a program icon
	QApplication::setWindowIcon(QIcon(":/rpcemu_icon.png"));
	
	// start enough of the emulator system to allow
	// the GUI to initialise (e.g. load the config to init
	// the configure window)
	rpcemu_prestart();

	// Allow additional types to be passed in slots and signals
	qRegisterMetaType<Model>("Model");
	qRegisterMetaType<VideoUpdate>("VideoUpdate");

	// Create Emulator Thread and Object
	QThread *emu_thread = new QThread;
	emu_thread->setObjectName("rpcemu: emu");

	emulator = new Emulator;
	emulator->moveToThread(emu_thread);
	QThread::connect(emu_thread, &QThread::started, emulator, &Emulator::mainemuloop);
	QThread::connect(emulator, &Emulator::finished, emu_thread, &QThread::quit);
	QThread::connect(emulator, &Emulator::finished, emulator, &Emulator::deleteLater);
	QThread::connect(emu_thread, &QThread::finished, emu_thread, &QThread::deleteLater);

	// Create Main Window
	MainWindow main_window(*emulator);
	pMainWin = &main_window;

	// Show Main Window
	main_window.show();

	// Store a reference to the GUI thread
	// Needed to handle displaying fatal() and error()
	// in the GUI
	gui_thread = QThread::currentThread();

	// Initialise emulator system
	rpcemu_start();

	// Start Emulator Thread
	emu_thread->start();

	// Start main gui thread running
	return app.exec();
}

/**
 * Emulator class constructor
 * Connect up QT signals
 */
Emulator::Emulator()
{
	// Signals from the main GUI window to provide emulated machine input
	connect(this, &Emulator::key_press_signal,
	        this, &Emulator::key_press);

	connect(this, &Emulator::key_release_signal,
	        this, &Emulator::key_release);

	connect(this, &Emulator::mouse_move_signal, this, &Emulator::mouse_move);
	connect(this, &Emulator::mouse_move_relative_signal, this, &Emulator::mouse_move_relative);
	connect(this, &Emulator::mouse_press_signal, this, &Emulator::mouse_press);
	connect(this, &Emulator::mouse_release_signal, this, &Emulator::mouse_release);

	// Signals from user GUI interactions to control parts of the emulator
	connect(this, &Emulator::reset_signal, this, &Emulator::reset);
	connect(this, &Emulator::exit_signal, this, &Emulator::exit);
	connect(this, &Emulator::load_disc_0_signal, this, &Emulator::load_disc_0);
	connect(this, &Emulator::load_disc_1_signal, this, &Emulator::load_disc_1);
	connect(this, &Emulator::cpu_idle_signal, this, &Emulator::cpu_idle);
	connect(this, &Emulator::cdrom_disabled_signal, this, &Emulator::cdrom_disabled);
	connect(this, &Emulator::cdrom_empty_signal, this, &Emulator::cdrom_empty);
	connect(this, &Emulator::cdrom_load_iso_signal, this, &Emulator::cdrom_load_iso);
#if defined(Q_OS_LINUX)
	connect(this, &Emulator::cdrom_ioctl_signal, this, &Emulator::cdrom_ioctl);
#endif // linux
#if defined(Q_OS_WIN32)
	connect(this, &Emulator::cdrom_win_ioctl_signal, this, &Emulator::cdrom_win_ioctl);
#endif // win32
	connect(this, &Emulator::mouse_hack_signal, this, &Emulator::mouse_hack);
	connect(this, &Emulator::mouse_twobutton_signal, this, &Emulator::mouse_twobutton);
	connect(this, &Emulator::config_updated_signal, this, &Emulator::config_updated);
}

/**
 * Main thread function of the Emulator thread
 */
void
Emulator::mainemuloop()
{
	const int32_t iomd_timer_interval = 2000000; // 2000000 ns = 2 ms (500 Hz)
	video_timer_interval = 1000000000 / config.refresh;

	iomd_timer_next = (qint64) iomd_timer_interval; // Time after which the IOMD timer should trigger
	video_timer_next = (qint64) video_timer_interval;

	elapsed_timer.start();

	while (!quited) {
		// Handle qt events and messages
		QCoreApplication::processEvents();

		// Run some instructions in the emulator
		execrpcemu();

		const qint64 elapsed = elapsed_timer.nsecsElapsed();

		// If we have passed the time the IOMD timer event should occur, trigger it
		if (elapsed >= iomd_timer_next) {
			iomd_timer_count.fetchAndAddRelease(1);
			gentimerirq();
			iomd_timer_next += (qint64) iomd_timer_interval;
		}

		// If we have passed the time the Video timer event should occur, trigger it
		if (elapsed >= video_timer_next) {
			video_timer_count.fetchAndAddRelease(1);
			vblupdate();
			video_timer_next += (qint64) video_timer_interval;
		}

		// If the instruction count is greater than 100000, update the shared counter
		if (inscount >= 100000) {
			instruction_count.fetchAndAddRelease((int) inscount);
			inscount = 0;
		}
	}

	// Perform clean-up and finalising actions
	endrpcemu();

	emit finished();
}

/**
 * Process events for the CPU idle routine.
 */
void
Emulator::idle_process_events()
{
	const int32_t iomd_timer_interval = 2000000; // 2000000 ns = 2 ms (500 Hz)

	// Handle qt events and messages
	QCoreApplication::processEvents();

	const qint64 elapsed = elapsed_timer.nsecsElapsed();

	// If we have passed the time the IOMD timer event should occur, trigger it
	if (elapsed >= iomd_timer_next) {
		iomd_timer_count.fetchAndAddRelease(1);
		gentimerirq();
		iomd_timer_next += (qint64) iomd_timer_interval;
	}

	// If we have passed the time the Video timer event should occur, trigger it
	if (elapsed >= video_timer_next) {
		video_timer_count.fetchAndAddRelease(1);
		vblupdate();
		video_timer_next += (qint64) video_timer_interval;
	}
}

/**
 * Key pressed
 * 
 * @param scan_code QT native host key code
 */  
void
Emulator::key_press(unsigned scan_code)
{
	const uint8_t *scan_codes = keyboard_map_key(scan_code);
	keyboard_key_press(scan_codes);
}

/**
 * Key released
 * 
 * @param scan_code QT native host key code
 */  
void
Emulator::key_release(unsigned scan_code)
{
	const uint8_t *scan_codes = keyboard_map_key(scan_code);

	// Break key has no release code
	if(scan_codes && scan_codes[0] == 0xe1) {
		return;
	}

	keyboard_key_release(scan_codes);
}

/**
 * Mouse has moved in absolute position (mousehack mode)
 * 
 * @param x new x position
 * @param y new y position
 */ 
void
Emulator::mouse_move(int x, int y)
{
	mouse_mouse_move(x, y);
}
/**
 * Mouse has moved in relative position (mousecapture mode)
 * 
 * @param dx change in x pos
 * @param dy change in y pos
 */ 
void
Emulator::mouse_move_relative(int dx, int dy)
{
	mouse_mouse_move_relative(dx, dy);
}

/**
 * Mouse button pressed
 * 
 * @param buttons buttons pressed (QT format)
 */
void
Emulator::mouse_press(int buttons)
{
	mouse_mouse_press(buttons);
}

/**
 * Mouse button released
 *
 * @param buttons buttons pressed (QT format)
 */
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
 * User hit exit on GUI menu
 */
void
Emulator::exit()
{
	// Tell the main emulator loop to end
	// This causes the emulator thread run() function to end
	// It should also cause the vidc and sound threads to end
	quited = 1;

	// Kill emulator thread
	// This wakes up the GUI thread to continue the exit process
	this->thread()->quit();
}

/**
 * GUI wants to change disc image in floppy drive 0
 *
 * @param discname filename of disc image
 */
void
Emulator::load_disc_0(QString discname)
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
Emulator::load_disc_1(QString discname)
{
	const char *p;
	QByteArray ba;

	ba = discname.toUtf8();
	p = ba.data();

	rpcemu_floppy_load(1, p);
}

/**
 * GUI is toggling the CPU idling feature
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
	if(config.cdromenabled) {
		config.cdromenabled = 0;
		resetrpc();
	}
}

/**
 * GUI wants to empty/eject cdrom drive
 */
void
Emulator::cdrom_empty()
{
	if (!config.cdromenabled) {
		config.cdromenabled = 1;
		resetrpc();
	}

	atapi->exit();
	iso_init();
}

/**
 * GUI wants to change iso image in cdrom drive
 * 
 * @param discname filename of iso image
 */
void
Emulator::cdrom_load_iso(QString discname)
{
	const char *p;
	QByteArray ba;

	if(!config.cdromenabled) {
		config.cdromenabled = 1;
		resetrpc();
	}

	ba = discname.toUtf8();
	p = ba.data();

	strcpy(config.isoname, p);
	atapi->exit();
	iso_open(config.isoname);
}

#if defined(Q_OS_LINUX)
/**
 * GUI wants to use Linux real cdrom drive
 */
void
Emulator::cdrom_ioctl()
{
	if(!config.cdromenabled) {
		config.cdromenabled = 1;
		resetrpc();
	}

	atapi->exit();
	ioctl_init();
}
#endif // linux

#if defined(Q_OS_WIN32)
/**
 * GUI wants to use Windows real cdrom drive
 *
 * @param drive_letter drive letter of cdrom drive
 */
void
Emulator::cdrom_win_ioctl(char drive_letter)
{
	if(!config.cdromenabled) {
		config.cdromenabled = 1;
		resetrpc();
	}

	atapi->exit();
	ioctl_open(drive_letter);
}
#endif // win32

/**
 * GUI is toggling mousehack (follows host mouse)
 */
void
Emulator::mouse_hack()
{
	config.mousehackon ^= 1;
}

/**
 * GUI is toggling two button mouse mode
 */
void
Emulator::mouse_twobutton()
{
	config.mousetwobutton ^= 1;
}

/**
 * GUI is requesting setting of new config
 * 
 * @param new_config new config settings
 * @param new_model  new config settings
 */
void
Emulator::config_updated(Config *new_config, Model new_model)
{
	rpcemu_config_apply_new_settings(new_config, new_model);

	video_timer_interval = 1000000000 / config.refresh;

	// The new_config was created for the emulator thread in gui thread, this
	// function must free it
	free(new_config);
}

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** 
 * On startup log specific details related to QT
 */ 
void
rpcemu_log_platform(void)
{
	/* version of qt5 this app is running on */
	rpclog("QT5: %s\n", qVersion());

	/* Log display information */
	rpclog("Number of screens: %d\n", QGuiApplication::screens().size());
	rpclog("Primary screen: %s\n" , QGuiApplication::primaryScreen()->name().toLocal8Bit().constData());

	foreach (QScreen *screen, QGuiApplication::screens()) {
		rpclog("Information for screen: %s\n", screen->name().toLocal8Bit().constData());
		rpclog(" Resolution: %d x %d\n", screen->size().width(), screen->size().height());
		rpclog(" Colour depth: %d\n", screen->depth());
	}
}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
