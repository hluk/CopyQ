// SPDX-License-Identifier: GPL-3.0-or-later

#include "persistentdisplayitem.h"

#include "gui/traymenu.h"
#include "item/itemdelegate.h"

#include <QAction>

namespace {

bool dataEquals(const QVariantMap &lhs, const QVariantMap &rhs)
{
    for (auto it = lhs.constBegin(); it != lhs.constEnd(); ++it) {
        if ( it.value().toByteArray() != rhs.value(it.key()).toByteArray() )
            return false;
    }
    return true;
}

} // namespace

PersistentDisplayItem::PersistentDisplayItem(ItemDelegate *delegate,
        const QVariantMap &data,
        QWidget *widget)
    : m_data(data)
    , m_widget(widget)
    , m_delegate(delegate)
{
}

PersistentDisplayItem::PersistentDisplayItem(QAction *action, const QVariantMap &data)
    : m_data(data)
    , m_action(action)
{
}

bool PersistentDisplayItem::isValid()
{
    return !m_action.isNull() || (
        !m_widget.isNull() && !m_delegate.isNull()
        && !m_delegate->invalidateHidden(m_widget.data()) );
}

void PersistentDisplayItem::setData(const QVariantMap &data)
{
    if ( data.isEmpty() || dataEquals(data, m_data) )
        return;

    if ( !m_action.isNull() ) {
        TrayMenu::updateTextFromData(m_action, data);
        TrayMenu::updateIconFromData(m_action, data);
    } else if ( !m_widget.isNull() && !m_delegate.isNull() ) {
        m_delegate->updateWidget(m_widget, data);
    }
}
