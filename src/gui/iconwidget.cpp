// SPDX-License-Identifier: GPL-3.0-or-later

#include "iconwidget.h"

#include "gui/icons.h"
#include "gui/iconfont.h"

#include <QPainter>
#include <QPaintEvent>
#include <QString>

IconWidget::IconWidget(int icon, QWidget *parent)
    : QWidget(parent)
    , m_icon()
{
    QFontMetrics fm(iconFont());
    QChar c(icon);
    if ( fm.inFont(c) )
        m_icon = QString(c);

    setFixedSize(sizeHint());
}

IconWidget::IconWidget(const QString &icon, QWidget *parent)
    : QWidget(parent)
    , m_icon(icon)
{
    setFixedSize(sizeHint());
}

QSize IconWidget::sizeHint() const
{
    if ( m_icon.isEmpty() )
        return QSize(0, 0);

    const int side = iconFontSizePixels() + 4;
    return QSize(side, side);
}

void IconWidget::paintEvent(QPaintEvent *)
{
    if (m_icon.isEmpty())
        return;

    QPainter p(this);

    if (m_icon.size() == 1) {
        p.setFont(iconFont());
        p.setRenderHint(QPainter::TextAntialiasing, true);

        if (parentWidget())
            p.setPen( parentWidget()->palette().color(QPalette::Text) );
        p.drawText( rect(), Qt::AlignCenter, m_icon);
    } else {
        QPixmap pix(m_icon);
        p.drawPixmap( 0, 0, pix.scaled(size(), Qt::KeepAspectRatio) );
    }
}
