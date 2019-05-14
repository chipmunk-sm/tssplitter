#include "resource.h"
#include "infotreemodel.h"
#include <QtWidgets>

////////////////////////////////////////////
InfoTreeModel::InfoTreeModel(QWidget* parent) 
    : QAbstractItemModel(parent)
{}

void InfoTreeModel::addSourceTs(const QString& source)
{
    ItemInfo info = {QFileInfo(source).baseName(), StreamsMapT()};
    items_.push_back(info);
    int row = items_.size()-1;
    beginInsertRows(index(row,0), row, row);
    endInsertRows();
    view_->setItemsExpandable(false);
    view_->setIndentation(0);
    view_->doItemsLayout();
}

bool InfoTreeModel::isAdded(const QString& source)
{
    QVector<ItemInfo>::const_iterator It = items_.begin();
    for(; It != items_.end(); ++It) 
        if( QString::compare(It->sourceName, QFileInfo(source).baseName(), Qt::CaseInsensitive) == 0 )
            break;
    return (It != items_.end());
}

void InfoTreeModel::addStreamInfo(const STREAM_INFO& info)
{
/*    ItemInfo& nfo = items_.back();
    quint16 pid = info.pid;
    while( nfo.streamsInfos_.end() != nfo.streamsInfos_.find(pid) )
        pid++;

    nfo.streamsInfos_.insert(pid, info);
    view_->setItemsExpandable(false);
    view_->setIndentation(2);
    view_->doItemsLayout();
    */
}

void InfoTreeModel::clear()
{
    items_.clear();
    view_->update();
}

QModelIndex InfoTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    return createIndex(row,column);
}

QModelIndex InfoTreeModel::parent(const QModelIndex& child) const
{
    return QModelIndex();
}

int InfoTreeModel::rowCount(const QModelIndex& parent) const
{
    return items_.size();
}

int InfoTreeModel::columnCount(const QModelIndex& parent) const
{
    return 1;
}

QVariant InfoTreeModel::data(const QModelIndex& index, int role) const
{
    qint16 r = index.row();
    if( r >= items_.size() )
        return QVariant();

    if( role == Qt::DisplayRole ) 
    {
        return items_.at(r).sourceName;
    }
    else if( role == Qt::DecorationRole ) {
        return QIcon(*Resources::pxMpegtsFile);
    }
    return QVariant();
}

/*
bool InfoTreeModel::hasChildren(const QModelIndex& parent) const
{
    if( parent.column() == 2 )
        return false;

    qint16 r = parent.row();
    if( r == -1 )
        return (items_.empty() ? false : true);
    return (r < items_.size() ? !items_.at(r).streamsInfos_.empty() : false);
}
*/
