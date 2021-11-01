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
#ifndef NAT_LIST_DIALOG_H
#define NAT_LIST_DIALOG_H

#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QTableWidget>

#include "rpc-qt5.h"
#include "rpcemu.h"

class NatEditDialog;

class NatListDialog : public QDialog
{
	Q_OBJECT

public:
	NatListDialog(Emulator &emulator, QWidget *parent);
	virtual ~NatListDialog();

	void add_nat_rule(PortForwardRule rule);

	// Before GUI adding or editing a rule, make sure it's valid
	bool validate_add(PortForwardRule rule);
	bool validate_edit(PortForwardRule old_rule, PortForwardRule new_rule);
	// GUI adding or editing a rule
	void process_add(PortForwardRule rule);
	void process_edit(PortForwardRule old_rule, PortForwardRule new_rule);

private slots:
	void dialog_accepted();
	void dialog_rejected();

	void delete_button_clicked();
	void edit_button_clicked();

private:
	QTableWidget *rules_tablewidget;
	QDialogButtonBox *buttons_box;
	QVBoxLayout *vbox;

	Emulator &emulator;

	// Sub dialogs
	NatEditDialog *nat_edit_dialog;
};

#endif
