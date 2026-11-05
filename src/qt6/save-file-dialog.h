#ifndef SAVE_FILE_DIALOG_H
#define SAVE_FILE_DIALOG_H

#include <QFileDialog>
#include "file-dialog.h"

class SaveFileDialog : public FileDialog
{
    
public:
    SaveFileDialog(QWidget *parent = nullptr);

};

#endif
