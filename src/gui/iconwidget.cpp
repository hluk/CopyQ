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
