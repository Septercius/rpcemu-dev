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
#ifndef CONFIGURE_DIALOG_H
#define CONFIGURE_DIALOG_H

#include <vector>

#include <QDialog>
#include <QtWidgets>

#include "rpc-qt5.h"
#include "rpcemu.h"

class ConfigureDialog : public QDialog
{
	Q_OBJECT

public:
	ConfigureDialog(Emulator *emulator, Config *config_copy, Model *model_copy, QWidget *parent = 0);
	virtual ~ConfigureDialog();

	void keyPressEvent(QKeyEvent *);

private slots:
	void slider_moved(int value);

	void dialog_accepted();
	void dialog_rejected();

private:
	void applyConfig();

	QListWidget *hardware_listwidget;
	std::vector<QListWidgetItem *>  hardware_list_items;
	QVBoxLayout *hardware_vbox;
	QGroupBox *hardware_group_box;

	QButtonGroup *mem_group;
	QRadioButton *mem_4, *mem_8, *mem_16, *mem_32, *mem_64, *mem_128, *mem_256;
	QVBoxLayout *mem_vbox;
	QGroupBox *mem_group_box;

	QButtonGroup *vram_group;
	QRadioButton *vram_0, *vram_2;
	QVBoxLayout *vram_vbox;
	QGroupBox *vram_group_box;

	QCheckBox *sound_checkbox;

	QSlider *refresh_slider;
	QLabel *refresh_label;
	QHBoxLayout *refresh_hbox;
	QGroupBox *refresh_group_box;

	QDialogButtonBox *buttons_box;

	QGridLayout *grid;

	Emulator *emulator;

	// Pointers to GUI thread copies of the emulator's config
	Config *config_copy;
	Model *model_copy;

};

#endif
