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

#include "tabwidget.h"
#include "tabbar.h"
#include "tabtree.h"

#include <QPoint>

TabWidget::TabWidget(QWidget *parent)
    : QTabWidget(parent)
    , m_bar(new TabBar(this))
{
    m_bar->setObjectName("tabs");

    setTabBar(m_bar);

    connect( m_bar, SIGNAL(tabMoved(int, int)),
             this, SIGNAL(tabMoved(int, int)) );
    connect( m_bar, SIGNAL(tabMenuRequested(QPoint, int)),
             this, SIGNAL(tabMenuRequested(QPoint, int)) );
    connect( m_bar, SIGNAL(tabRenamed(QString,int)),
             this, SIGNAL(tabRenamed(QString,int)) );
    connect( m_bar, SIGNAL(tabCloseRequested(int)),
             this, SIGNAL(tabCloseRequested(int)) );
    connect( m_bar, SIGNAL(treeItemSelected(bool)),
             this, SLOT(onTreeItemSelected(bool)) );
}

void TabWidget::refreshTabBar()
{
    return m_bar->refresh();
}

int TabWidget::getCurrentTab() const
{
    return m_bar->getCurrentTab();
}

QString TabWidget::getCurrentTabPath() const
{
    return isTreeModeEnabled() ? tabTree()->getTabPath( tabTree()->currentItem() ) : QString();
}

bool TabWidget::isTabGroup(const QString &tab) const
{
    return isTreeModeEnabled() && tabTree()->isTabGroup( tabTree()->findTreeItem(tab) );
}

bool TabWidget::isTreeModeEnabled() const
{
    return m_bar->isTreeModeEnabled();
}

void TabWidget::moveTab(int from, int to)
{
    m_bar->moveTab(from, to);
}

void TabWidget::nextTab()
{
    if ( isTreeModeEnabled() ) {
        tabTree()->nextTreeItem();
    } else {
        const int tab = (currentIndex() + 1) % count();
        setCurrentIndex(tab);
    }
}

void TabWidget::previousTab()
{
    if ( isTreeModeEnabled() ) {
        tabTree()->previousTreeItem();
    } else {
        const int size = count();
        const int tab = (size + currentIndex() - 1) % size;
        setCurrentIndex(tab);
    }
}

void TabWidget::setTabBarDisabled(bool disabled)
{
    tabBar()->setDisabled(disabled);
}

void TabWidget::setTabBarHidden(bool hidden)
{
    tabBar()->setHidden(hidden);
}

void TabWidget::setTreeModeEnabled(bool enabled)
{
    if (m_bar->isTreeModeEnabled() == enabled)
        return;

    m_bar->setTreeModeEnabled(enabled);

    if (enabled) {
        connect( tabTree(), SIGNAL(tabMenuRequested(QPoint,QString)),
                 this, SIGNAL(tabMenuRequested(QPoint,QString)) );
        connect( tabTree(), SIGNAL(tabMoved(QString,QString,QString)),
                 this, SIGNAL(tabMoved(QString,QString,QString)) );
    } else {
        QWidget *w = currentWidget();
        if (w != NULL)
            w->show();
    }
}

void TabWidget::onTreeItemSelected(bool isGroup)
{
    QWidget *w = currentWidget();
    if (w == NULL)
        return;

    if (isGroup) {
        w->hide();
    } else {
        w->show();
        w->setFocus();
    }
}

TabTree *TabWidget::tabTree() const
{
    return m_bar->tabTree();
}
