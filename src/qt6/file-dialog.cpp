#include <QDir>
#include <QFileDialog>

#include "file-dialog.h"

FileDialog::FileDialog(QWidget *parent) : QFileDialog(parent)
{
    this->setFileMode(QFileDialog::AnyFile);
}

void FileDialog::appendFilter(QString extension, QString narrative, QString tag, bool isDefault)
{
    FilterEntry entry;
    entry.extension = extension;
    entry.narrative = narrative;
    entry.isDefault = isDefault;
    entry.tag = tag;
    
    filterEntries.append(entry);
    
    QString text = entry.narrative + " (*." + entry.extension + ")";
    nameFilters.append(text);
    
    if (isDefault)
    {
        this->defaultFilter = text;
    }
}

void FileDialog::addDefaultFilter(QString extension, QString narrative)
{
    this->appendFilter(extension, narrative, nullptr, true);
}

void FileDialog::addDefaultFilter(QString extension, QString narrative, QString tag)
{
    this->appendFilter(extension, narrative, tag, true);
}

void FileDialog::addFilter(QString extension, QString narrative)
{
    this->appendFilter(extension, narrative, nullptr, false);
}

void FileDialog::addFilter(QString extension, QString narrative, QString tag)
{
    this->appendFilter(extension, narrative, tag, false);
}

int FileDialog::exec()
{
    if (nameFilters.count() > 0)
    {
        this->setNameFilters(nameFilters);
        
        if (defaultFilter.length() > 0)
        {
            this->selectNameFilter(this->defaultFilter);
        }
    }
    
    this->setSelection();
    
    return QFileDialog::exec();
}

void FileDialog::getSelectedFilterEntry(FilterEntry &entry) const
{
    QString filterText = this->selectedNameFilter();
    
    qsizetype index = this->nameFilters.indexOf(filterText);
    entry = this->filterEntries.at(index);
}

QString FileDialog::selectedFileExtension() const
{
    FilterEntry filterEntry;
    this->getSelectedFilterEntry(filterEntry);
    
    return filterEntry.extension;
}

QString FileDialog::selectedTag() const
{
    FilterEntry filterEntry;
    this->getSelectedFilterEntry(filterEntry);
    
    return filterEntry.tag;
}

QFileInfo FileDialog::selection() const
{
    QString selectedPath = this->selectedFiles().at(0);
    QFileInfo info(selectedPath);
    
    return info;
}

void FileDialog::setInitialDirectory(QString directory)
{
    this->initialDirectory = directory;
}

void FileDialog::setInitialFileName(QString fileName)
{
    this->initialFileName = fileName;
}

void FileDialog::setSelection()
{
    QDir home = QDir::home();
    
    if (this->initialDirectory.length() > 0 && this->initialFileName.length() > 0)
    {
        QDir dir(initialDirectory);
        if (dir.exists())
        {
            QString fullPath = dir.absoluteFilePath(initialFileName);
            
            this->setDirectory(fullPath);
        }
        else
        {
            QString fullPath = home.absoluteFilePath(this->initialFileName);
            this->setDirectory(fullPath);
        }
        return;
    }
    else if (this->initialDirectory.length() > 0)
    {
        QDir dir(initialDirectory);
        if (dir.exists())
        {
            this->setDirectory(this->initialDirectory);
            return;
        }
    }
    
    QString absoluteHome = home.absolutePath();
    this->setDirectory(absoluteHome);
}
