#ifndef FOLDER_EDIT_H
#define FOLDER_EDIT_H

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QLineEdit>
#include <QMimeData>

class FolderEdit : public QLineEdit
{
    
    Q_OBJECT
    
public:
    FolderEdit(QWidget *parent = nullptr);
    
protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    
private:
    bool getFolderPath(const QMimeData *mimeData, char *folderPath);
    
};

#endif
