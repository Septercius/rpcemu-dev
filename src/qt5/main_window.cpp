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
#include <iostream>

#include <QDesktopServices>
#include <QFileDialog>
#include <QMenuBar>
#include <QMessageBox>

#include "keyboard.h"

#include "main_window.h"
#include "rpc-qt5.h"
#include "config.h"

#define URL_MANUAL	"http://www.marutan.net/rpcemu/manual/"
#define URL_WEBSITE	"http://www.marutan.net/rpcemu/"

MainLabel::MainLabel(Emulator &emulator)
    : emulator(emulator)
{
	// Hide pointer
	this->setCursor(Qt::BlankCursor);
}

void
MainLabel::mouseMoveEvent(QMouseEvent *event)
{
	emit this->emulator.mouse_move_signal(event->x(), event->y());
}

void
MainLabel::mousePressEvent(QMouseEvent *event)
{
	if (event->button() & 7) {
		emit this->emulator.mouse_press_signal(event->button() & 7);
	}
}

void
MainLabel::mouseReleaseEvent(QMouseEvent *event)
{
	if (event->button() & 7) {
		emit this->emulator.mouse_release_signal(event->button() & 7);
	}
}


MainWindow::MainWindow(Emulator &emulator)
    : full_screen(false),
      emulator(emulator),
      mips_timer(this),
      mips_total_instructions(0),
      mips_seconds(0)
{
	setWindowTitle("RPCEmu v" VERSION);
	
	image = new QImage(640, 480, QImage::Format_RGB32);

	label = new MainLabel(emulator);
	label->setMinimumSize(640, 480);
	label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	label->setPixmap(QPixmap::fromImage(*image));
	setCentralWidget(label);

	// Mouse handling
	label->setMouseTracking(true);

	create_actions();
	create_menus();
	create_tool_bars();

	readSettings();

	this->setFixedSize(this->sizeHint());
	setUnifiedTitleAndToolBarOnMac(true);

	// Copy the emulators config to a thread local copy
	memcpy(&config_copy, &config,  sizeof(Config));
	model_copy = machine.model;

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
	network_dialog = new NetworkDialog(emulator, &config_copy, &model_copy, this);
	about_dialog = new AboutDialog(this);

	// MIPS counting
	window_title.reserve(128);
	connect(&mips_timer, &QTimer::timeout, this, &MainWindow::mips_timer_timeout);
	mips_timer.start(1000);
}

MainWindow::~MainWindow()
{
	delete network_dialog;
	delete configure_dialog;
	delete about_dialog;
}

/**
 * Window close button or File->exit() selected
 * 
 * @param event
 */
void
MainWindow::closeEvent(QCloseEvent *event)
{
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
	if (!event->isAutoRepeat()) {
		emit this->emulator.key_press_signal(event->nativeScanCode());
	}
}

void
MainWindow::keyReleaseEvent(QKeyEvent *event)
{
	if (!event->isAutoRepeat()) {
		emit this->emulator.key_release_signal(event->nativeScanCode());
	}
}

void
MainWindow::menu_reset()
{
	emit this->emulator.reset_signal();
}

void 
MainWindow::menu_loaddisc0()
{
	QString fileName = QFileDialog::getOpenFileName(this,
	                                                tr("Open Disc Image"),
	                                                "",
	                                                tr("ADFS Disc Image (*.adf);;All Files (*.*)"));

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
	                                                tr("ADFS Disc Image (*.adf);;All Files (*.*)"));

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

void
MainWindow::menu_networking()
{
	network_dialog->exec(); // Modal
}

void
MainWindow::menu_fullscreen()
{
	std::cout << "fullscreen clicked" << std::endl;
}

void
MainWindow::menu_cpu_idle()
{
	QMessageBox msgBox;
	msgBox.setText("This will reset RPCEmu!\nOkay to continue?");
	msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
	msgBox.setDefaultButton(QMessageBox::Cancel);
	int ret = msgBox.exec();

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
		QMessageBox msgBox;
		msgBox.setText("This will reset RPCEmu!\nOkay to continue?");
		msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
		msgBox.setDefaultButton(QMessageBox::Cancel);
		int ret = msgBox.exec();

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
		QMessageBox msgBox;
		msgBox.setText("This will reset RPCEmu!\nOkay to continue?");
		msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
		msgBox.setDefaultButton(QMessageBox::Cancel);
		int ret = msgBox.exec();

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
			QMessageBox msgBox;
			msgBox.setText("This will reset RPCEmu!\nOkay to continue?");
			msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
			msgBox.setDefaultButton(QMessageBox::Cancel);
			int ret = msgBox.exec();

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
		QMessageBox msgBox;
		msgBox.setText("This will reset RPCEmu!\nOkay to continue?");
		msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
		msgBox.setDefaultButton(QMessageBox::Cancel);
		int ret = msgBox.exec();

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
		QMessageBox msgBox;
		msgBox.setText("This will reset RPCEmu!\nOkay to continue?");
		msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
		msgBox.setDefaultButton(QMessageBox::Cancel);
		int ret = msgBox.exec();

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
}

