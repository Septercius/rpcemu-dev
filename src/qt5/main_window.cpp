/*
  RPCEmu - An Acorn system emulator

  Copyright (C) 2016-2017 Matthew Howkins

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
#include <iostream>
#include <list>

#include <QGuiApplication>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>

#if defined(Q_OS_WIN32)
#include "Windows.h"
#endif /* Q_OS_WIN32 */

#if defined(Q_OS_MACOS)
#include "macosx/events-macosx.h"
#include "keyboard_macosx.h"
#endif /* Q_OS_MACOS */

#include "rpcemu.h"
#include "keyboard.h"
#include "main_window.h"
#include "rpc-qt5.h"
#include "vidc20.h"

#define URL_MANUAL	"http://www.marutan.net/rpcemu/manual/"
#define URL_WEBSITE	"http://www.marutan.net/rpcemu/"

MainDisplay::MainDisplay(Emulator &emulator, QWidget *parent)
    : QWidget(parent),
      emulator(emulator),
      double_size(VIDC_DOUBLE_NONE),
      full_screen(false)
{
	assert(pconfig_copy);

	image = new QImage(640, 480, QImage::Format_RGB32);

	// No need to erase to background colour before painting
	this->setAttribute(Qt::WA_OpaquePaintEvent);

	// Hide pointer in mouse hack mode
	if(pconfig_copy->mousehackon) {
		this->setCursor(Qt::BlankCursor);
	}
		
	calculate_scaling();
}

void
MainDisplay::mouseMoveEvent(QMouseEvent *event)
{
    // Ignore mouse events if the application is terminating.
    if (quited) return;
    
	if((!pconfig_copy->mousehackon && mouse_captured) || full_screen) {
		QPoint middle;

		// In mouse capture mode move the mouse back to the middle of the window */ 
		middle.setX(this->width() / 2);
		middle.setY(this->height() / 2);

		QCursor::setPos(this->mapToGlobal(middle));

		// Calculate relative deltas based on difference from centre of display widget
		int dx = event->x() - middle.x();
		int dy = event->y() - middle.y();

		emit this->emulator.mouse_move_relative_signal(dx, dy);
	} else if(pconfig_copy->mousehackon) {
		// Follows host mouse (mousehack) mode
		emit this->emulator.mouse_move_signal(event->x(), event->y());
	}

}

void
MainDisplay::mousePressEvent(QMouseEvent *event)
{
    // Ignore mouse events if the application is terminating.
    if (quited) return;
    
	// Handle turning on mouse capture in capture mouse mode
	if(!pconfig_copy->mousehackon) {
		if(!mouse_captured) {
			mouse_captured = 1;

			// Hide pointer in mouse capture mode when it's been captured
			this->setCursor(Qt::BlankCursor);

			return;
		}
	}

	if (event->button() & 7) {
		emit this->emulator.mouse_press_signal(event->button() & 7);
	}
}

void
MainDisplay::mouseReleaseEvent(QMouseEvent *event)
{
    // Ignore mouse events if the application is terminating.
    if (quited) return;
    
	if (event->button() & 7) {
		emit this->emulator.mouse_release_signal(event->button() & 7);
	}
}

void
MainDisplay::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);

	if(full_screen) {
		painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
	} else {
		painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
	}
	
	const QRect dest = event->rect();
	QRect source;

	switch (double_size) {
	case VIDC_DOUBLE_NONE:
		source = dest;
		break;
	case VIDC_DOUBLE_X:
		source = QRect(dest.x() / 2, dest.y(), dest.width() / 2, dest.height());
		break;
	case VIDC_DOUBLE_Y:
		source = QRect(dest.x(), dest.y() / 2, dest.width(), dest.height() / 2);
		break;
	case VIDC_DOUBLE_BOTH:
		source = QRect(dest.x() / 2, dest.y() / 2, dest.width() / 2, dest.height() / 2);
		break;
	}

	if (full_screen) {
		if ((dest.x() < offset_x) || (dest.y() < offset_y)) {
			painter.fillRect(dest, Qt::black);
		}

		const QRect rect(offset_x, offset_y, scaled_x, scaled_y);
		painter.drawImage(rect, *image);
	} else {
		painter.drawImage(dest, *image, source);
	}
}

void
MainDisplay::resizeEvent(QResizeEvent *)
{
	calculate_scaling();
}

void
MainDisplay::get_host_size(int& xsize, int& ysize) const
{
	xsize = this->host_xsize;
	ysize = this->host_ysize;
}

void
MainDisplay::set_full_screen(bool full_screen)
{
	this->full_screen = full_screen;

	calculate_scaling();
}

void
MainDisplay::update_image(const QImage& img, int yl, int yh, int double_size)
{
	bool recalculate_needed = false;

	if (img.size() != image->size()) {
		// Re-create image with new size and copy of data
		*(this->image) = img.copy();

		recalculate_needed = true;

	} else {
		// Copy just the data that has changed
		const void *src = img.scanLine(yl);
		void *dest = image->scanLine(yl);

		const int lines = yh - yl;
		const int bytes = img.bytesPerLine() * lines;

		memcpy(dest, src, (size_t) bytes);
	}

	if (double_size != this->double_size) {
		this->double_size = double_size;
		recalculate_needed = true;
	}

	if (recalculate_needed) {
		calculate_scaling();
		this->update();
		return;
	}

	// Trigger repaint of changed region
	int width = image->width();
	int ymin = yl;
	int ymax = yh;

	if (double_size & VIDC_DOUBLE_X) {
		width *= 2;
	}
	if (double_size & VIDC_DOUBLE_Y) {
		ymin *= 2;
		ymax *= 2;
	}

	if (full_screen) {
		width = (width * scaled_x) / host_xsize;

		/* For the Pixmap Smoothing to work properly, the height
		 * needs to be expanded by one pixel to avoid visual
		 * artifacts */
		if (ymin > 0) {
			ymin--;
		}
		if (ymax < host_ysize) {
			ymax++;
		}

		// calculate 'ymin' rounded down, 'ymax' rounded up
		ymin = (ymin * scaled_y) / host_ysize;
		ymax = ((ymax * scaled_y) + host_ysize - 1) / host_ysize;

		int height = ymax - ymin;
		this->update(offset_x, ymin + offset_y, width, height);
	} else {
		int height = ymax - ymin;
		this->update(0, ymin, width, height);
	}
}

