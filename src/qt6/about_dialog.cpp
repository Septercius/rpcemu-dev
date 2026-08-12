/*
  RPCEmu - An Acorn system emulator

  Copyright (C) 2017 Peter Howkins

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
#include <stdio.h>

#include <iostream>

#include "about_dialog.h"

#include "rpcemu.h"

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
	setWindowTitle("About RPCEmu");

	// Image label
	image_label = new QLabel();
	QPixmap icon = QPixmap(":/rpcemu_icon.png");
	image_label->setPixmap(icon.scaledToWidth(48, Qt::SmoothTransformation));
	image_label->setAlignment(Qt::AlignTop);

	// Date, based on build date from __DATE__ std C macro
	const char *datestr = __DATE__; // Format is 08 Nov 2013

	// Text label string containing year from __DATE__
	QString str = QString("<h1>RPCEmu</h1>"
	    "<h2>" VERSION "</h2>"
	    "<p>Copyright 2005-%1 RPCEmu Developers</p>"
	    "<p>RPCEmu is released under the terms of the "
	    "GNU General Public License, Version 2. Please see the file "
	    "COPYING for more details.</p>").arg(datestr + 7);

	// Text label
	text_label = new QLabel(str);
	text_label->setWordWrap(true);

	// Create Buttons
	buttons_box = new QDialogButtonBox(QDialogButtonBox::Ok);

	hbox = new QHBoxLayout();
	hbox->addWidget(image_label);
	hbox->addWidget(text_label);

	hbox->setSpacing(16);

	// Main layout
	vbox = new QVBoxLayout(this);
	vbox->addLayout(hbox);
	vbox->addWidget(buttons_box);

	// Remove resize on Dialog
	this->setFixedSize(this->sizeHint());

	connect(buttons_box, &QDialogButtonBox::accepted, this, &QDialog::close);
}

AboutDialog::~AboutDialog()
{
}
