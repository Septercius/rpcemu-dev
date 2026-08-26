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

#include <dirent.h>

#ifdef _MSC_VER
#define PATH_MAX 1024
#else
#include "unistd.h"
#endif

#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

#include <QButtonGroup>
#include <QFileDialog>
#include <QMessageBox>

#include "hostfs_dialog.h"
#include "paths.h"

HostFSDialog::HostFSDialog(Emulator &emulator, Config *config_copy, Model *model_copy, QWidget *parent) : QDialog(parent),
    emulator(emulator),
    config_copy(config_copy),
    model_copy(model_copy)
{
    setWindowTitle("Configure HostFS");
    
    // Create widgets and layout.
    grid = new QGridLayout(this);
    
    char title[20];
    
    for (int i = 0 ; i < HOSTFS_DRIVE_MAX ; i++)
    {
        driveLayout[i] = new QGridLayout();
        driveLayout[i]->setColumnMinimumWidth(1, 300);
        
        driveCheckBox[i] = new QCheckBox(tr("Enabled"));
        driveCheckBox[i]->setObjectName(QString::number(i));
        
        driveNameLabel[i] = new QLabel(tr("Name:"));
        driveNameLabel[i]->setMinimumWidth(100);
        driveNameLabel[i]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        
        driveNameEdit[i] = new QLineEdit();
        driveNameEdit[i]->setMaxLength(12);
        
        driveHostPathLabel[i] = new QLabel(tr("Location:"));
        driveHostPathLabel[i]->setMinimumWidth(100);
        driveHostPathLabel[i]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        
        driveHostPathEdit[i] = new QLineEdit();
        driveHostPathEdit[i]->setMaxLength(255);
        
        drivePathButton[i] = new QPushButton(tr("Select..."));
        
        sprintf(title, "Drive %d", i + 4);
        driveGroup[i] = new QGroupBox(tr(title));
        driveGroup[i]->setLayout(driveLayout[i]);
        
        driveLayout[i]->addWidget(driveCheckBox[i], 0, 0);
        driveLayout[i]->addWidget(driveNameLabel[i], 1, 0);
        driveLayout[i]->addWidget(driveNameEdit[i], 1, 1);
        driveLayout[i]->addWidget(driveHostPathLabel[i], 2, 0);
        driveLayout[i]->addWidget(driveHostPathEdit[i], 2, 1);
        driveLayout[i]->addWidget(drivePathButton[i], 2, 2);
        
        grid->addWidget(driveGroup[i], i, 0);
    }
    
    // Create check boxes.
    showDotFilesCheckBox = new QCheckBox("Show dot files");
    grid->addWidget(showDotFilesCheckBox, HOSTFS_DRIVE_MAX, 0);
    
    showSystemFilesCheckBox = new QCheckBox("Show system files");
    grid->addWidget(showSystemFilesCheckBox, HOSTFS_DRIVE_MAX + 1, 0);
    
    // Create buttons.
    buttonsBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    grid->addWidget(buttonsBox, HOSTFS_DRIVE_MAX + 2, 0);
    
    connect(buttonsBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonsBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    connect(this, &QDialog::accepted, this, &HostFSDialog::dialogAccepted);
    connect(this, &QDialog::rejected, this, &HostFSDialog::dialogRejected);
    
    for (int i = 0 ; i < HOSTFS_DRIVE_MAX ; i++)
    {
        connect(driveCheckBox[i], &QCheckBox::stateChanged, this, &HostFSDialog::checkBoxChanged);
        connect(drivePathButton[i], &QPushButton::clicked, this, &HostFSDialog::pathButtonPressed);
    }
    
    // Set the values in the dialog box.
    applyConfig();
    
    // Remove resize from dialog.
    this->setFixedSize(this->sizeHint());
}

HostFSDialog::~HostFSDialog()
{
}

/*
 * A key was pressed.
 */
void
HostFSDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
    {
        return;
    }
    
    QDialog::keyPressEvent(event);
}

void
HostFSDialog::done(int r)
{
    QString str;
    QByteArray ba;
    char path[PATH_MAX];
    char message[1024], *resolvedPath;
    struct stat info;
    
    if (r == QDialog::Accepted)
    {
        for (int i = 0 ; i < HOSTFS_DRIVE_MAX ; i++)
        {
            bool enabled = driveCheckBox[i]->isChecked();
            if (!enabled) continue;
        
            // Is there a name?
            str = driveNameEdit[i]->text();
            if (str.length() == 0)
            {
                sprintf(message, "You must enter a name for drive %d.", config_copy->hostfs_drive[i].id);
                QMessageBox::information(this, "HostFS", message);
                
                driveNameEdit[i]->setFocus();
                return;
            }
            
            // Is there a path?
            str = driveHostPathEdit[i]->text();
            if (str.length() == 0)
            {
                sprintf(message, "You must enter a path for drive %d.", config_copy->hostfs_drive[i].id);
                QMessageBox::information(this, "HostFS", message);
                
                driveHostPathEdit[i]->setFocus();
                return;
            }
            
            ba = str.toUtf8();
            strcpy(path, ba.data());
            
            resolvedPath = path_resolve(path);
            
            // Does the path exist?
            if (stat(resolvedPath, &info) != 0)
            {
                sprintf(message, "The path for drive %d does not exist or is inaccessible %s.", config_copy->hostfs_drive[i].id, resolvedPath);
                QMessageBox::information(this, "HostFS", message);
                
                driveHostPathEdit[i]->setFocus();
                return;
            }
            
            // Is it a directory?
            if ((info.st_mode & S_IFDIR) == 0)
            {
                sprintf(message, "The path for drive %d is not a directory.", config_copy->hostfs_drive[i].id);
                QMessageBox::information(this, "HostFS", message);
                
                driveHostPathEdit[i]->setFocus();
                return;
            }
        }
    }
    
    QDialog::done(r);
}

