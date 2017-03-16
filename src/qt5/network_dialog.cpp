#include <iostream>

#include "network_dialog.h"

NetworkDialog::NetworkDialog(QWidget *parent)
    : QDialog(parent)
{
	setWindowTitle("Configure RPCEmu Networking");

	// Create actions

	// Create widgets and layout
	net_off = new QRadioButton("Off");
	net_bridging = new QRadioButton("Ethernet Bridging");

	bridge_label = new QLabel("Bridge Name");
	bridge_name = new QLineEdit(QString("rpcemu"));
	bridge_name->setMinimumWidth(192);
	bridge_hbox = new QHBoxLayout();
	bridge_hbox->insertSpacing(0, 48);
	bridge_hbox->addWidget(bridge_label);
	bridge_hbox->addWidget(bridge_name);

	// Create Buttons
	buttons_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);


	// Main layout
	vbox = new QVBoxLayout(this);
	vbox->addWidget(net_off);
	vbox->addWidget(net_bridging);
	vbox->addLayout(bridge_hbox);
	vbox->addWidget(buttons_box);

	// Connect actions to widgets
	connect(net_off, SIGNAL(clicked(bool)), this, SLOT(radio_clicked()));
	connect(net_bridging, SIGNAL(clicked(bool)), this, SLOT(radio_clicked()));

	connect(buttons_box, SIGNAL(accepted()), this, SLOT(accept()));
	connect(buttons_box, SIGNAL(rejected()), this, SLOT(reject()));

	connect(this, SIGNAL(accepted()), this, SLOT(dialog_accepted()));
	connect(this, SIGNAL(rejected()), this, SLOT(dialog_rejected()));

	net_off->setChecked(true);
	bridge_label->setEnabled(false);
	bridge_name->setEnabled(false);

	//this->setFixedSize(this->sizeHint());
}

NetworkDialog::~NetworkDialog()
{
}

void
NetworkDialog::radio_clicked()
{
	if (net_off->isChecked()) {
		bridge_label->setEnabled(false);
		bridge_name->setEnabled(false);
	} else {
		bridge_label->setEnabled(true);
		bridge_name->setEnabled(true);
	}
}

void
NetworkDialog::dialog_accepted()
{
	std::cout << "dialog_accepted()" << std::endl;
}

void
NetworkDialog::dialog_rejected()
{
	std::cout << "dialog_rejected()" << std::endl;
}
