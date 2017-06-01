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
#ifndef ABOUT_DIALOG_H
#define ABOUT_DIALOG_H

#include <QDialog>
#include <QtWidgets>

class AboutDialog : public QDialog
{
	Q_OBJECT

public:
	AboutDialog(QWidget *parent = 0);
	virtual ~AboutDialog();

private slots:
	void dialog_accepted();
	void dialog_rejected();

private:
	QLabel *icon_label;
	QLabel *name_label;
	QLabel *version_label;
	QLabel *copyright_label;
	QLabel *gpl_label;

	QDialogButtonBox *buttons_box;

	QGridLayout *grid;
};

#endif /* ABOUT_DIALOG */
