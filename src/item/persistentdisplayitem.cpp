// SPDX-License-Identifier: GPL-3.0-or-later

#include "persistentdisplayitem.h"

#include "item/itemdelegate.h"

PersistentDisplayItem::PersistentDisplayItem(ItemDelegate *delegate,
        const QVariantMap &data,
        QWidget *widget)
    : m_data(data)
    , m_widget(widget)
    , m_delegate(delegate)
{
}

bool PersistentDisplayItem::isValid()
{
    if ( m_widget.isNull() || m_delegate.isNull() )
        return false;

    return !m_delegate->invalidateHidden( m_widget.data() );
}

void PersistentDisplayItem::setData(const QVariantMap &data)
{
    if ( !data.isEmpty() && isValid() && m_delegate && data != m_data )
        m_delegate->updateWidget(m_widget, data);
}