/**
 * Called to update the image scaling.
 *
 * Called when any of the following change:
 * - Image size
 * - Double-size
 * - Windowed or Full screen
 * - Widget size
 */
void
MainDisplay::calculate_scaling()
{
	if (double_size & VIDC_DOUBLE_X) {
		host_xsize = image->width() * 2;
	} else {
		host_xsize = image->width();
	}
	if (double_size & VIDC_DOUBLE_Y) {
		host_ysize = image->height() * 2;
	} else {
		host_ysize = image->height();
	}

	if (full_screen) {
		const int widget_x = this->width();
		const int widget_y = this->height();

		if ((widget_x * host_ysize) >= (widget_y * host_xsize)) {
			scaled_x = (widget_y * host_xsize) / host_ysize;
			scaled_y = widget_y;
		} else {
			scaled_x = widget_x;
			scaled_y = (widget_x * host_ysize) / host_xsize;
		}

		offset_x = (widget_x - scaled_x) / 2;
		offset_y = (widget_y - scaled_y) / 2;
	}
}

/**
 * Is the display currently doubling in either direction
 * needed in MainWindow to adjust mouse coordinates from the emulator
 */
int
MainDisplay::get_double_size()
{
	return double_size;
}

/**
 * Dump the current display to the file specified.
 *
 * @param filename Filename to save display image to
 *
 * @return bool of success or failure
 */
bool
MainDisplay::save_screenshot(QString filename)
{
	return this->image->save(filename, "png");
}

MainWindow::MainWindow(Emulator &emulator)
    : full_screen(false),
      reenable_mousehack(false),
      emulator(emulator),
      mips_timer(this),
      mips_total_instructions(0),
      mips_seconds(0),
      menu_open(false)
{
	setWindowTitle("RPCEmu v" VERSION);

	// Copy the emulators config to a thread local copy
	memcpy(&config_copy, &config,  sizeof(Config));
	pconfig_copy = &config_copy;
	model_copy = machine.model;

	display = new MainDisplay(emulator);
	display->setFixedSize(640, 480);
	setCentralWidget(display);

	// Mouse handling
	display->setMouseTracking(true);

	create_actions();
	create_menus();
	create_tool_bars();

	readSettings();

	this->setFixedSize(this->sizeHint());
	setUnifiedTitleAndToolBarOnMac(true);

	// Update the gui with the initial config setting
	// TODO what about fullscreen? (probably handled in GUI only
	if(config_copy.cpu_idle) {
		cpu_idle_action->setChecked(true);
	}
	if(config.mousehackon) {
		mouse_hack_action->setChecked(true);
	}
	if(config.mousetwobutton) {
		mouse_twobutton_action->setChecked(true);
	}
	if(config.cdromenabled) {
		// TODO we could check config.cdromtype here, but it's a bit
		// unreliable
		cdrom_empty_action->setChecked(true);
	} else {
		cdrom_disabled_action->setChecked(true);
	}

	configure_dialog = new ConfigureDialog(emulator, &config_copy, &model_copy, this);
#ifdef RPCEMU_NETWORKING
	network_dialog = new NetworkDialog(emulator, &config_copy, this);
#endif /* RPCEMU_NETWORKING */
	nat_list_dialog = new NatListDialog(emulator, this);
	about_dialog = new AboutDialog(this);

	// MIPS counting
	window_title.reserve(128);
	connect(&mips_timer, &QTimer::timeout, this, &MainWindow::mips_timer_timeout);
	mips_timer.start(1000);
	
	// App losing/gaining focus
	connect(qApp, &QGuiApplication::applicationStateChanged, this, &MainWindow::application_state_changed);
	
	// Workaround for for qt bug https://bugreports.qt.io/browse/QTBUG-67239
	// Prevents the menu code stealing the keyboard focus and preventing keys
	// like escape from working, when opening more than one menu on the menubar
	this->setFocus();
}

MainWindow::~MainWindow()
{
#ifdef RPCEMU_NETWORKING
	delete network_dialog;
	delete nat_list_dialog;
#endif /* RPCEMU_NETWORKING */
	delete configure_dialog;
	delete about_dialog;
}

/**
 * Ask the user if they'd like to reset RPCEmu
 *
 * @param parent pointer to mainwindow, used to centre dialog over mainwindow
 * @return code of which button pressed
 */
int
MainWindow::reset_question(QWidget *parent)
{
	QMessageBox msgBox(parent);

	msgBox.setWindowTitle("RPCEmu");
	msgBox.setText("This will reset RPCEmu!\n\nOkay to continue?");
	msgBox.setIcon(QMessageBox::Warning);
	msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
	msgBox.setDefaultButton(QMessageBox::Cancel);

	return msgBox.exec();
}

/**
 * Signal received about window gaining/losing focus, or minimising etc.
 *
 * @param state new application state
 */
void
MainWindow::application_state_changed(Qt::ApplicationState state)
{
	// If the application loses focus, release all the keys
	// that are pressed down. Prevents key stuck down repeats in emulator
	if(state != Qt::ApplicationActive) {
		release_held_keys();
		infocus = false;
	} else {
		infocus = true;
	}
}

/**
 * Generate a key-release message for each key recorded as held down, and then
 * clear this list of held keys.
 */
void
MainWindow::release_held_keys()
{
	// Release keys in the emulator
	for (std::list<quint32>::reverse_iterator it = held_keys.rbegin(); it != held_keys.rend(); ++it) {
		emit this->emulator.key_release_signal(*it);
	}

	// Clear the list of keys considered to be held in the host
	held_keys.clear();
    
#if defined(Q_OS_MACOS)
    emit this->emulator.modifier_keys_reset_signal();
#endif
}

