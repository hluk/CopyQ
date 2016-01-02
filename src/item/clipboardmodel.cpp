/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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

#include "common/contenttype.h"
#include "common/mimetypes.h"

#include <QStringList>

namespace {

QList<QPersistentModelIndex> validIndeces(const QModelIndexList &indexList)
{
    QList<QPersistentModelIndex> list;
    list.reserve(indexList.size());

    foreach (const QModelIndex &index, indexList) {
        if ( index.isValid() )
            list.append(index);
    }

    return list;
}

int topMostRow(const QList<QPersistentModelIndex> &indexList)
{
    int row = indexList.value(0).row();

    foreach (const QPersistentModelIndex &index, indexList)
        row = qMin(row, index.row());

    return row;
}

} // namespace

ClipboardModel::ClipboardModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_max(100)
    , m_clipboardList(m_max)
    , m_disabled(false)
    , m_tabName()
{
}

int ClipboardModel::rowCount(const QModelIndex&) const
{
    return m_clipboardList.size();
}

QVariant ClipboardModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_clipboardList.size())
        return QVariant();

    return m_clipboardList[index.row()].data(role);
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
        m_clipboardList[row].setText(value.toString());
    } else if (role == contentType::notes) {
        const QString notes = value.toString();
        if ( notes.isEmpty() )
            m_clipboardList[row].removeData(mimeItemNotes);
        else
            m_clipboardList[row].setData( mimeItemNotes, notes.toUtf8() );
    } else if (role == contentType::updateData) {
        if ( !m_clipboardList[row].updateData(value.toMap()) )
            return false;
    } else if (role == contentType::data) {
        ClipboardItem &item = m_clipboardList[row];
        const QVariantMap dataMap = value.toMap();
        if ( !item.setData(dataMap) )
            return false;
    } else if (role >= contentType::removeFormats) {
        if ( !m_clipboardList[row].removeData(value.toStringList()) )
            return false;
    } else {
        return false;
    }

    emit dataChanged(index, index);

    return true;
}

void ClipboardModel::insertItem(const QVariantMap &data, int row)
{
    ClipboardItem item;
    item.setData(data);

    beginInsertRows(QModelIndex(), row, row);

    m_clipboardList.insert(row, item);

    endInsertRows();
}

bool ClipboardModel::insertRows(int position, int rows, const QModelIndex&)
{
    if ( rows <= 0 || position < 0 )
        return false;

    beginInsertRows(QModelIndex(), position, position + rows - 1);

    for (int row = 0; row < rows; ++row)
        m_clipboardList.insert(position, ClipboardItem());

    endInsertRows();

    return true;
}

bool ClipboardModel::removeRows(int position, int rows, const QModelIndex&)
{
    if ( rows <= 0 || position < 0 || position + rows > rowCount() )
        return false;

    int last = qMin( position + rows, rowCount() ) - 1;

    beginRemoveRows(QModelIndex(), position, last);

    m_clipboardList.remove(position, last - position + 1);

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

    if (row < 0)
        return cycle ? n - 1 : 0;

    return row;
}

void ClipboardModel::unloadItems()
{
    emit unloaded();
    removeRows(0, rowCount());
}

void ClipboardModel::setMaxItems(int max)
{
    m_max = qMax(0, max);

    if ( m_max < m_clipboardList.size() ) {
        beginRemoveRows(QModelIndex(), m_max + 1, m_clipboardList.size() - 1);
        m_clipboardList.resize(m_max);
        endRemoveRows();
    } else {
        m_clipboardList.reserve(m_max);
    }
}

void ClipboardModel::setTabName(const QString &tabName)
{
    Q_ASSERT( !tabName.isEmpty() );

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

    const int sourceRow = from;
    const int targetRow = to;
    if (from < to)
        ++to;

    if ( !beginMoveRows(QModelIndex(), from, from, QModelIndex(), to) )
        return false;

    m_clipboardList.move(sourceRow, targetRow);

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

void ClipboardModel::sortItems(const QModelIndexList &indexList, CompareItems *compare)
{
    QList<QPersistentModelIndex> list = validIndeces(indexList);
    qSort( list.begin(), list.end(), compare );

    int targetRow = topMostRow(list);

    foreach (const QPersistentModelIndex &ind, list) {
        if (ind.isValid()) {
            const int sourceRow = ind.row();

            if (targetRow != sourceRow) {
                beginMoveRows(QModelIndex(), sourceRow, sourceRow, QModelIndex(), targetRow);
                m_clipboardList.move(sourceRow, targetRow);
                endMoveRows();

                // If the moved item was removed or moved further (as reaction on moving the item),
                // stop sorting.
                if (!ind.isValid() || ind.row() != targetRow)
                    break;
            }

            ++targetRow;
        }
    }
}

int ClipboardModel::findItem(uint item_hash) const
{
    for (int i = 0; i < m_clipboardList.size(); ++i) {
        if ( m_clipboardList[i].dataHash() == item_hash )
            return i;
    }

    return -1;
}
