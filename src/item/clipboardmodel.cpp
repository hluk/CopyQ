// SPDX-License-Identifier: GPL-3.0-or-later

#include "clipboardmodel.h"

#include "common/contenttype.h"
#include "common/mimetypes.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

#include <algorithm>

namespace {

QList<QPersistentModelIndex> validIndeces(const QModelIndexList &indexList)
{
    QList<QPersistentModelIndex> list;
    list.reserve(indexList.size());

    for (const auto &index : indexList) {
        if ( index.isValid() )
            list.append(index);
    }

    return list;
}

int topMostRow(const QList<QPersistentModelIndex> &indexList)
{
    int row = indexList.value(0).row();

    for (const auto &index : indexList)
        row = qMin(row, index.row());

    return row;
}

} // namespace

void ClipboardItemList::move(int from, int count, int to)
{
    if (to < from) {
        std::swap(from, to);
        to += count;
        count = to - from - count;
    }

    const auto start1 = std::begin(m_items) + from;
    const auto start2 = start1 + count;
    const auto end2 = std::begin(m_items) + to;
    std::rotate(start1, start2, end2);
}

ClipboardModel::ClipboardModel(QSqlDatabase db, QObject *parent)
    : QSqlTableModel(parent, db)
{
    setTable("items");
    setEditStrategy(QSqlTableModel::OnManualSubmit);  // Delayed commits

    // Setup delayed submit timer
    m_submitTimer.setSingleShot(true);
    m_submitTimer.setInterval(1000);  // 1 second delay
    connect(&m_submitTimer, &QTimer::timeout, this, [this]() {
        if (!submitAll()) {
            qWarning() << "Failed to submit changes:" << lastError().text();
        }
    });
}

void ClipboardModel::setTab(int tabId, const QString &tabName)
{
    m_tabId = tabId;
    m_tabName = tabName;

    // Filter to show only this tab's items
    setFilter(QString("tab_id = %1").arg(tabId));
    setSort(2, Qt::AscendingOrder);  // Column 2 = row_index
    select();  // Load from database
}

int ClipboardModel::rowCount(const QModelIndex &parent) const
{
    // If we haven't called setTab yet, use the old list
    if (m_tabId < 0)
        return m_clipboardList.size();

    // Otherwise delegate to QSqlTableModel
    return QSqlTableModel::rowCount(parent);
}

QVariant ClipboardModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    // If we haven't called setTab yet, use the old list
    if (m_tabId < 0) {
        if (index.row() >= m_clipboardList.size())
            return QVariant();
        return m_clipboardList[index.row()].data(role);
    }

    // For standard Qt roles, delegate to base class (reads from items table)
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        return QSqlTableModel::data(index, role);
    }

    // Get item_id from the items table (column 0)
    const int itemId = QSqlTableModel::data(this->index(index.row(), 0), Qt::DisplayRole).toInt();

    if (itemId == 0)
        return QVariant();

    // For contentType::data role, return full QVariantMap of MIME data
    if (role == contentType::data) {
        // Check cache first
        if (m_mimeDataCache.contains(itemId))
            return m_mimeDataCache[itemId];

        // Load from item_data table
        QVariantMap mimeData;
        QSqlQuery query(database());
        query.prepare("SELECT format, bytes FROM item_data WHERE item_id = ?");
        query.addBindValue(itemId);

        if (query.exec()) {
            while (query.next()) {
                const QString format = query.value(0).toString();
                const QByteArray bytes = query.value(1).toByteArray();
                mimeData[format] = bytes;
            }
        }

        // Cache for performance
        m_mimeDataCache[itemId] = mimeData;
        return mimeData;
    }

    // For other content roles, get the specific MIME type from the data
    const QVariantMap mimeData = data(index, contentType::data).toMap();

    // Map role to MIME type
    if (role == contentType::text)
        return mimeData.value(mimeText);
    if (role == contentType::html)
        return mimeData.value(mimeHtml);
    if (role == contentType::notes)
        return mimeData.value(mimeItemNotes);

    return QVariant();
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

    // Legacy mode - use old list
    if (m_tabId < 0) {
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
            // Emit dataChanged() only if really changed.
            if ( !item.setData(dataMap) )
                return true;
        } else if (role >= contentType::removeFormats) {
            if ( !m_clipboardList[row].removeData(value.toStringList()) )
                return false;
        } else {
            return false;
        }

        emit dataChanged(index, index);
        return true;
    }

    // Database mode - write to item_data table
    const int itemId = QSqlTableModel::data(this->index(row, 0), Qt::DisplayRole).toInt();
    if (itemId == 0)
        return false;

    if (role == contentType::data) {
        const QVariantMap mimeData = value.toMap();

        // Delete old MIME data
        QSqlQuery deleteQuery(database());
        deleteQuery.prepare("DELETE FROM item_data WHERE item_id = ?");
        deleteQuery.addBindValue(itemId);
        deleteQuery.exec();

        // Insert new MIME data
        QSqlQuery insertQuery(database());
        insertQuery.prepare("INSERT INTO item_data (item_id, format, bytes) VALUES (?, ?, ?)");

        for (auto it = mimeData.constBegin(); it != mimeData.constEnd(); ++it) {
            insertQuery.addBindValue(itemId);
            insertQuery.addBindValue(it.key());  // MIME format
            insertQuery.addBindValue(it.value().toByteArray());  // bytes
            if (!insertQuery.exec()) {
                qWarning() << "Failed to insert item_data:" << insertQuery.lastError().text();
                return false;
            }
        }

        // Update cache
        m_mimeDataCache[itemId] = mimeData;

        // Trigger delayed commit
        m_submitTimer.start();

        emit dataChanged(index, index);
        return true;
    } else if (role == contentType::notes) {
        // Update just the notes MIME type
        QVariantMap currentData = data(index, contentType::data).toMap();
        const QString notes = value.toString();
        if (notes.isEmpty())
            currentData.remove(mimeItemNotes);
        else
            currentData[mimeItemNotes] = notes.toUtf8();
        return setData(index, currentData, contentType::data);
    } else if (role == contentType::updateData) {
        // Merge with existing data
        QVariantMap currentData = data(index, contentType::data).toMap();
        const QVariantMap updates = value.toMap();
        for (auto it = updates.constBegin(); it != updates.constEnd(); ++it) {
            currentData[it.key()] = it.value();
        }
        return setData(index, currentData, contentType::data);
    } else if (role >= contentType::removeFormats) {
        // Remove specific formats
        QVariantMap currentData = data(index, contentType::data).toMap();
        const QStringList formats = value.toStringList();
        for (const QString &format : formats) {
            currentData.remove(format);
        }
        return setData(index, currentData, contentType::data);
    } else if (role == Qt::EditRole) {
        // Update text/plain MIME type
        QVariantMap currentData = data(index, contentType::data).toMap();
        currentData[mimeText] = value.toString().toUtf8();
        return setData(index, currentData, contentType::data);
    }

    // For other roles, delegate to base class
    bool result = QSqlTableModel::setData(index, value, role);
    if (result) {
        m_submitTimer.start();  // Delayed commit
    }
    return result;
}

