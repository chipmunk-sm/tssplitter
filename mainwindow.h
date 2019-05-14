
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "tsparser.h"
#include <QWidget>

class QPushButton;
class QProgressBar;
class QTreeView;
class QMutex;
class InfoTreeModel;
class QTextEdit;

class MainWindow : public QWidget
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    const QString m_supportedExt = "*.ts *.tp *.m2ts *.mts";

private slots:
    void onOpenFiles();
    void onStreamFound(const STREAM_INFO& streamInfo, TsParser *self);
    void onNotifyError(const QString& info);
    void onNotifyStart(Qt::HANDLE threadId, TsParser *parser);
    void onNotifyDone(int32_t percent, Qt::HANDLE threadId);

private:
    void OpenFiles(const QStringList & tsFiles);

    QTextEdit* m_error = nullptr;
    QTreeView*    m_treeView = nullptr;
    QProgressBar* m_progress = nullptr;
    QPushButton*  m_buttonOpenFile = nullptr;

    std::map<Qt::HANDLE, TsParser*> m_parsersMap;
    QSharedPointer<QMutex> m_lock;


protected:
    virtual void dropEvent(QDropEvent *event) override;
    virtual void dragEnterEvent(QDragEnterEvent *event) override;
};

#endif // MAINWINDOW_H
