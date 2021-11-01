/*
  RPCEmu - An Acorn system emulator

  Copyright (C) 2020 Peter Howkins

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
#ifndef NAT_EDIT_DIALOG_H
#define NAT_EDIT_DIALOG_H

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>

#include "rpc-qt5.h"
#include "rpcemu.h"

class NatListDialog;

class NatEditDialog : public QDialog
{
	Q_OBJECT

public:
	NatEditDialog(QWidget *parent);
	virtual ~NatEditDialog();

	void set_values(bool is_edit, PortForwardRule rule);


private slots:
	void dialog_accepted();
	void dialog_rejected();

private:
	QLabel *label_proto;
	QLabel *label_emu_port;
	QLabel *label_host_port;
	QComboBox *combobox_proto;
	QSpinBox *spinbox_emu_port;
	QSpinBox *spinbox_host_port;
	QDialogButtonBox *buttons_box;
	QGridLayout *grid;

	bool is_edit; // Else Add
	PortForwardRule old_rule; // For Edit, store the old values of the rule, for comparison to the user's choices

	NatListDialog *nat_list_dialog;
};

#endif
