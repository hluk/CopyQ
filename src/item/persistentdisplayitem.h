/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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

#ifndef PERSISTENTDISPLAYITEM_H
#define PERSISTENTDISPLAYITEM_H

#include <QMetaType>
#include <QPointer>
#include <QString>
#include <QVariantMap>

class ItemDelegate;
class QModelIndex;
class QWidget;
class QString;

/**
 * Holds data for currently displayed item.
 *
 * Can be used to change the data for displaying without altering the item data itself.
 */
class PersistentDisplayItem final
{
public:
    PersistentDisplayItem() = default;

    PersistentDisplayItem(
            ItemDelegate *delegate, const QVariantMap &data, QWidget *widget);

    /**
     * Returns display data of the item.
     *
     * This method is thread-safe.
     */
    const QVariantMap &data() const { return m_data; }

    /**
     * Returns true only if display item widget is still available.
     */
    bool isValid();

    /**
     * Sets display data.
     *
     * If data is empty, the item will be displayed later again.
     */
    void setData(const QVariantMap &data);

private:
    QVariantMap m_data;
    QPointer<QWidget> m_widget;
    QPointer<ItemDelegate> m_delegate;
};

Q_DECLARE_METATYPE(PersistentDisplayItem)

#endif // PERSISTENTDISPLAYITEM_H