void ClipboardModel::insertItem(const QVariantMap &data, int row)
{
    // Legacy mode - use old list
    if (m_tabId < 0) {
        ClipboardItem item;
        item.setData(data);

        beginInsertRows(QModelIndex(), row, row);

        m_clipboardList.insert(row, item);

        endInsertRows();
        return;
    }

    // Database mode - insert row then set data
    if (insertRows(row, 1)) {
        setData(index(row, 0), data, contentType::data);
    }
}

void ClipboardModel::insertItems(const QVector<QVariantMap> &dataList, int row)
{
    if ( dataList.isEmpty() )
        return;

    // Legacy mode - use old list
    if (m_tabId < 0) {
        int targetRow = row;
        m_clipboardList.reserve( m_clipboardList.size() + dataList.size() );

        beginInsertRows(QModelIndex(), row, row + dataList.size() - 1);

        for ( auto it = std::begin(dataList); it != std::end(dataList); ++it ) {
            m_clipboardList.insert(targetRow, ClipboardItem(*it));
            ++targetRow;
        }

        endInsertRows();
        return;
    }

    // Database mode - insert rows then set data
    if (insertRows(row, dataList.size())) {
        for (int i = 0; i < dataList.size(); ++i) {
            setData(index(row + i, 0), dataList[i], contentType::data);
        }
    }
}

void ClipboardModel::setItemsData(const QMap<QPersistentModelIndex, QVariantMap> &itemsData)
{
    QPersistentModelIndex topIndex;
    QPersistentModelIndex bottomIndex;

    for (auto it = std::begin(itemsData); it != std::end(itemsData); ++it) {
        const QPersistentModelIndex &index = it.key();
        if ( !index.isValid() )
            continue;

        const int row = index.row();
        ClipboardItem &item = m_clipboardList[row];

        // Emit dataChanged() only if really changed.
        if ( item.setData(it.value()) ) {
            if ( !topIndex.isValid() ) {
                topIndex = index;
                bottomIndex = index;
            } else {
                topIndex = std::min(topIndex, index);
                bottomIndex = std::max(bottomIndex, index);
            }
        }
    }

    if ( topIndex.isValid() )
        emit dataChanged(topIndex, bottomIndex);
}

