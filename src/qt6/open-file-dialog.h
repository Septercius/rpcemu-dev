#ifndef OPEN_FILE_DIALOG_H
#define OPEN_FILE_DIALOG_H

#include <QFileDialog>
#include <QList>

#include "file-dialog.h"

class OpenFileDialog : public FileDialog
{
    
public:
    OpenFileDialog(QWidget *parent = nullptr);
    
};

#endif

