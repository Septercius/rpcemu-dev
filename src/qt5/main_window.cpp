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
#include <QtWidgets>

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
//	fprintf(stderr, "press %x\n", mouse_b);
	if (event->button() & 7) {
		emit this->emulator.mouse_press_signal(event->button() & 7);
	}
}

void
MainLabel::mouseReleaseEvent(QMouseEvent *event)
{
//	fprintf(stderr, "release %x\n", mouse_b);
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
	create_status_bar();

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
	// TODO CDROM actions, were these ever set to anything?

	configure_dialog = new ConfigureDialog(&emulator, &config_copy, &model_copy, this);
	network_dialog = new NetworkDialog(this);

	// MIPS counting
	window_title.reserve(128);
	connect(&mips_timer, &QTimer::timeout, this, &MainWindow::mips_timer_timeout);
	mips_timer.start(1000);
}

MainWindow::~MainWindow()
{
	delete network_dialog;
	delete configure_dialog;
}

void
MainWindow::closeEvent(QCloseEvent *event)
{
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
	emit this->emulator.cdrom_disabled_signal();
}

void
MainWindow::menu_cdrom_empty()
{
	emit this->emulator.cdrom_empty_signal();
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
		emit this->emulator.cdrom_load_iso_signal(fileName);
	}
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
	QMessageBox::about(this, tr("About RPCEmu"),
	    tr("The <b>Application</b> example demonstrates how to "
	       "write modern GUI applications using Qt, with a menu bar, "
	       "toolbars, and a status bar."));
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
	networking_action = new QAction(tr("&Networking..."), this);
	connect(networking_action, SIGNAL(triggered()), this, SLOT(menu_networking()));
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
MainWindow::create_status_bar()
{
	statusBar()->showMessage(tr("Ready"));
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

	// Update window title
	window_title = QString("RPCEmu v" VERSION " - MIPS: %1 AVG: %2")
	    .arg(mips, 0, 'f', 1)
	    .arg(average, 0, 'f', 1);
	setWindowTitle(window_title);
}
