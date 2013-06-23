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
#include "common/client_server.h"

#include <QAction>
#include <QIcon>
#include <QModelIndex>
#include <QMouseEvent>
#include <QLabel>
#include <QTreeWidget>
#include <QPoint>
#include <QScrollBar>

namespace {

void addTreeAction(QTreeWidget *tree, const QList<QKeySequence> &shortcuts,
                   TabBar *bar, const char *slot, bool widgetContext = true)
{
    QAction *act = new QAction(tree);
    act->setShortcutContext(widgetContext ? Qt::WidgetShortcut : Qt::WindowShortcut);
    act->setShortcuts(shortcuts);
    act->setPriority(QAction::HighPriority);
    tree->addAction(act);
    bar->connect( act, SIGNAL(triggered()), bar, slot );
}

} // namespace

TabBar::TabBar(QWidget *parent)
    : QTabBar(parent)
    , m_treeWidget(NULL)
{
    setFocusPolicy(Qt::NoFocus);
}

void TabBar::setTreeModeEnabled(bool enabled)
{
    if ( isTreeModeEnabled() == enabled )
        return;

    if (enabled) {
        m_treeWidget = new QTreeWidget(this);
        m_treeWidget->setFrameShape(QFrame::NoFrame);
        m_treeWidget->setHeaderHidden(true);
        m_treeWidget->setSelectionMode(QAbstractItemView::SingleSelection);

        if ( isVisible() )
            m_treeWidget->show();

        refresh();

        connect( m_treeWidget->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
                 this, SLOT(onTreeCurrentChanged(QModelIndex)) );
        connect( this, SIGNAL(currentChanged(int)),
                 this, SLOT(onCurrentChanged(int)) );
        onCurrentChanged(currentIndex());

        // More consistent behavior for selecting items using arrow keys.
        addTreeAction(m_treeWidget, QList<QKeySequence>() << Qt::Key_Down << Qt::Key_Right,
                      this, SLOT(nextTreeItem()));
        addTreeAction(m_treeWidget, QList<QKeySequence>() << Qt::Key_Up << Qt::Key_Left,
                      this, SLOT(previousTreeItem()));
        addTreeAction(m_treeWidget, QList<QKeySequence>() << QKeySequence::NextChild,
                      this, SLOT(nextTreeItem()), false);
        addTreeAction(m_treeWidget, QList<QKeySequence>() << QKeySequence::PreviousChild,
                      this, SLOT(previousTreeItem()), false);
    } else {
        delete m_treeWidget;
        m_treeWidget = NULL;
    }

    setUsesScrollButtons(!enabled);
}

bool TabBar::isTreeModeEnabled() const
{
    return m_treeWidget != NULL;
}

QSize TabBar::minimumSizeHint() const
{
    return isTreeModeEnabled() ? m_treeWidget->minimumSize() : QTabBar::minimumSizeHint();
}

QSize TabBar::sizeHint() const
{
    if ( isTreeModeEnabled() )
        return m_treeWidget->size();

    return QTabBar::sizeHint();
}

void TabBar::refresh()
{
    m_treeWidget->clear();

    for (int i = 0; i < count(); ++i)
        insertTabToTree(i);
    updateTreeSize();
}

int TabBar::getCurrentTab() const
{
    QTreeWidgetItem *item = m_treeWidget->currentItem();
    return (item == NULL) ? -1 : item->data(0, Qt::UserRole).toInt();
}

void TabBar::nextTreeItem()
{
    if ( !isTreeModeEnabled() )
        return;

    QTreeWidgetItem *item = m_treeWidget->currentItem();
    if (item != NULL)
        item = m_treeWidget->itemBelow(item);

    if (item == NULL)
        item = m_treeWidget->topLevelItem(0);

    if (item != NULL)
        m_treeWidget->setCurrentItem(item);
}

