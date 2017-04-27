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
	// Luckily allegro and qt5 use 1, 2 and 4 for left, right, mid in the same way
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
      emulator(emulator)
{
	setWindowTitle("RPCEmu v" VERSION);
	
	image = new QImage(640, 480, QImage::Format_RGB32);

	label = new MainLabel(emulator);
	label->setMinimumSize(640, 480);
	label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	label->setPixmap(QPixmap::fromImage(*image));
	setCentralWidget(label);

	// Mouse handlind
	label->setMouseTracking(true);

	create_actions();
	create_menus();
	create_tool_bars();
	create_status_bar();

	readSettings();

	this->setFixedSize(this->sizeHint());
	setUnifiedTitleAndToolBarOnMac(true);

	configure_dialog = new ConfigureDialog(this);
	network_dialog = new NetworkDialog(this);
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
		//emit this->emulator.key_press_signal(42);
		emit this->emulator.key_press_signal(event->nativeScanCode());
		//emit this->emulator.key_press_signal(*event);
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
	delete image;
	image = new QImage(800, 600, QImage::Format_RGB32);

	label->setMinimumSize(800, 600);
	label->setPixmap(QPixmap::fromImage(*image));

	this->setFixedSize(this->sizeHint());
}

void 
MainWindow::menu_loaddisc0()
{
	const char *p;
	QByteArray ba;
	QString fileName = QFileDialog::getOpenFileName(this,
	                                                tr("Open Disc Image"),
	                                                "",
	                                                tr("ADFS Disc Image (*.adf);;All Files (*.*)"));

	if(fileName != NULL) {
		ba = fileName.toUtf8();
		p = ba.data();
		std::cout << "Load Disc :0 load '" << p << "'" << std::endl;
	} else {
		std::cout << "Load Disc :0 User hit cancel" << std::endl;
	}
}

void 
MainWindow::menu_loaddisc1()
{
	const char *p;
	QByteArray ba;
	QString fileName = QFileDialog::getOpenFileName(this,
	                                                tr("Open Disc Image"),
	                                                "",
	                                                tr("ADFS Disc Image (*.adf);;All Files (*.*)"));

	if(fileName != NULL) {
		ba = fileName.toUtf8();
		p = ba.data();
		std::cout << "Load Disc :1 load '" << p << "'" << std::endl;
	} else {
		std::cout << "Load Disc :1 User hit cancel" << std::endl;
	}
}

void
MainWindow::menu_configure()
{
	std::cout << "Configure clicked" << std::endl;
	configure_dialog->exec(); // Modal
	//configure_dialog->show(); // Non-modal
}

void
MainWindow::menu_networking()
{
	std::cout << "Networking clicked" << std::endl;
	network_dialog->exec(); // Modal
}

void
MainWindow::menu_cdrom_disabled()
{
	std::cout << "CDROM disabled clicked" << std::endl;
}

void
MainWindow::menu_cdrom_empty()
{
	std::cout << "CDROM empty clicked" << std::endl;
}

void
MainWindow::menu_cdrom_iso()
{
	const char *p;
	QByteArray ba;
	QString fileName = QFileDialog::getOpenFileName(this,
	                                                tr("Open ISO Image"),
	                                                "",
	                                                tr("ISO CD-ROM Image (*.iso);;All Files (*.*)"));

	if(fileName != NULL) {
		ba = fileName.toUtf8();
		p = ba.data();
		std::cout << "CDROM iso load '" << p << "'" << std::endl;
	} else {
		std::cout << "CDROM iso User hit cancel" << std::endl;
	}
}

void
MainWindow::menu_mouse_hack()
{
	std::cout << "Follows host mouse clicked" << std::endl;
}

void
MainWindow::menu_mouse_capture()
{
	std::cout << "Mouse capture clicked" << std::endl;
}

void
MainWindow::menu_mouse_twobutton()
{
	std::cout << "Mouse two button clicked" << std::endl;
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
	cpu_idle_action = new QAction(tr("&Reduce CPU usage"), this);

	// Actions on the Settings->CD ROM Menu
	cdrom_disabled_action = new QAction(tr("&Disabled"), this);
	connect(cdrom_disabled_action, SIGNAL(triggered()), this, SLOT(menu_cdrom_disabled()));
	cdrom_empty_action = new QAction(tr("&Empty"), this);
	connect(cdrom_empty_action, SIGNAL(triggered()), this, SLOT(menu_cdrom_empty()));
	cdrom_iso_action = new QAction(tr("&Iso Image..."), this);
	connect(cdrom_iso_action, SIGNAL(triggered()), this, SLOT(menu_cdrom_iso()));

	// Aactions on the Settings->Mouse menu
	mouse_hack_action = new QAction(tr("&Follow host mouse"), this);
	connect(mouse_hack_action, SIGNAL(triggered()), this, SLOT(menu_mouse_hack()));
	mouse_capture_action = new QAction(tr("&Capture"), this);
	connect(mouse_capture_action, SIGNAL(triggered()), this, SLOT(menu_mouse_capture()));
	mouse_twobutton_action = new QAction(tr("&Two-button Mouse Mode"), this);
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

