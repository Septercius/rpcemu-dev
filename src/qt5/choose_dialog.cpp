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

#include <QFileDialog>
#include "choose_dialog.h"
#include "preferences-macosx.h"

ChooseDialog::ChooseDialog(QWidget *parent) : QDialog(parent)
{
  setWindowTitle("RPCEmu - Choose Data Directory");
  
  buttons_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  
  // Create preamble label.
  QString str = QString("<p>Before using RPCEmu for the first time, you must select the directory <br/>"
                        "that contains the folders and files required by the emulator, such as <br>"
                        "ROMs, hard drive images and the HostFS share.</p>"
                        "<p>You can show this dialogue again by holding down the Shift key <br/>"
                        "whilst the application is loading.</p>");

  preamble_label = new QLabel(str);
  
  // Create choose label.
  choose_label = new QLabel();
  choose_label->setText("Please choose a directory below:");
  
  // Create directory line edit.
  directory_edit = new QLineEdit();
  directory_edit->setMaxLength(511);
  directory_edit->setReadOnly(true);
  
  // Create directory button.
  directory_button = new QPushButton("Select...", this);
  
  // Create box for line edit and button.
  directory_hbox = new QHBoxLayout();
  directory_hbox->setSpacing(16);
  directory_hbox->addWidget(directory_edit);
  directory_hbox->addWidget(directory_button);
  
  grid = new QGridLayout(this);
  grid->addWidget(preamble_label, 0, 0);
  grid->addWidget(choose_label, 1, 0);
  grid->addLayout(directory_hbox, 2, 0);
  grid->addWidget(buttons_box, 3, 0);
  
  // Connect actions to widgets.
  connect(directory_button, &QPushButton::pressed, this, &ChooseDialog::directory_button_pressed);
  
  connect(buttons_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  
  connect(this, &QDialog::accepted, this, &ChooseDialog::dialog_accepted);
  connect(this, &QDialog::accepted, this, &ChooseDialog::dialog_rejected);

  this->setFixedSize(this->sizeHint());
}

ChooseDialog::~ChooseDialog()
{
}

void ChooseDialog::directory_button_pressed()
{
  QFileDialog folderDialog;
  folderDialog.setWindowTitle("Choose Data Directory");
  folderDialog.setFileMode(QFileDialog::Directory);
  
  if (folderDialog.exec())
  {
    QStringList selection = folderDialog.selectedFiles();
    QString folderName = selection.at(0);
    
    directory_edit->setText(folderName);
  }
}

void ChooseDialog::dialog_accepted()
{
  QString selectedFolder = directory_edit->text();
  QByteArray ba = selectedFolder.toUtf8();
  
  char *ptr = ba.data();
  preferences_set_data_directory(ptr);
}

void ChooseDialog::dialog_rejected()
{
}

