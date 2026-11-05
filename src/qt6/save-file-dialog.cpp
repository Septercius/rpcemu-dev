#include <QDir>
#include <QFileDialog>

#include "save-file-dialog.h"

SaveFileDialog::SaveFileDialog(QWidget *parent) : FileDialog(parent)
{
    this->setAcceptMode(QFileDialog::AcceptSave);
}
