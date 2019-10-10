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

#include "network_dialog.h"

#include "network.h"

NetworkDialog::NetworkDialog(Emulator &emulator, Config *config_copy, Model *model_copy, QWidget *parent)
    : QDialog(parent),
	emulator(emulator),
	config_copy(config_copy),
	model_copy(model_copy)
{
	setWindowTitle("Configure RPCEmu Networking");

	// Create actions

	// Create widgets and layout
	net_off = new QRadioButton("Off");
	net_bridging = new QRadioButton("Ethernet Bridging");
	net_tunnelling = new QRadioButton("IP Tunnelling");

	bridge_label = new QLabel("Bridge Name");
	bridge_name = new QLineEdit(QString("rpcemu"));
	bridge_name->setMinimumWidth(192);
	bridge_hbox = new QHBoxLayout();
	bridge_hbox->insertSpacing(0, 48);
	bridge_hbox->addWidget(bridge_label);
	bridge_hbox->addWidget(bridge_name);

	tunnelling_label = new QLabel("IP Address");
	tunnelling_name = new QLineEdit(QString("172.31.0.1"));
	tunnelling_name->setMinimumWidth(192);
	tunnelling_hbox = new QHBoxLayout();
	tunnelling_hbox->insertSpacing(0, 48);
	tunnelling_hbox->addWidget(tunnelling_label);
	tunnelling_hbox->addWidget(tunnelling_name);

	// Create Buttons
	buttons_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);


	// Main layout
	vbox = new QVBoxLayout(this);
	vbox->addWidget(net_off);
	vbox->addWidget(net_bridging);
	vbox->addLayout(bridge_hbox);

	// IP Tunnelling is linux only
#if defined(Q_OS_LINUX)
	vbox->addWidget(net_tunnelling);
	vbox->addLayout(tunnelling_hbox);
#endif /* linux */

	vbox->addWidget(buttons_box);

	// Connect actions to widgets
	connect(net_off, &QRadioButton::clicked, this, &NetworkDialog::radio_clicked);
	connect(net_bridging, &QRadioButton::clicked, this, &NetworkDialog::radio_clicked);
	connect(net_tunnelling, &QRadioButton::clicked, this, &NetworkDialog::radio_clicked);

	connect(buttons_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

	connect(this, &QDialog::accepted, this, &NetworkDialog::dialog_accepted);
	connect(this, &QDialog::rejected, this, &NetworkDialog::dialog_rejected);

	// Set the values of the window to the config values
	applyConfig();

	// Remove resize on Dialog
	this->setFixedSize(this->sizeHint());
}

NetworkDialog::~NetworkDialog()
{
}

void
NetworkDialog::radio_clicked()
{
	if (net_bridging->isChecked()) {
		bridge_label->setEnabled(true);
		bridge_name->setEnabled(true);
	} else {
		bridge_label->setEnabled(false);
		bridge_name->setEnabled(false);
	}

	if (net_tunnelling->isChecked()) {
		tunnelling_label->setEnabled(true);
		tunnelling_name->setEnabled(true);
	} else {
		tunnelling_label->setEnabled(false);
		tunnelling_name->setEnabled(false);
	}
}

/**
 * User clicked OK on the Networking dialog box 
 */
void
NetworkDialog::dialog_accepted()
{
	QByteArray ba_bridgename, ba_ipaddress;
	char *bridgename, *ipaddress;
	NetworkType network_type = NetworkType_Off;

	// Fill in the choices from the dialog box
	if (net_off->isChecked()) {
		network_type = NetworkType_Off;
	} else if (net_bridging->isChecked()) {
		network_type = NetworkType_EthernetBridging;
	} else if (net_tunnelling->isChecked()) {
		network_type = NetworkType_IPTunnelling;
	}

	ba_bridgename = bridge_name->text().toUtf8();
	bridgename = ba_bridgename.data();

	ba_ipaddress = tunnelling_name->text().toUtf8();
	ipaddress = ba_ipaddress.data();

	// This is iffy thread safety, this function sets the networking
	// config from the GUI thread, luckily the config is only read
	// on emulator restart which happens in the emu thread.
	if(network_config_changed(network_type, bridgename, ipaddress)) {
		// Emulator reset required
		emit this->emulator.reset_signal();
	}

	// Apply configuration settings from Dialog to config_copy
	config_copy->network_type = network_type;
	if (config_copy->bridgename == NULL) {
		config_copy->bridgename = strdup(bridgename);
	} else if (strcmp(config_copy->bridgename, bridgename) != 0) {
		free(config_copy->bridgename);
		config_copy->bridgename = strdup(bridgename);
	}
	if (config_copy->ipaddress == NULL) {
		config_copy->ipaddress = strdup(ipaddress);
	} else if (strcmp(config_copy->ipaddress, ipaddress) != 0) {
		free(config_copy->ipaddress);
		config_copy->ipaddress = strdup(ipaddress);
	}
}

/**
 * User clicked cancel on the Networking dialog box 
 */
void
NetworkDialog::dialog_rejected()
{
	// Set the values in the dialog back to the current settings
	applyConfig();
}

/**
 * Set the values in the networking dialog box based on the current
 * values of the GUI config copy
 */
void
NetworkDialog::applyConfig()
{
//	if windows and iptunnelling, net = off

	// Select the correct radio button
	net_off->setChecked(false);
	net_bridging->setChecked(false);
	net_tunnelling->setChecked(false);
	switch (config_copy->network_type) {
	case NetworkType_Off:
		net_off->setChecked(true);
		break;
	case NetworkType_EthernetBridging:
		net_bridging->setChecked(true);
		break;
	case NetworkType_IPTunnelling:
		net_tunnelling->setChecked(true);
		break;
	}

	// Use the helper function to grey out the boxes of unselected
	// network types
	radio_clicked();

	if(config_copy->bridgename && config_copy->bridgename[0] != '\0') {
		bridge_name->setText(config_copy->bridgename);
	}

	if(config_copy->ipaddress && config_copy->ipaddress[0] != '\0') {
		tunnelling_name->setText(config_copy->ipaddress);
	}
}
