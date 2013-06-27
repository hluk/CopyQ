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

#include "gui/tabtree.h"

#include <QModelIndex>
#include <QMouseEvent>
#include <QTreeWidget>
#include <QScrollBar>

TabBar::TabBar(QWidget *parent)
    : QTabBar(parent)
    , m_tabTree(NULL)
{
    setFocusPolicy(Qt::NoFocus);
}

void TabBar::setTreeModeEnabled(bool enabled)
{
    if ( isTreeModeEnabled() == enabled )
        return;

    if (enabled) {
        m_tabTree = new TabTree(this);
        m_tabTree->setFrameShape(QFrame::NoFrame);
        m_tabTree->setHeaderHidden(true);
        m_tabTree->setSelectionMode(QAbstractItemView::SingleSelection);

        if ( isVisible() )
            m_tabTree->show();

        refresh();

        connect( m_tabTree, SIGNAL(currentTabChanged(int)),
                 this, SLOT(onTreeCurrentChanged(int)) );
        connect( this, SIGNAL(currentChanged(int)),
                 m_tabTree, SLOT(setCurrentTabIndex(int)) );
    } else {
        delete m_tabTree;
        m_tabTree = NULL;
    }

    setUsesScrollButtons(!enabled);
}

bool TabBar::isTreeModeEnabled() const
{
    return m_tabTree != NULL;
}

QSize TabBar::minimumSizeHint() const
{
    return isTreeModeEnabled() ? m_tabTree->minimumSize() : QTabBar::minimumSizeHint();
}

QSize TabBar::sizeHint() const
{
    if ( isTreeModeEnabled() )
        return m_tabTree->size();

    return QTabBar::sizeHint();
}

void TabBar::refresh(const QString &currentPath)
{
    if ( !isTreeModeEnabled() )
        return;

    m_tabTree->clear();

    for (int i = 0; i < count(); ++i)
        insertTabToTree(i);
    updateTreeSize();

    if ( !currentPath.isEmpty() )
        m_tabTree->setCurrentItem( m_tabTree->findTreeItem(currentPath) );
}

int TabBar::getCurrentTab() const
{
    return isTreeModeEnabled() ? m_tabTree->getTabIndex( m_tabTree->currentItem() ) : currentIndex();
}

void TabBar::tabInserted(int index)
{
    QTabBar::tabInserted(index);

    if ( isTreeModeEnabled() ) {
        insertTabToTree(index);
        updateTreeSize();
    }
}

void TabBar::tabRemoved(int index)
{
    QTabBar::tabRemoved(index);

    if ( isTreeModeEnabled() ) {
        removeTabFromTree(index);
        updateTreeSize();
        if ( getCurrentTab() != currentIndex() )
            m_tabTree->setCurrentTabIndex( currentIndex() );
    }
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

void TabBar::resizeEvent(QResizeEvent *event)
{
    QTabBar::resizeEvent(event);
    if ( isTreeModeEnabled() )
        m_tabTree->resize(size());
}

void TabBar::onTreeCurrentChanged(int index)
{
    bool isGroup = index < 0;

    if (!isGroup)
        setCurrentIndex(index);

    emit treeItemSelected(isGroup);
}

void TabBar::onTreeTabMoved(int index, const QString &newName)
{
    setTabText(index, newName);
    emit tabRenamed(newName, index);
}

void TabBar::onTreeTabMoved(int from, int to)
{
    moveTab(from, to);
}

void TabBar::insertTabToTree(int index)
{
    Q_ASSERT(isTreeModeEnabled());
    m_tabTree->insertTab(tabText(index), index, index == currentIndex());
}

void TabBar::removeTabFromTree(int index)
{
    Q_ASSERT(isTreeModeEnabled());
    m_tabTree->removeTab(index);
}

void TabBar::updateTreeSize()
{
    Q_ASSERT(isTreeModeEnabled());

    int w = m_tabTree->verticalScrollBar()->sizeHint().width(); // space for scrollbar

    resize(1, height());
    m_tabTree->resizeColumnToContents(0);
    w += m_tabTree->columnWidth(0);

    resize( w, height() );
}
