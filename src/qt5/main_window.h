#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>

#include "configure_dialog.h"
#include "network_dialog.h"
#include "rpc-qt5.h"

class QAction;
class QLabel;
class QMenu;

class MainLabel : public QLabel
{
	Q_OBJECT

public:
	MainLabel(Emulator &emulator);

protected:
	void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
	void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
	void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;

private:
	Emulator &emulator;
};
     
class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(Emulator &emulator);
	virtual ~MainWindow();
	QLabel *label;

protected:
	void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;
	void keyPressEvent(QKeyEvent *event) Q_DECL_OVERRIDE;
	void keyReleaseEvent(QKeyEvent *event) Q_DECL_OVERRIDE;

private slots:
	void menu_reset();
	void menu_configure();
	void menu_networking();
	void menu_online_manual();
	void menu_visit_website();
	void menu_about();

	void main_display_update(QPixmap pixmap);

signals:
        void main_display_signal(QPixmap);

private:
	void create_actions();
	void create_menus();
	void create_tool_bars();
	void create_status_bar();

	void readSettings();
	void writeSettings();

	bool full_screen;

	QImage *image;

	QString curFile;

	// Menus
	QMenu *file_menu;
	QMenu *disc_menu;
	QMenu *settings_menu;
	QMenu *cdrom_menu;
	QMenu *mouse_menu;
	QMenu *help_menu;

	// Actions on File menu
	QAction *reset_action;
	QAction *exit_action;

	// Actions on Settings menu (and submenus)
	QAction *configure_action;
	QAction *networking_action;
	QAction *fullscreen_action;
	QAction *cpu_idle_action;
	QAction *cdrom_disabled_action;

	// Actions on About menu
	QAction *online_manual_action;
	QAction *visit_website_action;
	QAction *about_action;
	QAction *aboutQtAct;

	// Dialogs
	ConfigureDialog *configure_dialog;
	NetworkDialog *network_dialog;


	Emulator &emulator;
};

#endif
