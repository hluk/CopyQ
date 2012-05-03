/*
    Copyright (c) 2012, Lukas Holecek <hluk@email.cz>

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

#include "tabwidget.h"
#include "tabbar.h"

TabWidget::TabWidget(QWidget *parent)
    : QTabWidget(parent)
{
    TabBar *bar = new TabBar(this);
    setTabBar(bar);

    connect( bar, SIGNAL(tabMoved(int, int)),
             this, SIGNAL(tabMoved(int, int)) );
    connect( bar, SIGNAL(tabMenuRequested(QPoint, int)),
             this, SIGNAL(tabMenuRequested(QPoint, int)) );
    connect( bar, SIGNAL(tabCloseRequested(int)),
             this, SIGNAL(tabCloseRequested(int)) );
}