/**
 * Window close button or File->exit() selected
 * 
 * @param event
 */
void
MainWindow::closeEvent(QCloseEvent *event)
{
	// Request confirmation to exit
	QMessageBox msgBox(QMessageBox::Question,
	    "RPCEmu",
	    "Are you sure you want to exit?",
	    QMessageBox::Cancel,
	    this);
	QPushButton *exit_button = msgBox.addButton("Exit", QMessageBox::ActionRole);
	msgBox.setDefaultButton(QMessageBox::Cancel);
	msgBox.setInformativeText("Any unsaved data will be lost.");
	msgBox.exec();

	if (msgBox.clickedButton() != exit_button) {
		// Prevent this close message triggering any more effects
		event->ignore();
		return;
	}

	// Disconnect the applicationStateChanged event, because our handler
	// can generate messages the machine won't be able to handle when quit
	disconnect(qApp, &QGuiApplication::applicationStateChanged, this, &MainWindow::application_state_changed);

	// Inform the emulator thread that we're quitting
	emit this->emulator.exit_signal();

	// Wait until emulator thread has exited
	this->emulator.thread()->wait();

	// Pass on the close message for the main window, this
	// will cause the program to quit
	event->accept();
}

void
MainWindow::keyPressEvent(QKeyEvent *event)
{
	// Block keyboard input (to non-GUI elements) if menu open
	if (this->menu_open) {
		return;
	}

	// Ignore unknown key events (can be generated by dead keys)
	if (event->key() == 0 || event->key() == Qt::Key_unknown) {
		return;
	}

	// Special case, handle windows menu key as being menu mouse button
	if(Qt::Key_Menu == event->key()) {
		emit this->emulator.mouse_press_signal(Qt::MidButton);
		return;
	}

	// Special case, check for Ctrl-End, our multi purpose do clever things key
	if((Qt::Key_End == event->key()) && (event->modifiers() & Qt::ControlModifier))
	{
        processMagicKeys();
    }

	// Regular case pass key press onto the emulator
	if (!event->isAutoRepeat()) {
#if defined(Q_OS_MACOS)
        native_keypress_event(event->nativeVirtualKey(), event->nativeModifiers());
#else
        native_keypress_event(event->nativeScanCode(), event->nativeModifiers());
#endif
	}
}

void
MainWindow::keyReleaseEvent(QKeyEvent *event)
{
	// Ignore unknown key events (can be generated by dead keys)
	if (event->key() == 0 || event->key() == Qt::Key_unknown) {
		return;
	}

	// Special case, handle windows menu key as being menu mouse button
	if(Qt::Key_Menu == event->key()) {
		emit this->emulator.mouse_release_signal(Qt::MidButton);
		return;
	}

	// Regular case pass key release onto the emulator
	if (!event->isAutoRepeat()) {
#if defined(Q_OS_MACOS)
        native_keyrelease_event(event->nativeVirtualKey(), event->nativeModifiers());
#else
        native_keyrelease_event(event->nativeScanCode(), event->nativeModifiers());
#endif    /* Q_OS_MACOS */
	}
}

/**
 * Called by us with native scan-code to forward key-press to the emulator
 *
 * @param scan_code Native scan code of key
 */
void
MainWindow::native_keypress_event(unsigned scan_code, unsigned modifiers)
{
#if defined(Q_OS_MACOS)
    if (!(scan_code == 0 && modifiers == 0))
    {
        // Check the key isn't already marked as held down (else ignore)
        // (to deal with potentially inconsistent host messages)
        bool found = (std::find(held_keys.begin(), held_keys.end(), scan_code) != held_keys.end());

        if (!found) {
            // Add the key to the list of held_keys, that will be released
            // when the window loses the focus
            held_keys.insert(held_keys.end(), scan_code);

            emit this->emulator.key_press_signal(scan_code);
        }
    }
#else
	// Check the key isn't already marked as held down (else ignore)
	// (to deal with potentially inconsistent host messages)
	bool found = (std::find(held_keys.begin(), held_keys.end(), scan_code) != held_keys.end());

	if (!found) {
		// Add the key to the list of held_keys, that will be released
		// when the window loses the focus
		held_keys.insert(held_keys.end(), scan_code);

		emit this->emulator.key_press_signal(scan_code);
	}
#endif
}

/**
 * Called by us with native scan-code to forward key-release to the emulator
 *
 * @param scan_code Native scan code of key
 */
void
MainWindow::native_keyrelease_event(unsigned scan_code, unsigned modifiers)
{
#if defined(Q_OS_MACOS)
    if (!(scan_code == 0 && modifiers == 0))
    {
        // Check the key is marked as held down (else ignore)
        // (to deal with potentially inconsistent host messages)
        bool found = (std::find(held_keys.begin(), held_keys.end(), scan_code) != held_keys.end());

        if (found) {
            // Remove the key from the list of held_keys, that will be released
            // when the window loses the focus
            held_keys.remove(scan_code);

            emit this->emulator.key_release_signal(scan_code);
        }
    }
#else
	// Check the key is marked as held down (else ignore)
	// (to deal with potentially inconsistent host messages)
	bool found = (std::find(held_keys.begin(), held_keys.end(), scan_code) != held_keys.end());

	if (found) {
		// Remove the key from the list of held_keys, that will be released
		// when the window loses the focus
		held_keys.remove(scan_code);

		emit this->emulator.key_release_signal(scan_code);
	}
#endif
}

void
MainWindow::menu_screenshot()
{
	QString fileName = QFileDialog::getSaveFileName(this,
	                                                tr("Save Screenshot"),
	                                                "screenshot.png",
	                                                tr("PNG (*.png)"));

	// fileName is NULL if user hit cancel
	if (fileName != NULL) {
		bool result = this->display->save_screenshot(fileName);

		if (result == false) {
			QMessageBox msgBox(this);
			msgBox.setText("Error saving screenshot");
			msgBox.setStandardButtons(QMessageBox::Ok);
			msgBox.setDefaultButton(QMessageBox::Ok);
			msgBox.exec();
		}
	}
}

