#ifndef __infotreemodel_h__
#define __infotreemodel_h__

#include "tsstream.h"
#include <QAbstractItemModel>
#include <QVector>
#include <QMap>

///////////////////////////////////////////////////////////////
QT_BEGIN_NAMESPACE
class QTreeView;
QT_END_NAMESPACE

///////////////////////////////////////////////////////////////
class InfoTreeModel : public QAbstractItemModel
{
public:
    InfoTreeModel(QWidget* parent);

    void addSourceTs(const QString& source);
    void addStreamInfo(const STREAM_INFO& info);
    void clear();
    void setView(QTreeView* view) { view_ = view; }
    bool isAdded(const QString& source);

protected:
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex& child) const;
    int rowCount(const QModelIndex&  parent = QModelIndex()) const;
    int columnCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role) const;
    //bool hasChildren(const QModelIndex& parent = QModelIndex()) const;

private:
    typedef QMap<quint16,STREAM_INFO> StreamsMapT;
    typedef struct _ItemInfoT {
        QString     sourceName;
        StreamsMapT streamsInfos_;
    } ItemInfo;

    QVector<ItemInfo> items_;
    QTreeView* view_;
};

#endif // __infotreemodel_h__