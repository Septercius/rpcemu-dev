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

#ifndef CHOOSE_DIALOG_H
#define CHOOSE_DIALOG_H

#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include "rpc-qt5.h"
#include "rpcemu.h"

class ChooseDialog : public QDialog
{
  
  Q_OBJECT
  
public:
  ChooseDialog(QWidget *parent = 0);
  virtual ~ChooseDialog();
  
private slots:
  void directory_button_pressed();
  
  void dialog_accepted();
  void dialog_rejected();
  
private:
  
  QLabel *preamble_label;
  QLabel *choose_label;
  
  QHBoxLayout *directory_hbox;
  QLineEdit *directory_edit;
  QPushButton *directory_button;
  
  QDialogButtonBox *buttons_box;
  
  QGridLayout *grid;
  
};

#endif
