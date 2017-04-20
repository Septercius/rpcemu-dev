#include <iostream>

#include <QDesktopServices>
#include <QtWidgets>

#include "keyboard.h"

#include "main_window.h"
#include "rpc-qt5.h"

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

	// Actions on Settings menu
	configure_action = new QAction(tr("&Configure..."), this);
	connect(configure_action, SIGNAL(triggered()), this, SLOT(menu_configure()));
	networking_action = new QAction(tr("&Networking..."), this);
	connect(networking_action, SIGNAL(triggered()), this, SLOT(menu_networking()));
	fullscreen_action = new QAction(tr("&Fullscreen mode"), this);
	cpu_idle_action = new QAction(tr("&Reduce CPU usage"), this);
	cdrom_disabled_action = new QAction(tr("&Disabled"), this);

	// Actions on About menu
	online_manual_action = new QAction(tr("Online &Manual..."), this);
	connect(online_manual_action, SIGNAL(triggered()), this, SLOT(menu_online_manual()));
	visit_website_action = new QAction(tr("Visit &Website..."), this);
	connect(visit_website_action, SIGNAL(triggered()), this, SLOT(menu_visit_website()));

	about_action = new QAction(tr("&About RPCEmu..."), this);
	about_action->setStatusTip(tr("Show the application's About box"));
	connect(about_action, SIGNAL(triggered()), this, SLOT(menu_about()));

	aboutQtAct = new QAction(tr("About &Qt..."), this);
	aboutQtAct->setStatusTip(tr("Show the Qt library's About box"));
	connect(aboutQtAct, SIGNAL(triggered()), qApp, SLOT(aboutQt()));

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

	// Mouse submenu (nothing yet)



	menuBar()->addSeparator();

	// Help menu
	help_menu = menuBar()->addMenu(tr("&Help"));
	help_menu->addAction(online_manual_action);
	help_menu->addAction(visit_website_action);
	help_menu->addSeparator();
	help_menu->addAction(about_action);
	help_menu->addAction(aboutQtAct);
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