void
MainWindow::menu_reset()
{
	int ret = MainWindow::reset_question(this);

	switch (ret) {
	case QMessageBox::Ok:
		emit this->emulator.reset_signal();
		break;

	case QMessageBox::Cancel:
	default:
		break;
	}
}

void 
MainWindow::menu_loaddisc0()
{
	QString fileName = QFileDialog::getOpenFileName(this,
	    tr("Open Disc Image"),
	    "",
	    tr("All disc images (*.adf *.adl *.hfe *.img);;ADFS D/E/F Disc Image (*.adf);;ADFS L Disc Image (*.adl);;DOS Disc Image (*.img);;HFE Disc Image (*.hfe)"));

	/* fileName is NULL if user hit cancel */
	if(fileName != NULL) {
		emit this->emulator.load_disc_0_signal(fileName);
	}
}

void 
MainWindow::menu_loaddisc1()
{
	QString fileName = QFileDialog::getOpenFileName(this,
	    tr("Open Disc Image"),
	    "",
	    tr("All disc images (*.adf *.adl *.hfe *.img);;ADFS D/E/F Disc Image (*.adf);;ADFS L Disc Image (*.adl);;DOS Disc Image (*.img);;HFE Disc Image (*.hfe)"));

	/* fileName is NULL if user hit cancel */
	if(fileName != NULL) {
		emit this->emulator.load_disc_1_signal(fileName);
	}
}

void
MainWindow::menu_configure()
{
	configure_dialog->exec(); // Modal
}

#ifdef RPCEMU_NETWORKING
void
MainWindow::menu_networking()
{
	network_dialog->exec(); // Modal

	// Update the NAT Port Forwarding Rules menu item based on choice
	if (config_copy.network_type == NetworkType_NAT) {
		nat_list_action->setEnabled(true);
	} else {
		nat_list_action->setEnabled(false);
	}
}

void
MainWindow::menu_nat_list()
{
	nat_list_dialog->exec(); // Modal
}
#endif /* RPCEMU_NETWORKING */

/**
 * Handle clicking on the Settings->Fullscreen option
 */
void
MainWindow::menu_fullscreen()
{
	if (!full_screen) {
		// Change Windowed -> Full Screen

		// Make sure people know how to exit full-screen
		if (config_copy.show_fullscreen_message) {
			QCheckBox *checkBox = new QCheckBox("Do not show this message again");

			QMessageBox msg_box(QMessageBox::Information,
			    "RPCEmu - Full-screen mode",
			    "<p>This window will now be switched to <b>full-screen</b> mode.</p>"
			    "<p>To leave full-screen mode press <b>Ctrl-End</b>.</p>",
			    QMessageBox::Ok | QMessageBox::Cancel,
			    this);
			msg_box.setDefaultButton(QMessageBox::Ok);
			msg_box.setCheckBox(checkBox);

			int ret = msg_box.exec();

			// If they didn't click OK, revert the tick on the menu item and return
			if (ret != QMessageBox::Ok) {
				// Keep tick of menu item in sync
				fullscreen_action->setChecked(false);
				return;
			}

			// If they checked the box don't show this message again
			if (msg_box.checkBox()->isChecked()) {
				emit this->emulator.show_fullscreen_message_off();
				config_copy.show_fullscreen_message = 0;
			}
		}

		display->set_full_screen(true);

		this->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
		display->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
		menuBar()->setVisible(false);
		this->showFullScreen();

		full_screen = true;
		
		// If in mousehack mode, change to a temporary mouse capture style
		// during full screen
		if(config_copy.mousehackon) {
			emit this->emulator.mouse_hack_signal();
			reenable_mousehack = true; 
		}
		
		// If in mouse capture mode and not captured, the cursor will be visible, hide it
		this->display->setCursor(Qt::BlankCursor);
	}

	// Keep tick of menu item in sync
	fullscreen_action->setChecked(false);
}

void
MainWindow::menu_cpu_idle()
{
	int ret = MainWindow::reset_question(this);

	switch (ret) {
	case QMessageBox::Ok:
		emit this->emulator.cpu_idle_signal();
		config_copy.cpu_idle ^= 1;
		break;

	case QMessageBox::Cancel:
		// If cancelled reset the tick box on the menu back to current emu state
		cpu_idle_action->setChecked(config_copy.cpu_idle);
		break;
	default:
		break;
	}

}


void
MainWindow::menu_cdrom_disabled()
{
	if (config_copy.cdromenabled) {
		int ret = MainWindow::reset_question(this);

		switch (ret) {
		case QMessageBox::Ok:
			break;

		case QMessageBox::Cancel:
			cdrom_disabled_action->setChecked(false);
			return;
		default:
			cdrom_disabled_action->setChecked(false);
			return;
		}
	}

	/* we now have either no need to reboot or an agreement to reboot */

	emit this->emulator.cdrom_disabled_signal();
	config_copy.cdromenabled = 0;

	cdrom_menu_selection_update(cdrom_disabled_action);
}

void
MainWindow::menu_cdrom_empty()
{
	if (!config_copy.cdromenabled) {
		int ret = MainWindow::reset_question(this);

		switch (ret) {
		case QMessageBox::Ok:
			break;

		case QMessageBox::Cancel:
			cdrom_empty_action->setChecked(false);
			return;
		default:
			cdrom_empty_action->setChecked(false);
			return;
		}
	}

	/* we now have either no need to reboot or an agreement to reboot */

	emit this->emulator.cdrom_empty_signal();
	config_copy.cdromenabled = 1;

	cdrom_menu_selection_update(cdrom_empty_action);
}

