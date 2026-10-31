#include <QFileInfo>
#include <QMimeData>
#include <QStringList>
#include <QUrl>

#include "folder-edit.h"

FolderEdit::FolderEdit(QWidget *parent) : QLineEdit(parent)
{
    this->setAcceptDrops(true);
}

void FolderEdit::dragEnterEvent(QDragEnterEvent *event)
{
    char folderPath[PATH_MAX];
    
    if (!this->getFolderPath(event->mimeData(), folderPath))
    {
        event->ignore();
    }
    else
    {
        event->acceptProposedAction();
    }
}

void FolderEdit::dropEvent(QDropEvent *event)
{
    char folderPath[PATH_MAX];
    
    if (!this->getFolderPath(event->mimeData(), folderPath))
    {
        event->ignore();
    }
    else
    {
        event->acceptProposedAction();
        this->setText(folderPath);
    }
}

bool FolderEdit::getFolderPath(const QMimeData *mimeData, char *folderPath)
{
    QList<QUrl> urls = mimeData->urls();
    if (urls.count() != 1)
    {
        folderPath = nullptr;
        return false;
    }
    
    QString localPath = urls.at(0).toLocalFile();
    QFileInfo info = QFileInfo(localPath);
    
    if (!info.isDir())
    {
        folderPath = nullptr;
        return false;
    }
    
    QByteArray ba = localPath.toUtf8();
    char *data = ba.data();
    
    strncpy(folderPath, data, PATH_MAX);
    
    return true;
}
