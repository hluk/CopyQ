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

#include "tabbar.h"

#include <QMouseEvent>

TabBar::TabBar(QWidget *parent)
    : QTabBar(parent)
{
    setFocusPolicy(Qt::NoFocus);
    setDrawBase(false);
    setMinimumSize(1, 1);
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
}

void TabBar::mousePressEvent(QMouseEvent *event)
{
    bool menu = event->button() == Qt::RightButton;
    bool close = event->button() == Qt::MiddleButton;

    if (menu || close) {
        int tab = tabAt(event->pos());
        if (menu)
            emit tabMenuRequested(event->globalPos(), tab);
        else
            emit tabCloseRequested(tab);
        event->accept();
        return;
    }

    QTabBar::mousePressEvent(event);
}