void
MainWindow::menu_cdrom_iso()
{
	QString fileName = QFileDialog::getOpenFileName(this,
	                                                tr("Open ISO Image"),
	                                                "",
	                                                tr("ISO CD-ROM Image (*.iso);;All Files (*.*)"));

	/* fileName is NULL if user hit cancel */
	if(fileName != NULL) {
		if (!config_copy.cdromenabled) {
			int ret = MainWindow::reset_question(this);

			switch (ret) {
			case QMessageBox::Ok:
				break;

			case QMessageBox::Cancel:
				cdrom_iso_action->setChecked(false);
				return;
			default:
				cdrom_iso_action->setChecked(false);
				return;
			}
		}

		/* we now have either no need to reboot or an agreement to reboot */

		emit this->emulator.cdrom_load_iso_signal(fileName);
		config_copy.cdromenabled = 1;

		cdrom_menu_selection_update(cdrom_iso_action);
		return;
	}

	cdrom_iso_action->setChecked(false);
}

void
MainWindow::menu_cdrom_ioctl()
{
#if defined(Q_OS_LINUX)
	if (!config_copy.cdromenabled) {
		int ret = MainWindow::reset_question(this);

		switch (ret) {
		case QMessageBox::Ok:
			break;

		case QMessageBox::Cancel:
			cdrom_ioctl_action->setChecked(false);
			return;
		default:
			cdrom_ioctl_action->setChecked(false);
			return;
		}
	}

	/* we now have either no need to reboot or an agreement to reboot */

	emit this->emulator.cdrom_ioctl_signal();
	config_copy.cdromenabled = 1;

	cdrom_menu_selection_update(cdrom_ioctl_action);
#endif /* linux */
}

void
MainWindow::menu_cdrom_win_ioctl()
{
#if defined(Q_OS_WIN32)
	QAction* action = qobject_cast<QAction *>(QObject::sender());
	if(!action) {
		fatal("menu_cdrom_win_ioctl no action\n");
	}
	char drive_letter = action->data().toChar().toLatin1();

	if (!config_copy.cdromenabled) {
		int ret = MainWindow::reset_question(this);

		switch (ret) {
		case QMessageBox::Ok:
			break;

		case QMessageBox::Cancel:
			action->setChecked(false);
			return;
		default:
			action->setChecked(false);
			return;
		}
	}

	/* we now have either no need to reboot or an agreement to reboot */

	emit this->emulator.cdrom_win_ioctl_signal(drive_letter);
	config_copy.cdromenabled = 1;

	cdrom_menu_selection_update(action);
#endif /* win32 */
}
void
MainWindow::menu_mouse_hack()
{
	emit this->emulator.mouse_hack_signal();
	config_copy.mousehackon ^= 1;

	// If we were previously in mouse capture mode (somehow having
	// escaped the mouse capturing), decapture the mouse
	if(config_copy.mousehackon) {
		mouse_captured = 0;

		// Hide pointer in mouse hack mode
		this->display->setCursor(Qt::BlankCursor);
	} else {
		// Show pointer in mouse capture mode when it's not been captured
		this->display->setCursor(Qt::ArrowCursor);
	}
}

void
MainWindow::menu_mouse_twobutton()
{
	emit this->emulator.mouse_twobutton_signal();
	config_copy.mousetwobutton ^= 1;
}


void
MainWindow::menu_online_manual()
{
	QDesktopServices::openUrl(QUrl(URL_MANUAL));
}

void
MainWindow::menu_visit_website()
{
	QDesktopServices::openUrl(QUrl(URL_WEBSITE));
}

void
MainWindow::menu_about()
{
	about_dialog->show(); // Modeless
}

/**
 * A menu is being shown.
 * Release held keys, and update the 'menu_open' variable.
 */
void
MainWindow::menu_aboutToShow()
{
	release_held_keys();
	this->menu_open = true;
}

/**
 * A menu is being hidden.
 * Update the 'menu_open' variable.
 */
void
MainWindow::menu_aboutToHide()
{
	this->menu_open = false;
}

