#include <QDir>
#include <QFileDialog>

#include "open-file-dialog.h"

OpenFileDialog::OpenFileDialog(QWidget *parent) : FileDialog(parent)
{
    this->setAcceptMode(QFileDialog::AcceptOpen);
}

