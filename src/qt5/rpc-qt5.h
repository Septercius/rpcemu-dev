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
#ifndef RPC_QT5_H
#define RPC_QT5_H

#include <stdint.h>

#include <QtCore>
#include <QAtomicInt>
#include <QKeyEvent>

#include "rpcemu.h"

/// Instruction counter shared between Emulator and GUI threads
extern QAtomicInt instruction_count;
extern QAtomicInt iomd_timer_count; ///< IOMD timer counter shared between Emulator and GUI threads
extern QAtomicInt video_timer_count; ///< Video timer counter shared between Emulator and GUI threads

class Emulator : public QObject {
	Q_OBJECT
public:
	Emulator();
signals:
	void key_press_signal(unsigned scan_code);

	void key_release_signal(unsigned scan_code);

	void mouse_move_signal(int x, int y);
	void mouse_press_signal(int buttons);
	void mouse_release_signal(int buttons);

	// GUI actions
	void reset_signal();
	void exit_signal();
	void load_disc_0_signal(QString discname);
	void load_disc_1_signal(QString discname);
	void cpu_idle_signal();
	void cdrom_disabled_signal();
	void cdrom_empty_signal();
	void cdrom_load_iso_signal(const QString &discname);
	void cdrom_ioctl_signal();
	void cdrom_win_ioctl_signal(char drive_letter);
	void mouse_hack_signal();
	void mouse_capture_signal();
	void mouse_twobutton_signal();
	void config_updated_signal(Config *new_config, Model new_model);

public slots:
	void mainemuloop();

	void key_press(unsigned scan_code);

	void key_release(unsigned scan_code);

	void mouse_move(int x, int y);
	void mouse_press(int buttons);
	void mouse_release(int buttons);

	// GUI actions
	void reset();
	void exit();
	void load_disc_0(QString discname);
	void load_disc_1(QString discname);
	void cpu_idle();
	void cdrom_disabled();
	void cdrom_empty();
	void cdrom_load_iso(QString discname);
#if defined(Q_OS_LINUX)
	void cdrom_ioctl();
#endif /* linux */
#if defined(Q_OS_WIN32)
	void cdrom_win_ioctl(char drive_letter);
#endif /* win32 */
	void mouse_hack();
	void mouse_capture();
	void mouse_twobutton();
	void config_updated(Config *new_config, Model new_model);

private:
	int32_t video_timer_interval; ///< Interval between video timer events (in nanoseconds)
};

#endif /* RPC_QT5_H */
