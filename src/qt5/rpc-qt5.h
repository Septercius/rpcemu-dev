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
	void load_disc_0_signal(const QString &discname);
	void load_disc_1_signal(const QString &discname);
	void cpu_idle_signal();
	void cdrom_disabled_signal();
	void cdrom_empty_signal();
	void cdrom_load_iso_signal(const QString &discname);
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
	void load_disc_0(const QString &discname);
	void load_disc_1(const QString &discname);
	void cpu_idle();
	void cdrom_disabled();
	void cdrom_empty();
	void cdrom_load_iso(const QString &discname);
	void mouse_hack();
	void mouse_capture();
	void mouse_twobutton();
	void config_updated(Config *new_config, Model new_model);
};



class VLBUpdateTimer : public QTimer {
  Q_OBJECT
public:
  VLBUpdateTimer(QObject *parent = 0);
private slots:
  void VLBUpdate();
};

#endif /* RPC_QT5_H */
