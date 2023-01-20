// SPDX-License-Identifier: GPL-3.0-or-later

#include "iconselectbutton.h"

#include "common/config.h"
#include "common/common.h"
#include "gui/fix_icon_id.h"
#include "gui/iconfont.h"
#include "gui/iconselectdialog.h"
#include "gui/icons.h"

#include <QApplication>
#include <QIcon>

#include <memory>

IconSelectButton::IconSelectButton(QWidget *parent)
    : QPushButton(parent)
    , m_currentIcon()
{
    setToolTip(tr("Select Icon…"));

    connect( this, &QAbstractButton::clicked, this, &IconSelectButton::onClicked );

    // reset button text to "..."
    m_currentIcon = "X";
    setCurrentIcon(QString());
}

void IconSelectButton::setCurrentIcon(const QString &iconString)
{
    if ( m_currentIcon == iconString )
        return;

    m_currentIcon = iconString;

    setText(QString());
    setIcon(QIcon());

    if ( iconString.size() == 1 ) {
        const QChar c = iconString[0];
        const ushort id = fixIconId( c.unicode() );
        m_currentIcon = QString(QChar(id));
        setFont(iconFont());
        setText(m_currentIcon);
    } else if ( !iconString.isEmpty() ) {
        const QIcon icon(iconString);
        if ( icon.isNull() )
            m_currentIcon = QString();
        else
            setIcon(icon);
    }

    if (m_currentIcon.isEmpty()) {
        setFont(QFont());
        setText( tr("...", "Select/browse icon.") );
    }

    emit currentIconChanged(m_currentIcon);
}

QSize IconSelectButton::sizeHint() const
{
    const int h = QPushButton::sizeHint().height();
    return QSize(h, h);
}

void IconSelectButton::onClicked()
{
    std::unique_ptr<IconSelectDialog> dialog( new IconSelectDialog(m_currentIcon, this) );

    // Set position under button.
    const QPoint dialogPosition = mapToGlobal(QPoint(0, height()));
    moveWindowOnScreen( dialog.get(), dialogPosition );

    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    connect( dialog.get(), &IconSelectDialog::iconSelected,
             this, &IconSelectButton::setCurrentIcon );
    dialog->open();
    dialog.release();
}