bool ClipboardModel::insertRows(int position, int rows, const QModelIndex &parent)
{
    if ( rows <= 0 || position < 0 )
        return false;

    // Legacy mode - use old list
    if (m_tabId < 0) {
        beginInsertRows(QModelIndex(), position, position + rows - 1);

        for (int row = 0; row < rows; ++row)
            m_clipboardList.insert(position, ClipboardItem());

        endInsertRows();

        return true;
    }

    // Database mode - shift existing rows down
    QSqlQuery query(database());
    query.prepare("UPDATE items SET row_index = row_index + ? "
                  "WHERE tab_id = ? AND row_index >= ?");
    query.addBindValue(rows);
    query.addBindValue(m_tabId);
    query.addBindValue(position);
    if (!query.exec()) {
        qWarning() << "Failed to renumber rows for insert:" << query.lastError().text();
        return false;
    }

    // Insert new rows via base class
    if (!QSqlTableModel::insertRows(position, rows, parent))
        return false;

    // Set tab_id and row_index for new rows
    for (int i = 0; i < rows; ++i) {
        const int row = position + i;
        QSqlTableModel::setData(index(row, 1), m_tabId);  // Column 1 = tab_id
        QSqlTableModel::setData(index(row, 2), row);  // Column 2 = row_index
        QSqlTableModel::setData(index(row, 3), 0);  // Column 3 = item_hash (placeholder)
    }

    m_submitTimer.start();  // Delayed commit

    return true;
}

bool ClipboardModel::removeRows(int position, int rows, const QModelIndex &parent)
{
    if ( rows <= 0 || position < 0 || position + rows > rowCount() )
        return false;

    // Legacy mode - use old list
    if (m_tabId < 0) {
        int last = qMin( position + rows, rowCount() ) - 1;

        beginRemoveRows(QModelIndex(), position, last);

        m_clipboardList.remove(position, last - position + 1);

        endRemoveRows();

        return true;
    }

    // Database mode - get item IDs before removing (for cache cleanup)
    QList<int> itemIdsToRemove;
    for (int i = 0; i < rows; ++i) {
        const int itemId = QSqlTableModel::data(index(position + i, 0), Qt::DisplayRole).toInt();
        if (itemId > 0)
            itemIdsToRemove.append(itemId);
    }

    // Remove rows via base class
    if (!QSqlTableModel::removeRows(position, rows, parent))
        return false;

    // Renumber subsequent rows
    QSqlQuery query(database());
    query.prepare("UPDATE items SET row_index = row_index - ? "
                  "WHERE tab_id = ? AND row_index > ?");
    query.addBindValue(rows);
    query.addBindValue(m_tabId);
    query.addBindValue(position);
    if (!query.exec()) {
        qWarning() << "Failed to renumber rows after delete:" << query.lastError().text();
    }

    // Clear cache for removed items
    for (int itemId : itemIdsToRemove) {
        m_mimeDataCache.remove(itemId);
    }

    m_submitTimer.start();  // Delayed commit

    return true;
}

bool ClipboardModel::moveRows(
        const QModelIndex &sourceParent, int sourceRow, int rows,
        const QModelIndex &destinationParent, int destinationRow)
{
    if (sourceParent.isValid() || destinationParent.isValid())
        return false;

    if (sourceRow < 0 || destinationRow < 0 || destinationRow > rowCount()
            || rows <= 0 || sourceRow + rows > rowCount())
    {
        return false;
    }

    const int last = sourceRow + rows - 1;
    if (sourceRow <= destinationRow && destinationRow <= last + 1)
        return false;

    // Legacy mode - use old list
    if (m_tabId < 0) {
        beginMoveRows(sourceParent, sourceRow, last, destinationParent, destinationRow);
        m_clipboardList.move(sourceRow, rows, destinationRow);
        endMoveRows();

        return true;
    }

    // Database mode - update row_index values
    // This is complex, so for now we'll do it by re-assigning all row_index values
    // in the affected range. A more optimized approach would use a temp table.

    // For simplicity: just reload the tab after calling base moveRows
    // TODO: Optimize this with direct SQL updates
    if (!QSqlTableModel::moveRows(sourceParent, sourceRow, rows, destinationParent, destinationRow))
        return false;

    // Renumber all rows to match their current positions
    for (int row = 0; row < rowCount(); ++row) {
        QSqlTableModel::setData(index(row, 2), row);  // Column 2 = row_index
    }

    m_submitTimer.start();  // Delayed commit

    return true;
}

void ClipboardModel::sortItems(const QModelIndexList &indexList, CompareItems *compare)
{
    QList<QPersistentModelIndex> list = validIndeces(indexList);
    std::sort( list.begin(), list.end(), compare );
    sortItems(list);
}

void ClipboardModel::sortItems(const QList<QPersistentModelIndex> &sorted)
{
    int targetRow = topMostRow(sorted);

    for (const auto &ind : sorted) {
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

int ClipboardModel::findItem(uint itemHash) const
{
    for (int i = 0; i < m_clipboardList.size(); ++i) {
        if ( m_clipboardList[i].dataHash() == itemHash )
            return i;
    }

    return -1;
}