void
MainWindow::menu_mouse_capture()
{
	emit this->emulator.mouse_capture_signal();
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
	about_dialog->exec(); // Modal
}

void
MainWindow::create_actions()
{
	// Actions on File menu
	reset_action = new QAction(tr("&Reset"), this);
	connect(reset_action, SIGNAL(triggered()), this, SLOT(menu_reset()));

	exit_action = new QAction(tr("E&xit"), this);
	exit_action->setShortcuts(QKeySequence::Quit);
	exit_action->setStatusTip(tr("Exit the application"));
	connect(exit_action, SIGNAL(triggered()), this, SLOT(close()));

	// Actions on Disc menu
	loaddisc0_action = new QAction(tr("Load Disc :0"), this);
	connect(loaddisc0_action, SIGNAL(triggered()), this, SLOT(menu_loaddisc0()));
	loaddisc1_action = new QAction(tr("Load Disc :1"), this);
	connect(loaddisc1_action, SIGNAL(triggered()), this, SLOT(menu_loaddisc1()));

	// Actions on Settings menu
	configure_action = new QAction(tr("&Configure..."), this);
	connect(configure_action, SIGNAL(triggered()), this, SLOT(menu_configure()));
#ifdef RPCEMU_NETWORKING
	networking_action = new QAction(tr("&Networking..."), this);
	connect(networking_action, SIGNAL(triggered()), this, SLOT(menu_networking()));
#endif
	fullscreen_action = new QAction(tr("&Fullscreen mode"), this);
	fullscreen_action->setCheckable(true);
	connect(fullscreen_action, SIGNAL(triggered()), this, SLOT(menu_fullscreen()));
	cpu_idle_action = new QAction(tr("&Reduce CPU usage"), this);
	cpu_idle_action->setCheckable(true);
	connect(cpu_idle_action, SIGNAL(triggered()), this, SLOT(menu_cpu_idle()));

	// Actions on the Settings->CD ROM Menu
	cdrom_disabled_action = new QAction(tr("&Disabled"), this);
	cdrom_disabled_action->setCheckable(true);
	connect(cdrom_disabled_action, SIGNAL(triggered()), this, SLOT(menu_cdrom_disabled()));

	cdrom_empty_action = new QAction(tr("&Empty"), this);
	cdrom_empty_action->setCheckable(true);
	connect(cdrom_empty_action, SIGNAL(triggered()), this, SLOT(menu_cdrom_empty()));

	cdrom_iso_action = new QAction(tr("&Iso Image..."), this);
	cdrom_iso_action->setCheckable(true);
	connect(cdrom_iso_action, SIGNAL(triggered()), this, SLOT(menu_cdrom_iso()));

#if defined(Q_OS_LINUX)
	cdrom_ioctl_action = new QAction(tr("&Host CD/DVD Drive"), this);
	cdrom_ioctl_action->setCheckable(true);
	connect(cdrom_ioctl_action, SIGNAL(triggered()), this, SLOT(menu_cdrom_ioctl()));
#endif /* linux */
#if defined(Q_OS_WIN32)
	// Dynamically add windows cdrom drives to the settings->cdrom menu
	char s[32];
	// Loop through each Windows drive letter and test to see if it's a CDROM
	for (char c = 'A'; c <= 'Z'; c++) {
		sprintf(s, "%c:\\", c);
		if (GetDriveTypeA(s) == DRIVE_CDROM) {
			sprintf(s, "Host CD/DVD Drive (%c:)", c);
			rpclog(s);
			
			QAction *new_action = new QAction(s, this);
			new_action->setCheckable(true);
			new_action->setData(c);

			connect(new_action, SIGNAL(triggered()), this, SLOT(menu_cdrom_win_ioctl()));

			cdrom_win_ioctl_actions.insert(cdrom_win_ioctl_actions.end(), new_action);
		}
	}
#endif

	// Aactions on the Settings->Mouse menu
	mouse_hack_action = new QAction(tr("&Follow host mouse"), this);
	mouse_hack_action->setCheckable(true);
	connect(mouse_hack_action, SIGNAL(triggered()), this, SLOT(menu_mouse_hack()));

	mouse_capture_action = new QAction(tr("&Capture"), this);
	mouse_capture_action->setCheckable(true);
	connect(mouse_capture_action, SIGNAL(triggered()), this, SLOT(menu_mouse_capture()));

	mouse_twobutton_action = new QAction(tr("&Two-button Mouse Mode"), this);
	mouse_twobutton_action->setCheckable(true);
	connect(mouse_twobutton_action, SIGNAL(triggered()), this, SLOT(menu_mouse_twobutton()));

	// Actions on About menu
	online_manual_action = new QAction(tr("Online &Manual..."), this);
	connect(online_manual_action, SIGNAL(triggered()), this, SLOT(menu_online_manual()));
	visit_website_action = new QAction(tr("Visit &Website..."), this);
	connect(visit_website_action, SIGNAL(triggered()), this, SLOT(menu_visit_website()));

	about_action = new QAction(tr("&About RPCEmu..."), this);
	about_action->setStatusTip(tr("Show the application's About box"));
	connect(about_action, SIGNAL(triggered()), this, SLOT(menu_about()));

	connect(this, SIGNAL(main_display_signal(QPixmap)), this, SLOT(main_display_update(QPixmap)), Qt::BlockingQueuedConnection);
//	connect(this, SIGNAL(main_display_signal(QPixmap)), this, SLOT(main_display_update(QPixmap)));

	// Connections for displaying error messages in the GUI
	connect(this, &MainWindow::error_signal, this, &MainWindow::error);
	connect(this, &MainWindow::fatal_signal, this, &MainWindow::fatal);
}

