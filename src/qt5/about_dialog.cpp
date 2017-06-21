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
#include "config.h"

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
	setWindowTitle("About RPCEmu");

	this->setFixedWidth(350);

	// Create actions

	// Create widgets and layout
	icon_label = new QLabel("R"); // Temporary until program icon available
	icon_label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	icon_label->setFixedWidth(40);

	name_label = new QLabel("RPCEmu");
	name_label->setAlignment(Qt::AlignHCenter);
	name_label->setWordWrap(true);
	version_label = new QLabel(VERSION);
	version_label->setAlignment(Qt::AlignHCenter);
	version_label->setWordWrap(true);

	const char datestr[] = __DATE__; /* Format is 08 Nov 2013 */
	char copyright[64] = "";
	sprintf(copyright, "Copyright 2005-%s RPCEmu Developers", datestr + 7);
	copyright_label = new QLabel(copyright);
	copyright_label->setAlignment(Qt::AlignHCenter);
	copyright_label->setWordWrap(true);

	gpl_label = new QLabel("RPCEmu is released under the terms of the GNU General Public Licence, Version 2. Please see the file COPYING for more details.");
	gpl_label->setWordWrap(true);

	// Create Buttons
	buttons_box = new QDialogButtonBox(QDialogButtonBox::Ok);

	// Main layout
	grid = new QGridLayout(this);
	grid->addWidget(icon_label, 0, 0, 3, 1); // Span 3 rows
	grid->addWidget(name_label, 0, 1);
	grid->addWidget(version_label, 1, 1);
	grid->addWidget(copyright_label, 2, 1);

	grid->addWidget(gpl_label, 3, 0, 1, 2); // Span 2 columns
	grid->addWidget(buttons_box, 4, 0, 1, 2); // Span 2 columns

	connect(buttons_box, SIGNAL(accepted()), this, SLOT(accept()));
	connect(this, SIGNAL(accepted()), this, SLOT(dialog_accepted()));
}

AboutDialog::~AboutDialog()
{
}

/**
 * User clicked OK on the About dialog box 
 */
void
AboutDialog::dialog_accepted()
{
	this->close();
}

/**
 * User clicked close button on the About dialog box 
 */
void
AboutDialog::dialog_rejected()
{
	this->close();
}

