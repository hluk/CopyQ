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
#include <QStackedLayout>

TabWidget::TabWidget(QWidget *parent)
    : QWidget(parent)
    , m_tabBar(NULL)
    , m_tabTree(NULL)
    , m_layout(NULL)
    , m_stackedLayout(NULL)
{
    m_layout = new QBoxLayout(QBoxLayout::TopToBottom, this);
    setLayout(m_layout);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);

    m_stackedLayout = new QStackedLayout(m_layout);
    m_layout->addLayout(m_stackedLayout);

    createTabBar();
}

QString TabWidget::getCurrentTabPath() const
{
    return isTreeModeEnabled() ? m_tabTree->getTabPath( m_tabTree->currentItem() ) : QString();
}

bool TabWidget::isTabGroup(const QString &tab) const
{
    return isTreeModeEnabled() && m_tabTree->isTabGroup( m_tabTree->findTreeItem(tab) );
}

bool TabWidget::isTreeModeEnabled() const
{
    return m_tabTree != NULL;
}

int TabWidget::currentIndex() const
{
    return m_stackedLayout->currentIndex();
}

QWidget *TabWidget::widget(int tabIndex)
{
    return m_stackedLayout->widget(tabIndex);
}

int TabWidget::count() const
{
    return m_stackedLayout->count();
}

QString TabWidget::tabText(int tabIndex) const
{
    return isTreeModeEnabled() ? m_tabTree->getTabPath( m_tabTree->findTreeItem(tabIndex) )
                               : m_tabBar->tabText(tabIndex);
}

void TabWidget::setTabText(int tabIndex, const QString &tabText)
{
    if ( isTreeModeEnabled() ) {
        m_tabTree->setTabText(tabIndex, tabText);
    } else {
        m_tabBar->setTabText(tabIndex, tabText);
    }
}

void TabWidget::insertTab(int tabIndex, QWidget *widget, const QString &tabText)
{
    m_stackedLayout->insertWidget(tabIndex, widget);

    if ( isTreeModeEnabled() )
        m_tabTree->insertTab(tabText, tabIndex, count() == 1);
    else
        m_tabBar->insertTab(tabIndex, tabText);
}

void TabWidget::removeTab(int tabIndex)
{
    if (tabIndex == currentIndex())
        setCurrentIndex(0);

    if ( isTreeModeEnabled() )
        m_tabTree->removeTab(tabIndex);
    else
        m_tabBar->removeTab(tabIndex);

    QWidget *w = m_stackedLayout->widget(tabIndex);
    m_stackedLayout->removeWidget(w);
    delete w;
}

void TabWidget::setTabPosition(QBoxLayout::Direction direction)
{
    m_layout->setDirection(direction);
}

void TabWidget::clear()
{
    while ( !m_stackedLayout->isEmpty() ) {
        QWidget *w = m_stackedLayout->widget(0);
        m_stackedLayout->removeWidget(w);
        delete w;
    }

    if ( isTreeModeEnabled() ) {
        m_tabTree->clear();
    } else {
        while( m_tabBar->count() > 0 )
            m_tabBar->removeTab(0);
    }
}

QStringList TabWidget::tabs() const
{
    QStringList tabs;

    for( int i = 0; i < count(); ++i )
        tabs.append( tabText(i) );

    return tabs;
}

void TabWidget::moveTab(int from, int to)
{
    if ( isTreeModeEnabled() )
        m_tabTree->moveTab(from, to);
    else
        m_tabBar->moveTab(from, to);

    bool isCurrent = currentIndex() == from;

    m_stackedLayout->insertWidget(to, m_stackedLayout->widget(from));

    if (isCurrent)
        setCurrentIndex(to);
}

void TabWidget::setCurrentIndex(int tabIndex)
{
    QWidget *w = currentWidget();
    const int current = (isTreeModeEnabled() && w != NULL && w->isHidden()) ? -1 : currentIndex();

    if (tabIndex == current)
        return;

    if (tabIndex != -1) {
        m_stackedLayout->setCurrentIndex(tabIndex);

        w = currentWidget();
        if (w == NULL)
            return;

        w->show();
        if (isTreeModeEnabled() ? m_tabTree->hasFocus() : m_tabBar->hasFocus())
            w->setFocus();

        if ( isTreeModeEnabled() )
            m_tabTree->setCurrentTabIndex(tabIndex);
        else
            m_tabBar->setCurrentIndex(tabIndex);
    } else if (w != NULL) {
        if (w->hasFocus())
            isTreeModeEnabled() ? m_tabTree->setFocus() : m_tabBar->setFocus();
        w->hide();
    }

    emit currentChanged(tabIndex, current);
}

void TabWidget::nextTab()
{
    if ( isTreeModeEnabled() ) {
        m_tabTree->nextTreeItem();
    } else {
        const int tab = (currentIndex() + 1) % count();
        setCurrentIndex(tab);
    }
}

void TabWidget::previousTab()
{
    if ( isTreeModeEnabled() ) {
        m_tabTree->previousTreeItem();
    } else {
        const int size = count();
        const int tab = (size + currentIndex() - 1) % size;
        setCurrentIndex(tab);
    }
}

void TabWidget::setTabBarDisabled(bool disabled)
{
    if ( isTreeModeEnabled() )
        m_tabTree->setDisabled(disabled);
    else
        m_tabBar->setDisabled(disabled);
}

void TabWidget::setTabBarHidden(bool hidden)
{
    if ( isTreeModeEnabled() )
        m_tabTree->setHidden(hidden);
    else
        m_tabBar->setHidden(hidden);
}

void TabWidget::setTreeModeEnabled(bool enabled)
{
    if (isTreeModeEnabled() == enabled)
        return;

    if (enabled) {
        delete m_tabBar;
        m_tabBar = NULL;
        createTabTree();
    } else {
        delete m_tabTree;
        m_tabTree = NULL;
        createTabBar();
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

void TabWidget::onTabMoved(int from, int to)
{
    m_stackedLayout->insertWidget(to, m_stackedLayout->widget(from));
}

void TabWidget::createTabBar()
{
    m_tabBar = new TabBar(this);
    m_layout->addWidget(m_tabBar);

    m_tabBar->setExpanding(false);
    m_tabBar->setMovable(true);

    connect( m_tabBar, SIGNAL(tabMenuRequested(QPoint, int)),
             this, SIGNAL(tabMenuRequested(QPoint, int)) );
    connect( m_tabBar, SIGNAL(tabRenamed(QString,int)),
             this, SIGNAL(tabRenamed(QString,int)) );
    connect( m_tabBar, SIGNAL(tabCloseRequested(int)),
             this, SIGNAL(tabCloseRequested(int)) );
    connect( m_tabBar, SIGNAL(currentChanged(int)),
             this, SLOT(setCurrentIndex(int)) );
    connect( m_tabBar, SIGNAL(tabMoved(int, int)),
             this, SLOT(onTabMoved(int, int)) );
}

void TabWidget::createTabTree()
{
    m_tabTree = new TabTree(this);
    m_layout->addWidget(m_tabTree);

    m_tabTree->setObjectName("tab_tree");

    connect( m_tabTree, SIGNAL(tabMenuRequested(QPoint,QString)),
             this, SIGNAL(tabMenuRequested(QPoint,QString)) );
    connect( m_tabTree, SIGNAL(tabMoved(QString,QString,QString)),
             this, SIGNAL(tabMoved(QString,QString,QString)) );
    connect( m_tabTree, SIGNAL(currentTabChanged(int)),
             this, SLOT(setCurrentIndex(int)) );
}
