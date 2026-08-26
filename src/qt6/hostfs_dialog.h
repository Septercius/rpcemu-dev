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

#ifndef HOSTFS_DIALOG_H
#define HOSTFS_DIALOG_H

#include <vector>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include "rpc-qt6.h"
#include "rpcemu.h"

class HostFSDialog : public QDialog
{
    
    Q_OBJECT
    
public:
    HostFSDialog(Emulator &emulator, Config *config_copy, Model *model_copy, QWidget *parent = 0);
    virtual ~HostFSDialog();
    
    void keyPressEvent(QKeyEvent *) Q_DECL_OVERRIDE;
    void done(int) Q_DECL_OVERRIDE;
    
private slots:
    
    void dialogAccepted();
    void dialogRejected();
    
    void checkBoxChanged(int);
    void pathButtonPressed();
    
private:
    
    void applyConfig();
    void applyShading(int);
    
    QCheckBox *driveCheckBox[HOSTFS_DRIVE_MAX];
    QGridLayout *driveLayout[HOSTFS_DRIVE_MAX];
    QGroupBox *driveGroup[HOSTFS_DRIVE_MAX];
    QLabel *driveNameLabel[HOSTFS_DRIVE_MAX];
    QLineEdit *driveNameEdit[HOSTFS_DRIVE_MAX];
    QLabel *driveHostPathLabel[HOSTFS_DRIVE_MAX];
    QLineEdit *driveHostPathEdit[HOSTFS_DRIVE_MAX];
    QPushButton *drivePathButton[HOSTFS_DRIVE_MAX];
    
    QCheckBox *showDotFilesCheckBox;
    QCheckBox *showSystemFilesCheckBox;
    
    QDialogButtonBox *buttonsBox;
    QGridLayout *grid;

    Emulator &emulator;
    
    // Pointers to GUI thread copies of the emulator configuration.
    Config *config_copy;
    Model *model_copy;
    
};

#endif