void
MainWindow::create_actions()
{
	// Actions on File menu
	screenshot_action = new QAction(tr("Take Screenshot..."), this);
	connect(screenshot_action, &QAction::triggered, this, &MainWindow::menu_screenshot);
	reset_action = new QAction(tr("Reset"), this);
	connect(reset_action, &QAction::triggered, this, &MainWindow::menu_reset);
	exit_action = new QAction(tr("Exit"), this);
	exit_action->setStatusTip(tr("Exit the application"));
	connect(exit_action, &QAction::triggered, this, &QMainWindow::close);

	// Actions on Disc->Floppy menu
	loaddisc0_action = new QAction(tr("Load Drive :0..."), this);
	connect(loaddisc0_action, &QAction::triggered, this, &MainWindow::menu_loaddisc0);
	loaddisc1_action = new QAction(tr("Load Drive :1..."), this);
	connect(loaddisc1_action, &QAction::triggered, this, &MainWindow::menu_loaddisc1);

	// Actions on the Disc->CD-ROM menu
	cdrom_disabled_action = new QAction(tr("Disabled"), this);
	cdrom_disabled_action->setCheckable(true);
	connect(cdrom_disabled_action, &QAction::triggered, this, &MainWindow::menu_cdrom_disabled);

	cdrom_empty_action = new QAction(tr("Empty"), this);
	cdrom_empty_action->setCheckable(true);
	connect(cdrom_empty_action, &QAction::triggered, this, &MainWindow::menu_cdrom_empty);

	cdrom_iso_action = new QAction(tr("Iso Image..."), this);
	cdrom_iso_action->setCheckable(true);
	connect(cdrom_iso_action, &QAction::triggered, this, &MainWindow::menu_cdrom_iso);

#if defined(Q_OS_LINUX)
	cdrom_ioctl_action = new QAction(tr("Host CD/DVD Drive"), this);
	cdrom_ioctl_action->setCheckable(true);
	connect(cdrom_ioctl_action, &QAction::triggered, this, &MainWindow::menu_cdrom_ioctl);
#endif /* linux */
#if defined(Q_OS_WIN32)
	// Dynamically add windows cdrom drives to the settings->cdrom menu
	char s[32];
	// Loop through each Windows drive letter and test to see if it's a CDROM
	for (char c = 'A'; c <= 'Z'; c++) {
		sprintf(s, "%c:\\", c);
		if (GetDriveTypeA(s) == DRIVE_CDROM) {
			QAction *new_action = new QAction(s, this);
			new_action->setCheckable(true);
			new_action->setData(c);

			connect(new_action, &QAction::triggered, this, &MainWindow::menu_cdrom_win_ioctl);

			cdrom_win_ioctl_actions.insert(cdrom_win_ioctl_actions.end(), new_action);
		}
	}
#endif

	// Actions on Settings menu
	configure_action = new QAction(tr("Configure..."), this);
	connect(configure_action, &QAction::triggered, this, &MainWindow::menu_configure);
#ifdef RPCEMU_NETWORKING
	networking_action = new QAction(tr("Networking..."), this);
	connect(networking_action, &QAction::triggered, this, &MainWindow::menu_networking);
	nat_list_action = new QAction(tr("NAT Port Forwarding Rules..."), this);
	connect(nat_list_action, &QAction::triggered, this, &MainWindow::menu_nat_list);
#endif /* RPCEMU_NETWORKING */
	fullscreen_action = new QAction(tr("Full-screen Mode"), this);
	fullscreen_action->setCheckable(true);
	connect(fullscreen_action, &QAction::triggered, this, &MainWindow::menu_fullscreen);
	cpu_idle_action = new QAction(tr("Reduce CPU Usage"), this);
	cpu_idle_action->setCheckable(true);
	connect(cpu_idle_action, &QAction::triggered, this, &MainWindow::menu_cpu_idle);

	// Actions on the Settings->Mouse menu
	mouse_hack_action = new QAction(tr("Follow Host Mouse"), this);
	mouse_hack_action->setCheckable(true);
	connect(mouse_hack_action, &QAction::triggered, this, &MainWindow::menu_mouse_hack);

	mouse_twobutton_action = new QAction(tr("Two-button Mouse Mode"), this);
	mouse_twobutton_action->setCheckable(true);
	connect(mouse_twobutton_action, &QAction::triggered, this, &MainWindow::menu_mouse_twobutton);

	// Actions on About menu
	online_manual_action = new QAction(tr("Online Manual..."), this);
	connect(online_manual_action, &QAction::triggered, this, &MainWindow::menu_online_manual);
	visit_website_action = new QAction(tr("Visit Website..."), this);
	connect(visit_website_action, &QAction::triggered, this, &MainWindow::menu_visit_website);

	about_action = new QAction(tr("About RPCEmu..."), this);
	about_action->setStatusTip(tr("Show the application's About box"));
	connect(about_action, &QAction::triggered, this, &MainWindow::menu_about);

	connect(this, &MainWindow::main_display_signal, this, &MainWindow::main_display_update, Qt::BlockingQueuedConnection);
//	connect(this, &MainWindow::main_display_signal, this, &MainWindow::main_display_update);
	connect(this, &MainWindow::move_host_mouse_signal, this, &MainWindow::move_host_mouse);
	connect(this, &MainWindow::send_nat_rule_to_gui_signal, this, &MainWindow::send_nat_rule_to_gui);

	// Connections for displaying error messages in the GUI
	connect(this, &MainWindow::error_signal, this, &MainWindow::error);
	connect(this, &MainWindow::fatal_signal, this, &MainWindow::fatal);
}

void
MainWindow::create_menus()
{
	// File menu
	file_menu = menuBar()->addMenu(tr("File"));
	file_menu->addAction(screenshot_action);
	file_menu->addSeparator();
	file_menu->addAction(reset_action);
	file_menu->addSeparator();
	file_menu->addAction(exit_action);

	// Disc menu
	disc_menu = menuBar()->addMenu(tr("Disc"));
	floppy_menu = disc_menu->addMenu(tr("Floppy"));
	cdrom_menu = disc_menu->addMenu(tr("CD-ROM"));

	// Disc->Floppy menu
	floppy_menu->addAction(loaddisc0_action);
	floppy_menu->addAction(loaddisc1_action);

	// Disc->CD-ROM menu
	cdrom_menu->addAction(cdrom_disabled_action);
	cdrom_menu->addAction(cdrom_empty_action);
	cdrom_menu->addAction(cdrom_iso_action);
#if defined(Q_OS_LINUX)
	cdrom_menu->addAction(cdrom_ioctl_action);
#endif /* linux */
#if defined(Q_OS_WIN32)
	for (unsigned i = 0; i < cdrom_win_ioctl_actions.size(); i++) {
		cdrom_menu->addAction(cdrom_win_ioctl_actions[i]);
	}
#endif

	// Settings menu
	settings_menu = menuBar()->addMenu(tr("Settings"));
	settings_menu->addAction(configure_action);
#ifdef RPCEMU_NETWORKING
	settings_menu->addAction(networking_action);
	settings_menu->addAction(nat_list_action);
	if (this->config_copy.network_type != NetworkType_NAT) {
		nat_list_action->setEnabled(false);
	}
#endif /* RPCEMU_NETWORKING */
	settings_menu->addSeparator();
	settings_menu->addAction(fullscreen_action);
	settings_menu->addSeparator();
	settings_menu->addAction(cpu_idle_action);
	settings_menu->addSeparator();
	mouse_menu = settings_menu->addMenu(tr("Mouse"));

	// Mouse submenu
	mouse_menu->addAction(mouse_hack_action);
	mouse_menu->addSeparator();
	mouse_menu->addAction(mouse_twobutton_action);

	menuBar()->addSeparator();

	// Help menu
	help_menu = menuBar()->addMenu(tr("Help"));
	help_menu->addAction(online_manual_action);
	help_menu->addAction(visit_website_action);
	help_menu->addSeparator();
	help_menu->addAction(about_action);

	// Add handlers to track menu show/hide events
	add_menu_show_hide_handlers();
}

void
MainWindow::create_tool_bars()
{
}

