#ifndef FILE_DIALOG_H
#define FILE_DIALOG_H

#include <QFileDialog>
#include <QList>

class FileDialog : public QFileDialog
{
    
public:
    FileDialog(QWidget *parent = nullptr);
    
    void addDefaultFilter(QString extension, QString narrative);
    void addDefaultFilter(QString extension, QString narrative, QString tag);
    void addFilter(QString extension, QString narrative);
    void addFilter(QString extension, QString narrative, QString tag);
    
    void setInitialDirectory(QString directoryName);
    void setInitialFileName(QString fileName);
    
    int exec() override;
    
    QFileInfo selection() const;
    
    QString selectedFileExtension() const;
    QString selectedTag() const;
    
protected:
    
    typedef struct
    {
        bool isDefault;
        QString extension;
        QString narrative;
        QString tag;
    } FilterEntry;
    
    void getSelectedFilterEntry(FilterEntry &entry) const;
    
private:
    
    void appendFilter(QString extension, QString narrative, QString tag, bool isDefault);
    void setSelection();
    
    QString defaultFilter;
    QList<FilterEntry> filterEntries;
    QStringList nameFilters;
    
    QString initialDirectory;
    QString initialFileName;

};

#endif
