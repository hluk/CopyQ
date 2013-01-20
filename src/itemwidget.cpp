/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#include "itemwidget.h"

#include <QLabel>
#include <QPainter>

ItemWidget::ItemWidget(QWidget *widget)
    : m_re()
    , m_size()
{
    Q_ASSERT(widget != NULL);

    // Limit size of items.
    widget->setMaximumSize(2048, 2048);

    QWidget *parent = widget->parentWidget();
    if (parent != NULL) {
        QPalette palette( parent->palette() );
        palette.setColor(QPalette::Background, Qt::transparent);
        palette.setColor(QPalette::Base, Qt::transparent);
        widget->setPalette(palette);
        widget->setFont( parent->font() );
        widget->setAutoFillBackground(true);
        widget->setFocusPolicy(Qt::NoFocus);
    }
}

void ItemWidget::setHighlight(const QRegExp &re, const QFont &highlightFont,
                              const QPalette &highlightPalette)
{
    QPalette palette( widget()->palette() );
    palette.setColor(QPalette::Highlight, highlightPalette.base().color());
    palette.setColor(QPalette::HighlightedText, highlightPalette.text().color());
    widget()->setPalette(palette);

    if (m_re == re)
        return;
    m_re = re;
    highlight(re, highlightFont, highlightPalette);
}

void ItemWidget::setMaximumSize(const QSize &size)
{
    widget()->setMaximumSize(size);
    updateSize();
    updateItem();
}

void ItemWidget::updateItem()
{
    m_size = widget()->size();
}