/**
 * The user clicked on "OK" in the dialog.
 */
void
HostFSDialog::dialogAccepted()
{
    QString str;
    QByteArray ba;
    
    Config new_config;
    memcpy(&new_config, config_copy, sizeof(Config));
    
    // Fill in the choices from the dialog box.
    for (int i = 0 ; i < HOSTFS_DRIVE_MAX ; i++)
    {
        HostFSDrive *drive = &new_config.hostfs_drive[i];
        
        bool enabled = driveCheckBox[i]->isChecked();
        
        drive->enabled = enabled;
        if (drive->enabled)
        {
            str = driveNameEdit[i]->text();
            ba = str.toUtf8();
            
            if (drive->driveName) free(drive->driveName);
            drive->driveName = strdup(ba.data());
            
            str = driveHostPathEdit[i]->text();
            ba = str.toUtf8();
            
            if (drive->hostPath) free(drive->hostPath);
            drive->hostPath = strdup(ba.data());
            
            drive->resolvedHostPath = path_resolve(drive->hostPath);
        }
    }
    
    new_config.show_dotfiles = showDotFilesCheckBox->isChecked();
    new_config.show_systemfiles = showSystemFilesCheckBox->isChecked();
    
    // Copy to the GUI copy.
    memcpy(config_copy, &new_config, sizeof(Config));
    
    // Create a new copy of the config, to be owned and freed by the emulator thread.
    Config *emu_config = (Config *) malloc(sizeof(Config));
    if (emu_config == NULL)
    {
        fatal("Out of memory creating config copy.");
    }
    
    memcpy(emu_config, &new_config, sizeof(Config));
    
    // Inform the emulator thread of the new choices.
    emit this->emulator.hostfs_updated_signal(emu_config);
}

/**
 * The user clicked on "Cancel" in the dialog.
 */
void
HostFSDialog::dialogRejected()
{
    applyConfig();
}

void HostFSDialog::applyConfig()
{
    HostFSDrive *drive = NULL;
    
    for (int i = 0 ; i < HOSTFS_DRIVE_MAX ; i++)
    {
        drive = &config_copy->hostfs_drive[i];
        
        driveCheckBox[i]->setChecked(drive->enabled);
        driveNameEdit[i]->setText(drive->driveName);
        driveHostPathEdit[i]->setText(drive->hostPath);
    }
    
    showDotFilesCheckBox->setChecked(config_copy->show_dotfiles);
    showSystemFilesCheckBox->setChecked(config_copy->show_systemfiles);
    
    applyShading(-1);
}

/**
 * Sets the shading for widgets of a drive.
 *
 * @param drive The index of the drive.
 */
void
HostFSDialog::applyShading(int drive)
{
    bool enabled;
    
    for (int i = 0 ; i < HOSTFS_DRIVE_MAX ; i++)
    {
        enabled = driveCheckBox[i]->isChecked();
        
        if (drive == i || drive == -1)
        {
            driveNameLabel[i]->setEnabled(enabled);
            driveNameEdit[i]->setEnabled(enabled);
            driveHostPathLabel[i]->setEnabled(enabled);
            driveHostPathEdit[i]->setEnabled(enabled);
            drivePathButton[i]->setEnabled(enabled);
        }
    }
}

/**
 * The state of a check box has changed.
 */
void
HostFSDialog::checkBoxChanged(int state)
{
    Q_UNUSED(state);
    
    int index = sender()->objectName().toInt();
    applyShading(index);
}

/**
 * The user has clicked on a button to choose a path.
 */
void
HostFSDialog::pathButtonPressed()
{
    int index = sender()->objectName().toInt();
    
    QString str = driveHostPathEdit[index]->text();
    QDir dir(str);
    
    QFileDialog dialog(this);
    dialog.setFileMode(QFileDialog::Directory);
    
    if (str.length() > 0 && dir.exists())
    {
        dialog.setDirectory(dir);
    }
    
    dialog.setWindowTitle(tr("Select host path"));
    if (dialog.exec())
    {
        QStringList selection = dialog.selectedFiles();

        driveHostPathEdit[index]->setText(selection.at(0));
    }
}