void TabBar::previousTreeItem()
{
    if ( !isTreeModeEnabled() )
        return;

    QTreeWidgetItem *item = m_treeWidget->currentItem();
    if (item != NULL)
        item = m_treeWidget->itemAbove(item);

    if (item == NULL) {
        item = m_treeWidget->topLevelItem( m_treeWidget->topLevelItemCount() - 1 );
        while (item != NULL && item->childCount() > 0)
            item = item->child( item->childCount() - 1 );
    }

    if (item != NULL)
        m_treeWidget->setCurrentItem(item);
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
        m_treeWidget->resize(size());
}

void TabBar::onCurrentChanged(int index)
{
    if ( isTreeModeEnabled() ) {
        QTreeWidgetItem *item = findTreeItem(index);
        if (item != NULL)
            m_treeWidget->setCurrentItem(item);
    }
}

void TabBar::onTreeCurrentChanged(const QModelIndex &index)
{
    int i = index.data(Qt::UserRole).toInt();
    bool isGroup = i < 0;

    if (!isGroup)
        setCurrentIndex(i);

    emit treeItemSelected(isGroup);
}

QTreeWidgetItem *TabBar::findTreeItem(int index)
{
    QList<QTreeWidgetItem *> items =
            m_treeWidget->findItems(QString(), Qt::MatchContains | Qt::MatchRecursive);
    for (int i = items.size() - 1; i >= 0; --i) {
        if ( items[i]->data(0, Qt::UserRole).toInt() == index )
            return items[i];
    }

    return NULL;
}

void TabBar::insertTabToTree(int index)
{
    Q_ASSERT(isTreeModeEnabled());

    QTreeWidgetItem *parentItem = NULL;
    const QString pathText = tabText(index);
    const QStringList path = pathText.split('/');

    QTreeWidgetItem *item = NULL;

    foreach (const QString &text, path) {
        item = NULL;

        if (parentItem == NULL) {
            for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
                if ( m_treeWidget->topLevelItem(i)->data(0, Qt::UserRole + 1).toString() == text ) {
                    item = m_treeWidget->topLevelItem(i);
                    break;
                }
            }
        } else {
            parentItem->setExpanded(true);
            for (int i = 0; i < parentItem->childCount(); ++i) {
                if ( parentItem->child(i)->data(0, Qt::UserRole + 1).toString() == text ) {
                    item = parentItem->child(i);
                    break;
                }
            }
        }

        if (item == NULL) {
            item = (parentItem == NULL) ? new QTreeWidgetItem(m_treeWidget)
                                        : new QTreeWidgetItem(parentItem);

            item->setData(0, Qt::UserRole, -1);
            item->setData(0, Qt::UserRole + 1, text);

            // Underline key hint in text.
            QString labelText = text;
            int i = labelText.indexOf('&');
            if (i != -1 && i + 1 < text.size()) {
                labelText = labelText.mid(0, i)
                        + "<u>"
                        + escapeHtml(labelText.at(i+1))
                        + "</u>"
                        + labelText.mid(i + 2);
            }

            // Create widget and set item height.
            QLabel *label = new QLabel(labelText, m_treeWidget);
            label->setMargin(2);
            label->adjustSize();
            item->setSizeHint( 0, label->sizeHint() + QSize(2, 2) );
            m_treeWidget->setItemWidget(item, 0, label);
        }

        parentItem = item;
    }

    Q_ASSERT(item != NULL);
    item->setData(0, Qt::UserRole, index);

    onCurrentChanged(currentIndex());
}

void TabBar::removeTabFromTree(int index)
{
    Q_ASSERT(isTreeModeEnabled());

    QTreeWidgetItem *item = findTreeItem(index);
    Q_ASSERT(item != NULL);

    if (item->childCount() == 0) {
        // Recursively remove empty parent item.
        while (item->parent() != NULL && item->parent()->childCount() == 1)
            item = item->parent();
        delete item;
    } else {
        item->setData(0, Qt::UserRole, -1);
    }
}

void TabBar::updateTreeSize()
{
    Q_ASSERT(isTreeModeEnabled());

    int w = m_treeWidget->verticalScrollBar()->sizeHint().width(); // space for scrollbar

    resize(1, height());
    m_treeWidget->resizeColumnToContents(0);
    w += m_treeWidget->columnWidth(0);

    resize( w, height() );
}
