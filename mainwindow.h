#ifndef __mainwindow_h__
#define __mainwindow_h__

#include "tsparser.h"
#include <QDialog>

///////////////////////////////////////////////////////////////
QT_BEGIN_NAMESPACE
class QPushButton;
class QProgressBar;
class QTreeView;
class QMutex;
QT_END_NAMESPACE

class InfoTreeModel;
///////////////////////////////////////////////////////////////
class MainWindow : public QDialog
{
    Q_OBJECT

public:
    MainWindow();

private slots:
    void onOpenFiles();
    void onStreamFound(const STREAM_INFO& streamInfo);
    void onNotifyError(const QString& info);
    void onNotifyStart(qint32 threadId, const TsParser& parser);
    void onNotifyDone(qint32 percent, qint32 threadId);

private:
    void setup();

private:
    QPushButton* btnOpenFile_;
    InfoTreeModel* treeModel_;
    QTreeView* treeView_;
    QProgressBar* progress_;

    typedef QMap<qint32,TsParser*> ParsersMapT;
    ParsersMapT parsersMap_;
    QSharedPointer<QMutex> guardParsers_;
};

#endif // __mainwindow_h__