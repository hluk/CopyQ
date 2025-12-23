// SPDX-License-Identifier: GPL-3.0-or-later

#include "clipboardmodel.h"

#include "common/contenttype.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/textdata.h"

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
        COPYQ_LOG(QString("Submitting database changes for tab: %1").arg(m_tabName));
        if (!submitAll()) {
            qWarning() << "Failed to submit changes:" << lastError().text();
        } else {
            COPYQ_LOG(QString("Successfully submitted changes for tab: %1").arg(m_tabName));
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
    // Only use database - if tab not set, return 0
    if (m_tabId < 0)
        return 0;

    return QSqlTableModel::rowCount(parent);
}

QVariant ClipboardModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    // Database mode only - if tab not set, return empty
    if (m_tabId < 0)
        return QVariant();

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

    // Database mode only - if tab not set, return false
    if (m_tabId < 0)
        return false;

    const int row = index.row();
    const int itemId = QSqlTableModel::data(this->index(row, 0), Qt::DisplayRole).toInt();
    if (itemId == 0)
        return false;

    if (role == contentType::data) {
        const QVariantMap mimeData = value.toMap();
        COPYQ_LOG(QString("setData: Setting MIME data for item_id=%1, formats=%2")
            .arg(itemId).arg(mimeData.keys().join(", ")));

        // Delete old MIME data
        QSqlQuery deleteQuery(database());
        deleteQuery.prepare("DELETE FROM item_data WHERE item_id = ?");
        deleteQuery.addBindValue(itemId);
        deleteQuery.exec();

        // Insert new MIME data
        QSqlQuery insertQuery(database());
        insertQuery.prepare("INSERT INTO item_data (item_id, format, bytes) VALUES (?, ?, ?)");

        int insertedCount = 0;
        for (auto it = mimeData.constBegin(); it != mimeData.constEnd(); ++it) {
            insertQuery.addBindValue(itemId);
            insertQuery.addBindValue(it.key());  // MIME format
            insertQuery.addBindValue(it.value().toByteArray());  // bytes
            if (!insertQuery.exec()) {
                qWarning() << "Failed to insert item_data:" << insertQuery.lastError().text();
                return false;
            }
            insertedCount++;
        }
        COPYQ_LOG(QString("setData: Inserted %1 MIME formats for item_id=%2").arg(insertedCount).arg(itemId));

        // Update item_hash in items table
        const uint itemHash = hash(mimeData);
        QSqlQuery updateHashQuery(database());
        updateHashQuery.prepare("UPDATE items SET item_hash = ? WHERE item_id = ?");
        updateHashQuery.addBindValue(static_cast<int>(itemHash));
        updateHashQuery.addBindValue(itemId);
        if (!updateHashQuery.exec()) {
            qWarning() << "Failed to update item_hash:" << updateHashQuery.lastError().text();
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
    // Database mode only
    if (m_tabId < 0)
        return;

    if (insertRows(row, 1)) {
        // Submit to get the item_id assigned
        submitAll();
        // Now set the MIME data
        setData(index(row, 0), data, contentType::data);
    }
}

void ClipboardModel::insertItems(const QVector<QVariantMap> &dataList, int row)
{
    if ( dataList.isEmpty() )
        return;

    // Database mode only
    if (m_tabId < 0)
        return;

    if (insertRows(row, dataList.size())) {
        // Submit to get the item_ids assigned
        submitAll();
        // Now set the MIME data for all items
        for (int i = 0; i < dataList.size(); ++i) {
            setData(index(row + i, 0), dataList[i], contentType::data);
        }
    }
}

void ClipboardModel::setItemsData(const QMap<QPersistentModelIndex, QVariantMap> &itemsData)
{
    // Database mode only
    if (m_tabId < 0)
        return;

    for (auto it = std::begin(itemsData); it != std::end(itemsData); ++it) {
        const QPersistentModelIndex &index = it.key();
        if ( !index.isValid() )
            continue;

        setData(index, it.value(), contentType::data);
    }
}

bool ClipboardModel::insertRows(int position, int rows, const QModelIndex &parent)
{
    if ( rows <= 0 || position < 0 )
        return false;

    // Database mode only
    if (m_tabId < 0)
        return false;

    // Shift existing rows down
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

    // Database mode only
    if (m_tabId < 0)
        return false;

    // Get item IDs before removing (for cache cleanup)
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

    // Database mode only
    if (m_tabId < 0)
        return false;

    // Update row_index values by re-assigning all row_index values
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
    // Database mode only
    if (m_tabId < 0)
        return;

    int targetRow = topMostRow(sorted);

    for (const auto &ind : sorted) {
        if (ind.isValid()) {
            const int sourceRow = ind.row();

            if (targetRow != sourceRow) {
                moveRows(QModelIndex(), sourceRow, 1, QModelIndex(), targetRow);

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
    // Database mode only
    if (m_tabId < 0)
        return -1;

    // Query database for item with given hash
    QSqlQuery query(database());
    query.prepare("SELECT row_index FROM items WHERE tab_id = ? AND item_hash = ?");
    query.addBindValue(m_tabId);
    query.addBindValue(static_cast<int>(itemHash));

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }

    return -1;
}
