#include <iostream>

#include "configure_dialog.h"

ConfigureDialog::ConfigureDialog(QWidget *parent)
    : QDialog(parent)
{
	setWindowTitle("Configure RPCEmu");

	// Create actions

	// Create widgets and layout


	// Create Memory Group
	mem_4 = new QRadioButton("4 MB");
	mem_8 = new QRadioButton("8 MB");
	mem_16 = new QRadioButton("16 MB");
	mem_32 = new QRadioButton("32 MB");
	mem_64 = new QRadioButton("64 MB");
	mem_128 = new QRadioButton("128 MB");
	mem_256 = new QRadioButton("256 MB");

	mem_group = new QButtonGroup();
	mem_group->addButton(mem_4);
	mem_group->addButton(mem_8);
	mem_group->addButton(mem_16);
	mem_group->addButton(mem_32);
	mem_group->addButton(mem_64);
	mem_group->addButton(mem_128);
	mem_group->addButton(mem_256);

	mem_vbox = new QVBoxLayout();
	mem_vbox->addWidget(mem_4);
	mem_vbox->addWidget(mem_8);
	mem_vbox->addWidget(mem_16);
	mem_vbox->addWidget(mem_32);
	mem_vbox->addWidget(mem_64);
	mem_vbox->addWidget(mem_128);
	mem_vbox->addWidget(mem_256);

	mem_group_box = new QGroupBox("RAM");
	mem_group_box->setLayout(mem_vbox);



	// Create refresh
	refresh_slider = new QSlider(Qt::Horizontal);
	refresh_slider->setRange(20, 100);
	refresh_slider->setValue(60);
	refresh_slider->setTickPosition(QSlider::TicksBothSides);
	refresh_slider->setFixedWidth(256);

	refresh_label = new QLabel("60 Hz");

	refresh_hbox = new QHBoxLayout();
	refresh_hbox->addWidget(refresh_slider);
	refresh_hbox->addWidget(refresh_label);

	refresh_group_box = new QGroupBox("Video refresh rate");
	refresh_group_box->setLayout(refresh_hbox);

	// Create Buttons
	buttons_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

	// Main layout

	vbox = new QVBoxLayout(this);
	vbox->addWidget(mem_group_box);
	vbox->addWidget(refresh_group_box);
	vbox->addWidget(buttons_box);

	// Connect actions to widgets

	connect(refresh_slider, SIGNAL(sliderMoved(int)), this, SLOT(slider_moved(int)));

	connect(buttons_box, SIGNAL(accepted()), this, SLOT(accept()));
	connect(buttons_box, SIGNAL(rejected()), this, SLOT(reject()));

	connect(this, SIGNAL(accepted()), this, SLOT(dialog_accepted()));
	connect(this, SIGNAL(rejected()), this, SLOT(dialog_rejected()));

	mem_32->setChecked(true);

	//this->setFixedSize(this->sizeHint());

}

ConfigureDialog::~ConfigureDialog()
{
}

void
ConfigureDialog::keyPressEvent(QKeyEvent *evt)
{
	std::cout << "keyPressEvent()" << std::endl;

	if (evt->key() == Qt::Key_Enter || evt->key() == Qt::Key_Return) {
		return;
	}
	QDialog::keyPressEvent(evt);
}

void
ConfigureDialog::slider_moved(int value)
{
	refresh_label->setText(QString::number(value) + " Hz");
}

void
ConfigureDialog::dialog_accepted()
{
	std::cout << "dialog_accepted()" << std::endl;
}

void
ConfigureDialog::dialog_rejected()
{
	std::cout << "dialog_rejected()" << std::endl;
}
