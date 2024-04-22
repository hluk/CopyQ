// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CLIPBOARDMODEL_H
#define CLIPBOARDMODEL_H

#include "item/clipboarditem.h"

#include <QAbstractListModel>
#include <QList>

/**
 * Container with clipboard items.
 *
 * Item prepending is optimized.
 */
class ClipboardItemList final {
public:
    ClipboardItem &operator [](int i)
    {
        return m_items[i];
    }

    const ClipboardItem &operator [](int i) const
    {
        return m_items[i];
    }

    void insert(int row, const ClipboardItem &item)
    {
        m_items.insert(row, item);
    }

    void remove(int row, int count)
    {
        const QList<ClipboardItem>::iterator from = m_items.begin() + row;
        const QList<ClipboardItem>::iterator to = from + count;
        m_items.erase(from, to);
    }

    int size() const
    {
        return m_items.size();
    }

    void move(int from, int to)
    {
        const ClipboardItem item = m_items[from];
        m_items.removeAt(from);
        m_items.insert(to, item);
    }

    void move(int from, int count, int to);

    void reserve(int maxItems)
    {
        m_items.reserve(maxItems);
    }

    void resize(int size)
    {
        if (size < this->size())
            remove(size, this->size());

        while (size > this->size())
            m_items.append(ClipboardItem());
    }

private:
    QList<ClipboardItem> m_items;
};

/**
 * Model containing ClipboardItem objects.
 *
 * Class implements QAbstractListModel interface.
 *
 * Clipboard item in model can be serialized and deserialized using
 * operators << and >> (see @ref clipboard_model_serialization_operators).
 */
class ClipboardModel final : public QAbstractListModel
{
public:
    /** Return true if @a lhs is less than @a rhs. */
    using CompareItems = bool (const QModelIndex &, const QModelIndex &);

    explicit ClipboardModel(QObject *parent = nullptr);

    /** Return number of items in model. */
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    /** Return data for given @a index. */
    QVariant data(const QModelIndex &index, int role) const override;

    /** Return flags for given @a index. */
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    bool setData(const QModelIndex &index, const QVariant &value,
                 int role = Qt::EditRole) override;
    bool insertRows(int position, int rows,
                    const QModelIndex &index = QModelIndex()) override;
    bool removeRows(int position, int rows,
                    const QModelIndex &index = QModelIndex()) override;
    bool moveRows(const QModelIndex &sourceParent, int sourceRow, int rows,
            const QModelIndex &destinationParent, int destinationRow) override;

    /** insert new item to model. */
    void insertItem(const QVariantMap &data, int row);

    void insertItems(const QVector<QVariantMap> &dataList, int row);

    void setItemsData(const QMap<QPersistentModelIndex, QVariantMap> &itemsData);

    /**
     * Sort items in ascending order.
     */
    void sortItems(const QModelIndexList &indexList, CompareItems *compare);
    void sortItems(const QList<QPersistentModelIndex> &sorted);

    /**
     * Find item with given @a hash.
     * @return Row number with found item or -1 if no item was found.
     */
    int findItem(uint itemHash) const;

private:
    ClipboardItemList m_clipboardList;
};

#endif // CLIPBOARDMODEL_H
