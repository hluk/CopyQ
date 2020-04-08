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

#include "tabbar.h"

#include "common/common.h"
#include "common/mimetypes.h"
#include "gui/iconfactory.h"
#include "gui/tabicons.h"

#include <QIcon>
#include <QMimeData>
#include <QMouseEvent>
#include <QLabel>
#include <QStyle>

namespace {

int dropItemsTabIndex(const QDropEvent &event, const QTabBar &parent)
{
    return canDropToTab(event) ? parent.tabAt( event.pos() ) : -1;
}

int tabIndex(const QString &tabName, const TabBar &parent)
{
    for (int i = 0; i < parent.count(); ++i) {
        if (parent.tabName(i) == tabName)
            return i;
    }

    return -1;
}

void updateTabIcon(int i, TabBar *parent)
{
    const QIcon icon = getIconForTabName(parent->tabName(i));
    parent->setTabIcon(i, icon);
}

} // namespace

TabBar::TabBar(QWidget *parent)
    : QTabBar(parent)
{
    setFocusPolicy(Qt::NoFocus);
    setDrawBase(false);
    setMinimumSize(1, 1);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAcceptDrops(true);

    connect( this, &QTabBar::currentChanged,
             this, &TabBar::onCurrentChanged );
}

QString TabBar::getCurrentTabPath() const
{
    return QString();
}

QString TabBar::tabName(int tabIndex) const
{
    return QTabBar::tabData(tabIndex).toString();
}

void TabBar::setTabName(int tabIndex, const QString &tabName)
{
    // Use tab data to store tab name because tab text can change externally,
    // e.g. Breeze theme adds missing accelerators (renames "test" to "&test).
    setTabData(tabIndex, tabName);
    QTabBar::setTabText(tabIndex, tabName);
}

void TabBar::setTabItemCount(const QString &tabName, const QString &itemCount)
{
    const int i = tabIndex(tabName, *this);
    if (i == -1)
        return;

    QWidget *tabCountLabel = tabButton(i, QTabBar::RightSide);

    if ( itemCount.isEmpty() ) {
        if (tabCountLabel) {
            tabCountLabel->deleteLater();
            setTabButton(i, QTabBar::RightSide, nullptr);
        }
    } else {
        if (!tabCountLabel) {
            tabCountLabel = new QLabel(this);
            tabCountLabel->setObjectName("tab_item_counter");
            setDefaultTabItemCounterStyle(tabCountLabel);
            setTabButton(i, QTabBar::RightSide, tabCountLabel);
        }

        tabCountLabel->setProperty("text", itemCount);
        tabCountLabel->adjustSize();
    }

    updateTabStyle(i);
}

void TabBar::updateTabIcon(const QString &tabName)
{
    const int i = tabIndex(tabName, *this);
    if (i == -1)
        return;

    ::updateTabIcon(i, this);
}

void TabBar::insertTab(int index, const QString &tabName)
{
    const int i = QTabBar::insertTab(index, tabName);
    setTabData(i, tabName);
}

void TabBar::removeTab(int index)
{
    QTabBar::removeTab(index);
}

void TabBar::moveTab(int from, int to)
{
    QTabBar::moveTab(from, to);
}

void TabBar::updateTabIcons()
{
    for (int i = 0; i < count(); ++i)
        ::updateTabIcon(i, this);
}

void TabBar::nextTab()
{
    const int index = (currentIndex() + 1) % count();
    setCurrentIndex(index);
}

void TabBar::previousTab()
{
    const int index = (count() + currentIndex() - 1) % count();
    setCurrentIndex(index);
}

void TabBar::setCurrentTab(int index)
{
    QTabBar::setCurrentIndex(index);
}

void TabBar::adjustSize()
{
    QTabBar::adjustSize();
}

void TabBar::contextMenuEvent(QContextMenuEvent *event)
{
    const int tab = tabAt(event->pos());
    emit tabBarMenuRequested(event->globalPos(), tab);
    event->accept();
}

void TabBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        const int tab = tabAt(event->pos());
        emit tabCloseRequested(tab);
        event->accept();
    } else {
        QTabBar::mousePressEvent(event);
    }
}

void TabBar::dragEnterEvent(QDragEnterEvent *event)
{
    if ( canDropToTab(*event) )
        acceptDrag(event);
    else
        QTabBar::dragEnterEvent(event);
}

void TabBar::dragMoveEvent(QDragMoveEvent *event)
{
    if ( dropItemsTabIndex(*event, *this) != -1 )
        acceptDrag(event);
    else
        QTabBar::dragMoveEvent(event);
}

void TabBar::dropEvent(QDropEvent *event)
{
    int tabIndex = dropItemsTabIndex(*event, *this);

    if ( tabIndex != -1 )
        emit dropItems( tabName(tabIndex), event->mimeData() );
    else
        QTabBar::dropEvent(event);
}

void TabBar::tabInserted(int index)
{
    QTabBar::tabInserted(index);
    ::updateTabIcon(index, this);
    updateTabStyle(index);
}

void TabBar::onCurrentChanged()
{
    for ( int i = 0; i < count(); ++i )
        updateTabStyle(i);
}

void TabBar::updateTabStyle(int index)
{
    QWidget *tabCountLabel = tabButton(index, QTabBar::RightSide);
    if (tabCountLabel) {
        tabCountLabel->setProperty("CopyQ_selected", index == currentIndex());
        style()->unpolish(tabCountLabel);
        style()->polish(tabCountLabel);
    }
}
