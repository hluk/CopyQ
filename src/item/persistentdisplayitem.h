// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PERSISTENTDISPLAYITEM_H
#define PERSISTENTDISPLAYITEM_H

#include <QMetaType>
#include <QPointer>
#include <QString>
#include <QVariantMap>

class ItemDelegate;
class QAction;
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

    PersistentDisplayItem(QAction *action, const QVariantMap &data);

    /**
     * Returns display data of the item.
     *
     * This method is thread-safe.
     */
    const QVariantMap &data() const noexcept { return m_data; }

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
    QPointer<QAction> m_action;
    QPointer<ItemDelegate> m_delegate;
};

Q_DECLARE_METATYPE(PersistentDisplayItem)

#endif // PERSISTENTDISPLAYITEM_H
