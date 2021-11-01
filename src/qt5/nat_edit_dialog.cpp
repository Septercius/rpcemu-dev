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

#include <QButtonGroup>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidgetItem>

#include "main_window.h"
#include "nat_edit_dialog.h"

NatEditDialog::NatEditDialog(QWidget *parent)
    : QDialog(parent)
{
	setWindowTitle("Configure NAT Port Forwarding Rules");

	nat_list_dialog = (NatListDialog *) parent;

	// Create widgets and layout
	label_proto     = new QLabel("Protocol");
	label_emu_port  = new QLabel("Emulator Port");
	label_host_port = new QLabel("Host Port");
	combobox_proto  = new QComboBox();
	combobox_proto->addItem("TCP");
	combobox_proto->addItem("UDP");
	spinbox_emu_port  = new QSpinBox();
	spinbox_emu_port->setRange(1, 65535);
	spinbox_host_port = new QSpinBox();
	spinbox_host_port->setRange(1, 65535);

	// Create Buttons
	buttons_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	buttons_box->button(QDialogButtonBox::Ok)->setText("Save Changes");

	// Main layout
	grid = new QGridLayout(this);
	grid->addWidget(label_proto, 0, 0);
	grid->addWidget(label_emu_port, 0, 1);
	grid->addWidget(label_host_port, 0, 2);
	grid->addWidget(combobox_proto, 1, 0);
	grid->addWidget(spinbox_emu_port, 1, 1);
	grid->addWidget(spinbox_host_port, 1, 2);
	grid->addWidget(buttons_box, 2, 0, 1, 3); // span 3 columns

	// Connect actions to widgets

	connect(buttons_box, &QDialogButtonBox::accepted, this, &NatEditDialog::dialog_accepted);
	connect(buttons_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

	connect(this, &QDialog::rejected, this, &NatEditDialog::dialog_rejected);

	// Remove resize on Dialog
	this->setFixedSize(this->sizeHint());
}


NatEditDialog::~NatEditDialog()
{
}

/**
 * User clicked OK on the dialog box
 */
void
NatEditDialog::dialog_accepted()
{
	PortForwardRule new_rule;

	// Extract data from the GUI
	if (combobox_proto->currentIndex() == 0) {
		new_rule.type = PORT_FORWARD_TCP;
	} else {
		new_rule.type = PORT_FORWARD_UDP;
	}
	new_rule.emu_port  = spinbox_emu_port->value();
	new_rule.host_port = spinbox_host_port->value();

	if (is_edit) {
		// Edit Rule

		// Same as old data?
		if (new_rule.type == old_rule.type
		    && new_rule.emu_port == old_rule.emu_port
		    && new_rule.host_port == old_rule.host_port)
		{
			// Close the dialog
			accept();
			return;
		}

		// validate new data, this will popup errors if needed
		if (nat_list_dialog->validate_edit(old_rule, new_rule) == false) {
			// Failed vailidate, don't close the dialog and let the user make changes
			return;
		}

		// Apply new data
		nat_list_dialog->process_edit(old_rule, new_rule);

	} else {
		// Add Rule

		// validate new data, this will popup errors if needed
		if (nat_list_dialog->validate_add(new_rule) == false) {
			// Failed vailidate, don't close the dialog and let the user make changes
			return;
		}

		// Apply new data
		nat_list_dialog->process_add(new_rule);
	}

	// Close the dialog
	accept();
}

/**
 * User clicked cancel on the dialog box
 */
void
NatEditDialog::dialog_rejected()
{
}

/**
 * In preparation for displaying the dialog set the values
 */
void
NatEditDialog::set_values(bool is_edit, PortForwardRule rule)
{
	this->is_edit = is_edit;

	if (is_edit) {
		// Edit an existing rule
		setWindowTitle("Edit Rule");

		// For edit, store the old values of the rule for comparison
		old_rule = rule;

		// Set the UI elements to the current values
		if (rule.type == PORT_FORWARD_TCP) {
			combobox_proto->setCurrentIndex(0);
		} else {
			// Is UDP
			combobox_proto->setCurrentIndex(1);
		}
		spinbox_emu_port->setValue(rule.emu_port);
		spinbox_host_port->setValue(rule.host_port);
	} else {
		// Add a new rule
		setWindowTitle("Add Rule");

		// Set the UI elements to example defaults
		combobox_proto->setCurrentText("TCP");
		spinbox_emu_port->setValue(1);
		spinbox_host_port->setValue(1);
	}
}
