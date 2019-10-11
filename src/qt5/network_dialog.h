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
#ifndef NETWORK_DIALOG_H
#define NETWORK_DIALOG_H

#include <QBoxLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>

#include "rpc-qt5.h"
#include "rpcemu.h"

class NetworkDialog : public QDialog
{
	Q_OBJECT

public:
	NetworkDialog(Emulator &emulator, Config *config_copy, QWidget *parent = 0);
	virtual ~NetworkDialog();

private slots:
	void radio_clicked();;
	void dialog_accepted();
	void dialog_rejected();

private:
	void applyConfig();

	QRadioButton *net_off;
	QRadioButton *net_nat;
	QRadioButton *net_bridging;
	QRadioButton *net_tunnelling;

	QLabel *bridge_label;
	QLineEdit *bridge_name;
	QHBoxLayout *bridge_hbox;

	QLabel *tunnelling_label;
	QLineEdit *tunnelling_name;
	QHBoxLayout *tunnelling_hbox;

	QDialogButtonBox *buttons_box;

	QVBoxLayout *vbox;

	Emulator &emulator;

	// Pointers to GUI thread copies of the emulator's config
	Config *config_copy;
};

#endif