/**
 * Add handlers to track menu show/hide events.
 *
 * These are used to track open menus, and suppress key events.
 */
void
MainWindow::add_menu_show_hide_handlers()
{
	// Find the QMenu items that are children of menuBar()
	//
	// Filter for direct children only, otherwise when sub-menus are
	// hidden, we'll update the variable menu_open to false even though a
	// top-level menu remains open
	QList<QMenu *> menus = menuBar()->findChildren<QMenu *>(QString(), Qt::FindDirectChildrenOnly);

	QList<QMenu *>::const_iterator i;

	for (i = menus.constBegin(); i != menus.constEnd(); i++) {
		const QMenu *menu = *i;

		connect(menu, &QMenu::aboutToShow, this, &MainWindow::menu_aboutToShow);
		connect(menu, &QMenu::aboutToHide, this, &MainWindow::menu_aboutToHide);
	}
}

void
MainWindow::readSettings()
{
	QSettings settings("QtProject", "Application Example");
	QPoint pos = settings.value("pos", QPoint(200, 200)).toPoint();
	QSize size = settings.value("size", QSize(400, 400)).toSize();
	resize(size);
	move(pos);
}

void
MainWindow::writeSettings()
{
	QSettings settings("QtProject", "Application Example");
	settings.setValue("pos", pos());
	settings.setValue("size", size());
}

void
MainWindow::main_display_update(VideoUpdate video_update)
{
	if (video_update.host_xsize != display->width() ||
	    video_update.host_ysize != display->height())
	{
		if (!full_screen) {
			// Resize Widget containing image
			display->setFixedSize(video_update.host_xsize, video_update.host_ysize);

			// Resize Window
			this->setFixedSize(this->sizeHint());
		}
	}

	// Copy image data
	display->update_image(video_update.image, video_update.yl, video_update.yh,
	    video_update.double_size);
}

/**
 * Received a request from the emulator thread to position the host mouse pointer
 * Used in sections of Follows host mouse/mousehack code
 *
 * @param mouse_update message struct containing desired mouse coordinates
 */
void
MainWindow::move_host_mouse(MouseMoveUpdate mouse_update)
{
	QPoint pos;
	int double_size = display->get_double_size();

	// Do not move the mouse if rpcemu window doesn't have the focus
	if(false == infocus) {
		return;
	}

	// Don't move the mouse if the display widget is not directly under the mouse
	// This handles the mouse being moved away from rpcemu or the mouse over one of our
	// menus or configuration dialog boxes.
	if(false == display->underMouse()) {
		return;
	}

	pos.setX(mouse_update.x);
	pos.setY(mouse_update.y);

	// The mouse coordinates from the backend do not know about frontend double sizing
	// Add that on if necessary
	if(double_size == VIDC_DOUBLE_X
	   || double_size == VIDC_DOUBLE_BOTH)
	{
		pos.setX(mouse_update.x * 2);
	}

	if(double_size == VIDC_DOUBLE_Y
	   || double_size == VIDC_DOUBLE_BOTH)
	{
		pos.setY(mouse_update.y * 2);
	}

	// Due to the front end and backend vidc doublesize values being
	// potentially out of sync on mode change, as a temporary HACK, cap the
	// mouse pos to under the main display widget
	if(pos.x() > (display->width() - 1)) {
		pos.setX(display->width() - 1);
	}
	if(pos.y() > (display->height() - 1)) {
		pos.setY(display->height() - 1);
	}

	QCursor::setPos(display->mapToGlobal(pos));
}

void
MainWindow::send_nat_rule_to_gui(PortForwardRule rule)
{
	nat_list_dialog->add_nat_rule(rule);
}

/**
 * Called each time the mips_timer times out.
 *
 * The shared instruction counter is read and the title updated with the
 * MIPS and Average.
 */
void
MainWindow::mips_timer_timeout()
{
	const char *capture_text = NULL;

	assert(pconfig_copy);

	// Read (and zero atomically) the instruction count from the emulator core
	// 'instruction_count' is in multiples of 65536
	const unsigned count = (unsigned) instruction_count.fetchAndStoreRelease(0);

	// Calculate MIPS
	const double mips = (double) count * 65536.0 / 1000000.0;

	// Update variables used for average
	mips_total_instructions += (uint64_t) count << 16;
	mips_seconds++;

	// Calculate Average
	const double average = (double) mips_total_instructions / ((double) mips_seconds * 1000000.0);

	if(!pconfig_copy->mousehackon) {
		if(mouse_captured) {
#if defined(Q_OS_MACOS)
            capture_text = " Press CTRL-COMMAND to release mouse";
#else
			capture_text = " Press CTRL-END to release mouse";
#endif
		} else {
			capture_text = " Click to capture mouse";
		}
	} else {
		capture_text = "";
	}

#if 1
	// Update window title
	window_title = QString("RPCEmu - MIPS: %1 AVG: %2%3")
	    .arg(mips, 0, 'f', 1)
	    .arg(average, 0, 'f', 1)
	    .arg(capture_text);

#else
	// Read  (and zero atomically) the IOMD timer count from the emulator core
	const int icount = iomd_timer_count.fetchAndStoreRelease(0);

	// Read  (and zero atomically) the Video timer count from the emulator core
	const int vcount = video_timer_count.fetchAndStoreRelease(0);

	// Update window title (including timer information, for debug purposes)
	window_title = QString("RPCEmu - MIPS: %1 AVG: %2, ITimer: %3, VTimer: %4%5")
	    .arg(mips, 0, 'f', 1)
	    .arg(average, 0, 'f', 1)
	    .arg(icount)
	    .arg(vcount)
	    .arg(capture_text);
#endif

	setWindowTitle(window_title);
}

/**
 * Present a model dialog to the user about an error that has occured.
 * Wait for them to dismiss it
 * 
 * @param error error string
 */
void
MainWindow::error(QString error)
{
	QMessageBox::warning(this, "RPCEmu Error", error);
}

