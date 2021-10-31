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
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidgetItem>

#include "main_window.h"
#include "nat_edit_dialog.h"
#include "nat_list_dialog.h"

NatListDialog::NatListDialog(Emulator &emulator, QWidget *parent)
    : QDialog(parent),
	emulator(emulator)
{
	setWindowTitle("Configure NAT Port Forwarding Rules");

	nat_edit_dialog = new NatEditDialog(this);

	// Create widgets and layout
	rules_tablewidget = new QTableWidget();
	rules_tablewidget->setColumnCount(5);
	QStringList headers;
	headers << "Protocol" << "Emulator Port" << "Host Port" << "Delete" << "Edit";
	rules_tablewidget->setHorizontalHeaderLabels(headers);
	rules_tablewidget->verticalHeader()->setVisible(false);

	// Make the table non editable
	rules_tablewidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
	rules_tablewidget->setFocusPolicy(Qt::NoFocus);
	rules_tablewidget->setSelectionMode(QAbstractItemView::NoSelection);

	// Adjust the table widths to its contents
	rules_tablewidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	rules_tablewidget->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
	rules_tablewidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	rules_tablewidget->horizontalHeader()->setStretchLastSection(true);

	// Create Buttons
	buttons_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	buttons_box->button(QDialogButtonBox::Ok)->setText("Add Rule");
	buttons_box->button(QDialogButtonBox::Cancel)->setText("Close");

	// Main layout
	vbox = new QVBoxLayout(this);
	vbox->addWidget(rules_tablewidget);
	vbox->addWidget(buttons_box);

	// Connect actions to widgets

	connect(buttons_box, &QDialogButtonBox::accepted, this, &NatListDialog::dialog_accepted);
	connect(buttons_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

	connect(this, &QDialog::rejected, this, &NatListDialog::dialog_rejected);

	// Fix width on Dialog
	this->setFixedWidth(this->sizeHint().width());
}


NatListDialog::~NatListDialog()
{
	delete nat_edit_dialog;
}

/**
 * User clicked OK on the dialog box
 */
void
NatListDialog::dialog_accepted()
{
	// List is full?
	if (rules_tablewidget->rowCount() >= MAX_PORT_FORWARDS) {
		error("You are at the maximum number of rules. Please remove a rule to make space for a new one.");
		return;
	}

	PortForwardRule rule = { PORT_FORWARD_TCP, 1, 1 };

	nat_edit_dialog->set_values(false /* is Add */, rule);
	nat_edit_dialog->exec(); // Modal
}

/**
 * User clicked close on the dialog box
 */
void
NatListDialog::dialog_rejected()
{
}

void
NatListDialog::add_nat_rule(PortForwardRule rule)
{
	// Search list looking for correct place to insert the rule
	int insert_row = 0;
	while (insert_row < rules_tablewidget->rowCount()) {
		if (rule.emu_port < rules_tablewidget->item(insert_row, 1)->text().toUInt()) {
			// Insert at this position aka, before the rule
			break;
		} else if (rule.emu_port == rules_tablewidget->item(insert_row, 1)->text().toUInt()) {
			if (rule.type == PORT_FORWARD_TCP) {
				// TCP rules go before UDP rules if there's a duplicate port
				break;
			} else {
				insert_row++;
				break;
			}
		}

		// port number is higher than entry, try next, or go past end of list and insert on the end
		insert_row++;
	}

	rules_tablewidget->insertRow(insert_row);

	// Rule Type and Ports
	QTableWidgetItem *item;

	item = new QTableWidgetItem(rule.type == PORT_FORWARD_TCP ? "TCP" : "UDP");
	item->setFlags(item->flags() & ~Qt::ItemIsEditable);
	rules_tablewidget->setItem(insert_row, 0, item);

	item = new QTableWidgetItem(QString::number(rule.emu_port));
	item->setFlags(item->flags() & ~Qt::ItemIsEditable);
	rules_tablewidget->setItem(insert_row, 1, item);

	item = new QTableWidgetItem(QString::number(rule.host_port));
	item->setFlags(item->flags() & ~Qt::ItemIsEditable);
	rules_tablewidget->setItem(insert_row, 2, item);

	// Delete button
	QPushButton *btn1 = new QPushButton();
	btn1->setIcon(style()->standardIcon(QStyle::SP_BrowserStop));
	btn1->setVisible(true);
	connect(btn1, &QPushButton::clicked, this, &NatListDialog::delete_button_clicked);
	rules_tablewidget->setCellWidget(insert_row, 3, btn1);

	// Edit button
	QPushButton *btn2 = new QPushButton();
	btn2->setIcon(style()->standardIcon(QStyle::SP_ArrowRight));
	btn2->setVisible(true);
	connect(btn2, &QPushButton::clicked, this, &NatListDialog::edit_button_clicked);
	rules_tablewidget->setCellWidget(insert_row, 4, btn2);
}

void
NatListDialog::delete_button_clicked()
{
	QWidget *w = qobject_cast<QWidget *>(sender());

	if (w != NULL) {
		const int row = rules_tablewidget->indexAt(w->pos()).row();
		PortForwardRule rule;

		if (QString::compare(rules_tablewidget->item(row, 0)->text(), "TCP") == 0) {
			rule.type = PORT_FORWARD_TCP;
		} else {
			rule.type = PORT_FORWARD_UDP;
		}
		rule.emu_port  = rules_tablewidget->item(row, 1)->text().toUInt();
		rule.host_port = rules_tablewidget->item(row, 2)->text().toUInt();

		QMessageBox msgBox(this);
		msgBox.setWindowTitle("RPCEmu");
		msgBox.setText(QString("Are you sure you want to delete NAT rule %1, emu port %2, host port %3?")
		    .arg(rule.type == PORT_FORWARD_TCP ? "TCP" : "UDP")
		    .arg(rule.emu_port)
		    .arg(rule.host_port));
		msgBox.setIcon(QMessageBox::Question);
		msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
		msgBox.setDefaultButton(QMessageBox::Cancel);

		int ret = msgBox.exec();
		switch (ret) {
		case QMessageBox::Ok:
			// Remove from GUI
			rules_tablewidget->removeRow(row);

			// Pass on message to emulator thread to modify the copy there, and actually add the rule
			emit this->emulator.nat_rule_remove_signal(rule);

			break;

		case QMessageBox::Cancel:
		default:
			break;
		}
	}
}

void
NatListDialog::edit_button_clicked()
{
	QWidget *w = qobject_cast<QWidget *>(sender());

	if (w != NULL) {
		const int row = rules_tablewidget->indexAt(w->pos()).row();
		PortForwardRule rule;

		if (QString::compare(rules_tablewidget->item(row, 0)->text(), "TCP") == 0) {
			rule.type = PORT_FORWARD_TCP;
		} else {
			rule.type = PORT_FORWARD_UDP;
		}
		rule.emu_port = rules_tablewidget->item(row, 1)->text().toUInt();
		rule.host_port = rules_tablewidget->item(row, 2)->text().toUInt();

		// Open dialog
		nat_edit_dialog->set_values(true, rule);
		nat_edit_dialog->exec(); // Modal
	}
}

bool
NatListDialog::validate_add(PortForwardRule rule)
{
	// Loop through the list checking for duplicate port usage
	for (int i = 0; i < rules_tablewidget->rowCount(); i++) {
		PortForwardRule row_rule;

		if (QString::compare(rules_tablewidget->item(i, 0)->text(), "TCP") == 0) {
			row_rule.type = PORT_FORWARD_TCP;
		} else {
			row_rule.type = PORT_FORWARD_UDP;
		}
		row_rule.emu_port  = rules_tablewidget->item(i, 1)->text().toUInt();
		row_rule.host_port = rules_tablewidget->item(i, 2)->text().toUInt();

		if (rule.type == row_rule.type && rule.emu_port == row_rule.emu_port) {
			error("Emulated port is in use in another rule");
			return false;
		}

		if (rule.type == row_rule.type && rule.host_port == row_rule.host_port) {
			error("Host port is in use in another rule");
			return false;
		}
	}

	// Add is fine
	return true;
}

bool
NatListDialog::validate_edit(PortForwardRule old_rule, PortForwardRule new_rule)
{
	// Loop through the list checking for duplicate port usage
	for (int i = 0; i < rules_tablewidget->rowCount(); i++) {
		PortForwardRule row_rule;

		if (QString::compare(rules_tablewidget->item(i, 0)->text(), "TCP") == 0) {
			row_rule.type = PORT_FORWARD_TCP;
		} else {
			row_rule.type = PORT_FORWARD_UDP;
		}
		row_rule.emu_port  = rules_tablewidget->item(i, 1)->text().toUInt();
		row_rule.host_port = rules_tablewidget->item(i, 2)->text().toUInt();

		// Don't worry about duplicating any ports from rule's old values
		if (row_rule.type == old_rule.type
		    && row_rule.emu_port == old_rule.emu_port
		    && row_rule.host_port == old_rule.host_port)
		{
			continue;
		}

		if (new_rule.type == row_rule.type && new_rule.emu_port == row_rule.emu_port) {
			error("Emulated port is in use in another rule");
			return false;
		}

		if (new_rule.type == row_rule.type && new_rule.host_port == row_rule.host_port) {
			error("Host port is in use in another rule");
			return false;
		}
	}

	// Edit is fine
	return true;
}

void
NatListDialog::process_add(PortForwardRule rule)
{
	// Add Item to GUI list
	add_nat_rule(rule);

	// Pass on message to emulator thread to modify the copy there, and actually add the rule
	emit this->emulator.nat_rule_add_signal(rule);
}

void
NatListDialog::process_edit(PortForwardRule old_rule, PortForwardRule new_rule)
{
	// Remove old rule from GUI
	for (int i = 0; i < rules_tablewidget->rowCount(); i++) {
		PortForwardRule row_rule;

		if (QString::compare(rules_tablewidget->item(i, 0)->text(), "TCP") == 0) {
			row_rule.type = PORT_FORWARD_TCP;
		} else {
			row_rule.type = PORT_FORWARD_UDP;
		}
		row_rule.emu_port  = rules_tablewidget->item(i, 1)->text().toUInt();
		row_rule.host_port = rules_tablewidget->item(i, 2)->text().toUInt();

		if (row_rule.type == old_rule.type
		    && row_rule.emu_port == old_rule.emu_port
		    && row_rule.host_port == old_rule.host_port)
		{
			rules_tablewidget->removeRow(i);
			break;
		}
	}

	// Add new rule to GUI
	add_nat_rule(new_rule);

	// Pass on message to emulator thread to modify the copy there, and actually add the rule
	emit this->emulator.nat_rule_edit_signal(old_rule, new_rule);
}
