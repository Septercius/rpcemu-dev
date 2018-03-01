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
#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QAction>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>

#include "configure_dialog.h"
#include "network_dialog.h"
#include "about_dialog.h"
#include "rpc-qt5.h"

#include "rpcemu.h"


/**
 * Used to pass data from Emulator thread to GUI thread
 * when the main display needs to be updated
 */
struct VideoUpdate {
	QImage		image;
	int		yl;
	int		yh;

	int		double_size;
	int		host_xsize;
	int		host_ysize;
};

/**
 * Used to pass data from Emulator thread to GUI thread
 * when in mousehack wants to move the host mouse
 */
struct MouseMoveUpdate {
	int16_t x;
	int16_t y;
};

class MainDisplay : public QWidget
{
	Q_OBJECT

public:
	MainDisplay(Emulator &emulator, QWidget *parent = 0);

	void get_host_size(int& host_xsize, int& host_ysize) const;
	void set_full_screen(bool full_screen);
	void update_image(const QImage& img, int yl, int yh, int double_size);
	int get_double_size();

protected:
	void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
	void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
	void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
	void paintEvent(QPaintEvent *event) Q_DECL_OVERRIDE;
	void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;

private:
	void calculate_scaling();

	Emulator &emulator;

	QImage *image;
	int double_size;

	bool full_screen;
	int host_xsize, host_ysize;
	int scaled_x, scaled_y;
	int offset_x, offset_y;
};


class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(Emulator &emulator);
	virtual ~MainWindow();

	/* Handle displaying error messages */
	void error(QString error);
	void fatal(QString error);

protected:
	void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;
	void keyPressEvent(QKeyEvent *event) Q_DECL_OVERRIDE;
	void keyReleaseEvent(QKeyEvent *event) Q_DECL_OVERRIDE;
#if defined(Q_OS_WIN32)
	bool nativeEvent(const QByteArray &eventType, void *message, long *result) Q_DECL_OVERRIDE;
#endif /* Q_OS_WIN32 */
	
private slots:
	void menu_reset();
	void menu_loaddisc0();
	void menu_loaddisc1();
	void menu_configure();
#ifdef RPCEMU_NETWORKING
	void menu_networking();
#endif /* RPCEMU_NETWORKING */
	void menu_fullscreen();
	void menu_cpu_idle();
	void menu_cdrom_disabled();
	void menu_cdrom_empty();
	void menu_cdrom_iso();
	void menu_cdrom_ioctl();
	void menu_cdrom_win_ioctl();
	void menu_mouse_hack();
	void menu_mouse_twobutton();
	void menu_online_manual();
	void menu_visit_website();
	void menu_about();

	void main_display_update(VideoUpdate video_update);
	void move_host_mouse(MouseMoveUpdate mouse_update);

	// MIPS counting
	void mips_timer_timeout();

	void application_state_changed(Qt::ApplicationState state);
signals:
	void main_display_signal(VideoUpdate video_update);
	void move_host_mouse_signal(MouseMoveUpdate mouse_update);

	void error_signal(QString error);
	void fatal_signal(QString error);

private:
	void create_actions();
	void create_menus();
	void create_tool_bars();

	void readSettings();
	void writeSettings();

	void cdrom_menu_selection_update(const QAction *cdrom_action);

	bool full_screen;
	bool reenable_mousehack; ///< Did we disable mousehack entering fullscreen and have to reenable it on leaving fullscreen?

	MainDisplay *display;

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

	// Actions on Disc menu
	QAction *loaddisc0_action;
	QAction *loaddisc1_action;

	// Actions on Settings menu (and submenus)
	QAction *configure_action;
#ifdef RPCEMU_NETWORKING
	QAction *networking_action;
#endif /* RPCEMU_NETWORKING */
	QAction *fullscreen_action;
	QAction *cpu_idle_action;
	QAction *cdrom_disabled_action;
	QAction *cdrom_empty_action;
#if defined(Q_OS_LINUX)
	QAction *cdrom_ioctl_action;
#endif /* linux */
#if defined(Q_OS_WIN32)
	std::vector<QAction *> cdrom_win_ioctl_actions;
#endif /* win32 */
	QAction *cdrom_iso_action;
	QAction *mouse_hack_action;
	QAction *mouse_twobutton_action;

	// Actions on About menu
	QAction *online_manual_action;
	QAction *visit_website_action;
	QAction *about_action;

	// Dialogs
	ConfigureDialog *configure_dialog;
	NetworkDialog *network_dialog;
	AboutDialog *about_dialog;

	// Pointer to emulator instance
	Emulator &emulator;

	// GUI thread copy of the emulator's config
	Config config_copy;
	Model model_copy;

	// MIPS counting
	QTimer mips_timer;
	uint64_t mips_total_instructions;
	int32_t mips_seconds;
	QString window_title;
	
	// List of keys currently held down, released when window loses focus
	std::list<quint32> held_keys;

	bool infocus; ///< Does the main window currently have the focus
};

#endif
