/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "clipboardmodel.h"

#include "common/common.h"
#include "common/contenttype.h"
#include "item/serialize.h"

#include <QDataStream>
#include <QStringList>

ClipboardModel::ClipboardModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_clipboardList()
    , m_max(100)
    , m_disabled(false)
    , m_tabName()
{
}

ClipboardModel::~ClipboardModel()
{
}

int ClipboardModel::rowCount(const QModelIndex&) const
{
    return m_clipboardList.size();
}

QVariantMap ClipboardModel::dataMapInRow(int row) const
{
    return (row >= 0 && row < rowCount()) ? m_clipboardList[row]->data() : QVariantMap();
}

ClipboardItemPtr ClipboardModel::at(int row) const
{
    return m_clipboardList[row];
}

QVariant ClipboardModel::data(int row) const
{
    return data( index(row,0), Qt::EditRole );
}

QVariant ClipboardModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_clipboardList.size())
        return QVariant();

    return m_clipboardList[index.row()]->data(role);
}

Qt::ItemFlags ClipboardModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::ItemIsEnabled;

    return QAbstractItemModel::flags(index) | Qt::ItemIsEditable;
}

bool ClipboardModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if ( !index.isValid() )
        return false;

    int row = index.row();

    if (role == Qt::EditRole) {
        m_clipboardList[row]->setText(value.toString());
    } else if (role == contentType::notes) {
        const QString notes = value.toString();
        if ( notes.isEmpty() )
            m_clipboardList[row]->removeData(mimeItemNotes);
        else
            m_clipboardList[row]->setData( mimeItemNotes, notes.toUtf8() );
    } else if (role == contentType::updateData) {
        if ( !m_clipboardList[row]->updateData(value.toMap()) )
            return false;
    } else if (role == contentType::data) {
        const ClipboardItemPtr &item = m_clipboardList[row];
        const QVariantMap dataMap = value.toMap();
        if ( item->data() == dataMap )
            return false;
        item->setData(dataMap);
    } else if (role >= contentType::removeFormats) {
        if ( !m_clipboardList[row]->removeData(value.toStringList()) )
            return false;
    } else {
        return false;
    }

    emit dataChanged(index, index);

    return true;
}

bool ClipboardModel::setDataMap(const QModelIndex &index, const QVariantMap &value)
{
    if ( !index.isValid() )
        return false;

    m_clipboardList[index.row()]->setData(value);

    emit dataChanged(index, index);

    return true;
}

void ClipboardModel::insertItem(const QVariantMap &data, int row)
{
    ClipboardItemPtr item( new ClipboardItem() );
    item->setData(data);

    beginInsertRows(QModelIndex(), row, row);

    m_clipboardList.insert(row, item);

    endInsertRows();
}

bool ClipboardModel::insertRows(int position, int rows, const QModelIndex&)
{
    beginInsertRows(QModelIndex(), position, position + rows - 1);

    for (int row = 0; row < rows; ++row)
        m_clipboardList.insert(position, ClipboardItemPtr(new ClipboardItem()));

    endInsertRows();

    return true;
}

bool ClipboardModel::removeRows(int position, int rows, const QModelIndex&)
{
    if ( position < 0 || position >= rowCount() )
        return false;

    int last = qMin( position + rows, rowCount() ) - 1;

    beginRemoveRows(QModelIndex(), position, last);

    m_clipboardList.erase( m_clipboardList.begin() + position,
                           m_clipboardList.begin() + last + 1 );

    endRemoveRows();

    return true;
}

int ClipboardModel::getRowNumber(int row, bool cycle) const
{
    int n = rowCount();
    if (n == 0)
        return -1;

    if (row >= n)
        return cycle ? 0 : n - 1;
    else if (row < 0)
        return cycle ? n - 1 : 0;
    else
        return row;
}

void ClipboardModel::unloadItems()
{
    emit unloaded();
    removeRows(0, rowCount());
}

void ClipboardModel::setMaxItems(int max)
{
    m_max = max>0 ? max : 0;
    int rows = m_clipboardList.length();

    if (max >= rows)
        return;

    beginRemoveRows(QModelIndex(), max + 1, rows - 1);

    m_clipboardList.erase( m_clipboardList.begin() + max,
                           m_clipboardList.end() );

    endRemoveRows();
}

void ClipboardModel::setTabName(const QString &tabName)
{
    if (m_tabName == tabName)
        return;

    m_tabName = tabName;
    emit tabNameChanged(m_tabName);
}

bool ClipboardModel::move(int pos, int newpos)
{
    int from = getRowNumber(pos,true);
    int to   = getRowNumber(newpos,true);

    if( from == -1 || to == -1 )
        return false;

    if ( !beginMoveRows(QModelIndex(), from, from, QModelIndex(), from < to ? to + 1 : to) )
        return false;

    m_clipboardList.move(from, to);

    endMoveRows();

    return true;
}

bool ClipboardModel::moveItemsWithKeyboard(QModelIndexList indexList, int key, int count) {
    int from, to;
    bool res = false;

    QList<int> list;
    for ( int i = 0; i < indexList.length(); ++i )
        list.append( indexList.at(i).row() );

    if ( key == Qt::Key_Down || key == Qt::Key_End )
        qSort( list.begin(), list.end(), qGreater<int>() );
    else
        qSort( list.begin(), list.end(), qLess<int>() );

    for ( int i = 0, d = 0; i<list.length(); ++i ) {
        from = list.at(i) + d;

        switch (key) {
        case Qt::Key_Down:
            to = from + count;
            break;
        case Qt::Key_Up:
            to = from - count;
            break;
        case Qt::Key_End:
            to = rowCount()-i-1;
            break;
        default:
            to = 0+i;
            break;
        }

        if ( to < 0 )
            --d;
        else if (to >= rowCount() )
            ++d;

        if ( !move(from, to) )
            return false;
        if (!res)
            res = to==0 || from==0 || to == rowCount();
    }

    return res;
}

void ClipboardModel::sortItems(const QModelIndexList &indexList,
                               CompareItems *compare)
{
    QList< QPair<int, ClipboardItemPtr> > list;
    QList<int> rows;

    for (int i = 0; i < indexList.length(); ++i) {
        int row = indexList[i].row();
        if ( row >= m_clipboardList.length() )
            return;
        list.append( qMakePair(row, m_clipboardList[row]) );
        rows.append(row);
    }

    qSort(rows);
    qSort( list.begin(), list.end(), compare );

    for (int i = 0; i < list.length(); ++i ) {
        int row1 = list[i].first;
        int row2 = rows[i];
        if (row1 != row2) {
            m_clipboardList[row2] = list[i].second;
            QModelIndex ind = index(row2);
            emit dataChanged(ind, ind);
        }
    }
}

int ClipboardModel::findItem(uint item_hash) const
{
    for (int i = 0; i < m_clipboardList.length(); ++i) {
        if ( m_clipboardList[i]->dataHash() == item_hash )
            return i;
    }

    return -1;
}