void
MainWindow::create_menus()
{
	// File menu
	file_menu = menuBar()->addMenu(tr("&File"));
	file_menu->addAction(reset_action);
	file_menu->addSeparator();
	file_menu->addAction(exit_action);

	// Disc menu
	disc_menu = menuBar()->addMenu(tr("&Disc"));
	disc_menu->addAction(loaddisc0_action);
	disc_menu->addAction(loaddisc1_action);

	// Settings menu (and submenus)
	settings_menu = menuBar()->addMenu(tr("&Settings"));
	settings_menu->addAction(configure_action);
	settings_menu->addAction(networking_action);
	settings_menu->addSeparator();
	settings_menu->addAction(fullscreen_action);
	settings_menu->addAction(cpu_idle_action);
	settings_menu->addSeparator();
	cdrom_menu = settings_menu->addMenu(tr("&CD-ROM"));
	settings_menu->addSeparator();
	mouse_menu = settings_menu->addMenu(tr("&Mouse"));

	// CD-ROM submenu
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

	// Mouse submenu
	mouse_menu->addAction(mouse_hack_action);
	mouse_menu->addAction(mouse_capture_action);
	mouse_menu->addAction(mouse_twobutton_action);


	menuBar()->addSeparator();

	// Help menu
	help_menu = menuBar()->addMenu(tr("&Help"));
	help_menu->addAction(online_manual_action);
	help_menu->addAction(visit_website_action);
	help_menu->addSeparator();
	help_menu->addAction(about_action);
}

void
MainWindow::create_tool_bars()
{
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
MainWindow::main_display_update(QPixmap pixmap)
{
	if (pixmap.size() != label->size()) {
		// Resize Label containing image
		label->setMinimumSize(pixmap.size());
		label->setMaximumSize(pixmap.size());

		// Resize Window
		this->setFixedSize(this->sizeHint());
	}

	this->label->setPixmap(pixmap);
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
	// Read (and zero atomically) the instruction count from the emulator core
	const unsigned count = (unsigned) instruction_count.fetchAndStoreRelease(0);

	// Calculate MIPS
	const double mips = (double) count / 1000000.0;

	// Update variables used for average
	mips_total_instructions += (uint64_t) count;
	mips_seconds++;

	// Calculate Average
	const double average = (double) mips_total_instructions / ((double) mips_seconds * 1000000.0);

	// Read  (and zero atomically) the IOMD timer count from the emulator core
	const int icount = iomd_timer_count.fetchAndStoreRelease(0);

	// Read  (and zero atomically) the Video timer count from the emulator core
	const int vcount = video_timer_count.fetchAndStoreRelease(0);

	// Update window title
	window_title = QString("RPCEmu - MIPS: %1 AVG: %2, ITimer: %3, VTimer: %4")
	    .arg(mips, 0, 'f', 1)
	    .arg(average, 0, 'f', 1)
	    .arg(icount)
	    .arg(vcount);
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