/**
 * Present a model dialog to the user about a fatal error that has occured.
 * Wait for them to dismiss it and exit the program
 * 
 * @param error error string
 */
void
MainWindow::fatal(QString error)
{
	QMessageBox::critical(this, "RPCEmu Fatal Error", error);

	exit(EXIT_FAILURE);
}

/**
 * Make the selected CDROM menu item the only one selected 
 * 
 * @param cdrom_action CDROM item to make the selection
 */ 
void
MainWindow::cdrom_menu_selection_update(const QAction *cdrom_action)
{
	// Turn all tick boxes off 
	cdrom_disabled_action->setChecked(false);
	cdrom_empty_action->setChecked(false);
	cdrom_iso_action->setChecked(false);
#if defined(Q_OS_LINUX)
	cdrom_ioctl_action->setChecked(false);
#endif
#if defined(Q_OS_WIN32)
	for (unsigned i = 0; i < cdrom_win_ioctl_actions.size(); i++) {
		cdrom_win_ioctl_actions[i]->setChecked(false);
	}
#endif

	// Turn correct one on
	if(cdrom_action == cdrom_disabled_action) {
		cdrom_disabled_action->setChecked(true);
	} else if(cdrom_action == cdrom_empty_action) {
		cdrom_empty_action->setChecked(true);
	} else if(cdrom_action == cdrom_iso_action) {
		cdrom_iso_action->setChecked(true);
#if defined(Q_OS_LINUX)
	} else if(cdrom_action == cdrom_ioctl_action) {
		cdrom_ioctl_action->setChecked(true);
#endif
#if defined(Q_OS_WIN32)
	} else {
		for (unsigned i = 0; i < cdrom_win_ioctl_actions.size(); i++) {
			if(cdrom_action == cdrom_win_ioctl_actions[i]) {
				cdrom_win_ioctl_actions[i]->setChecked(true);
			}
		}
#endif
	}
}
    
void
MainWindow::processMagicKeys()
{
    if(full_screen) {
        // Change Full Screen -> Windowed

        display->set_full_screen(false);

        int host_xsize, host_ysize;
        display->get_host_size(host_xsize, host_ysize);
        display->setFixedSize(host_xsize, host_ysize);

        menuBar()->setVisible(true);
        this->showNormal();
        this->setFixedSize(this->sizeHint());

        full_screen = false;

        // Request redraw of display
        display->update();

        // If we were in mousehack mode before entering fullscreen
        // return to it now
        if(reenable_mousehack) {
            emit this->emulator.mouse_hack_signal();
        }
        reenable_mousehack = false;

        // If we were in mouse capture mode before entering fullscreen
        // and we hadn't captured the mouse, display the host cursor now
        if(!config_copy.mousehackon && !mouse_captured) {
            this->display->setCursor(Qt::ArrowCursor);
        }

        return;
    } else if(!pconfig_copy->mousehackon && mouse_captured) {
        // Turn off mouse capture
        mouse_captured = 0;

        // show pointer in mouse capture mode when it's not been captured
        this->display->setCursor(Qt::ArrowCursor);

        return;
    }
}

#if defined(Q_OS_WIN32)
/**
 * windows pre event handler used by us to modify some default behaviour
 *
 * Disable the use of the virtual menu key (alt) that otherwise goes off
 * every time someone presses alt in the emulated OS
 *
 * @param eventType unused
 * @param message window event MSG struct
 * @param result unused
 * @return bool of whether we've handled the event (true) or windows/qt should deal with it (false) 
 */
bool
MainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
	Q_UNUSED(result);
	Q_UNUSED(eventType);

	// Block keyboard input (to non-GUI elements) if menu open
	if (this->menu_open) {
		return false;
	}

	MSG *msg = static_cast<MSG*>(message);

	// Handle 'alt' key presses that would select the menu
	// Handle 'shift-f10' key presses that would select the context-menu
	// Fake 'normal' key press and release and then tell windows/qt to
	// not handle it
	if ((msg->message == WM_SYSKEYDOWN || msg->message == WM_SYSKEYUP)
	    && (msg->wParam == VK_MENU || msg->wParam == VK_F10))
	{
		unsigned scan_code = (unsigned) (msg->lParam >> 16) & 0x1ff;

		if (msg->message == WM_SYSKEYDOWN) {
			native_keypress_event(scan_code);
		} else {
			native_keyrelease_event(scan_code);
		}
		return true;
	}

	// Convert dead-key presses into normal key-presses:
	// If key pressed, delete the following message if it's WM_DEADCHAR.
	// This effectively converts it into a normal key-press and stops Qt bypassing QKeyEvent handling
	// Based on code @ https://stackoverflow.com/questions/3872085/dead-keys-dont-arrive-at-keypressedevent
	MSG peeked_msg;
	switch (msg->message) {
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		PeekMessage(&peeked_msg, msg->hwnd, WM_DEADCHAR, WM_DEADCHAR, PM_REMOVE);
		break;
	}

	// Anything else should be handled by the regular qt and windows handlers
	return false;
}
#endif // Q_OS_WIN32

#if defined(Q_OS_MACOS)
/**
 * On OS X, handle additional events for modifier keys.  The normal key press/release
 * events do not differentiate between left and right.
 *
 * @param eventType unused
 * @param message window event NSEvent data
 * @param result unused
 * @return bool of whether we've handled the event (true) or OS X/QT should deal with it (false)
 */
bool
MainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
    Q_UNUSED(eventType);
    Q_UNUSED(result);

    NativeEvent *event = handle_native_event(message);
    if (!event->processed)
    {
        free(event);
        return false;
    }

    if (event->eventType == nativeEventTypeModifiersChanged)
    {
        // Modifier key state has changed.
        emit this->emulator.modifier_keys_changed_signal(event->modifierMask);

        if (keyboard_check_special_keys())
        {
            // Magic key combination to release mouse capture.
            processMagicKeys();
        }

        free(event);
    }

    return true;
}

#endif /* Q_OS_MACOS */
    
