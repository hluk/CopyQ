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

#include "tabtree.h"

#include "common/client_server.h"

#include <QLabel>
#include <QList>
#include <QMouseEvent>
#include <QScrollBar>

namespace {

enum {
    DataIndex = Qt::UserRole,
    DataText
};

void setItemWidgetSelected(QTreeWidgetItem *item)
{
    if (item == NULL)
        return;

    QTreeWidget *parent = item->treeWidget();
    if (parent == NULL)
        return;

    QWidget *w = parent->itemWidget(item, 0);
    if (w == NULL)
        return;

    w->setProperty("CopyQ_selected", parent->currentItem() == item);
    QStyle *style = w->style();
    style->unpolish(w);
    style->polish(w);

    item->setSizeHint( 0, w->minimumSizeHint() );
}

void labelItem(QTreeWidgetItem *item)
{
    const QString text = item->data(0, DataText).toString();
    if ( text.isEmpty() )
        return;

    QTreeWidget *parent = item->treeWidget();
    QLabel *label = new QLabel(text, parent);
    label->setMargin(2);
    label->setObjectName("tab_tree_item");
    label->setBuddy(parent);
    label->installEventFilter(parent);
    parent->setItemWidget(item, 0, label);
    setItemWidgetSelected(item);
}

QTreeWidgetItem *findLastTreeItem(const QTreeWidget &tree, QStringList *pathComponents)
{
    QTreeWidgetItem *parentItem = NULL;

    if ( !pathComponents->isEmpty() ) {
        const QString &text = pathComponents->first();

        for (int i = 0; i < tree.topLevelItemCount(); ++i) {
            if ( tree.topLevelItem(i)->data(0, DataText).toString() == text ) {
                parentItem = tree.topLevelItem(i);
                break;
            }
        }
    }

    if (parentItem != NULL) {
        pathComponents->pop_front();

        while ( !pathComponents->isEmpty() ) {
            const QString &text = pathComponents->first();
            QTreeWidgetItem *item = NULL;

            for (int i = 0; i < parentItem->childCount(); ++i) {
                if ( parentItem->child(i)->data(0, DataText).toString() == text ) {
                    item = parentItem->child(i);
                    break;
                }
            }

            if (item == NULL)
                break;

            parentItem = item;
            pathComponents->pop_front();
        }
    }

    return parentItem;
}

} // namespace

TabTree::TabTree(QWidget *parent)
    : QTreeWidget(parent)
{
    connect( this, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
             this, SLOT(onCurrentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)) );

    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::InternalMove);
    setDragDropOverwriteMode(false);
    setDefaultDropAction(Qt::CopyAction); // otherwise tab is lost if moved outside tree

    setFrameShape(QFrame::NoFrame);
    setHeaderHidden(true);
    setSelectionMode(QAbstractItemView::SingleSelection);

    setMinimumHeight(fontMetrics().lineSpacing() * 3);
    verticalScrollBar()->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Ignored);

    verticalScrollBar()->installEventFilter(this);
}

void TabTree::insertTab(const QString &path, int index, bool selected)
{
    QStringList pathComponents = path.split('/');
    QTreeWidgetItem *item = findLastTreeItem(*this, &pathComponents);

    foreach (const QString &text, pathComponents) {
        QTreeWidgetItem *parent = item;

        if (parent != NULL) {
            int to = 0;
            for ( ; to < parent->childCount(); ++to ) {
                 const int index2 = getTabIndex(parent->child(to));
                 if (index2 != -1 && index < index2)
                     break;
            }
            int from = parent->childCount();
            item = new QTreeWidgetItem(parent);
            if (from != to)
                parent->insertChild(to, parent->takeChild(from));
        } else {
            int to = 0;
            for ( ; to < topLevelItemCount(); ++to ) {
                 const int index2 = getTabIndex(topLevelItem(to));
                 if (index2 != -1 && index < index2)
                     break;
            }
            int from = topLevelItemCount();
            item = new QTreeWidgetItem(this);
            if (from != to)
                insertTopLevelItem(to, takeTopLevelItem(from));
        }

        item->setExpanded(true);
        item->setData(0, DataIndex, -1);
        item->setData(0, DataText, text);

        labelItem(item);
    }

    Q_ASSERT(item != NULL);
    item->setData(0, DataIndex, index);

    if (selected)
        setCurrentItem(item);

    updateSize();
}

void TabTree::removeTab(int index)
{
    QTreeWidgetItem *item = findTreeItem(index);
    if (item == NULL)
        return;

    if (item->childCount() == 0)
        deleteItem(item);
    else
        item->setData(0, DataIndex, -1);

    shiftIndexesBetween(index + 1);
    updateSize();
}

QTreeWidgetItem *TabTree::findTreeItem(int index) const
{
    QList<QTreeWidgetItem *> items = findItems(QString(), Qt::MatchContains | Qt::MatchRecursive);
    for (int i = items.size() - 1; i >= 0; --i) {
        if ( getTabIndex(items[i]) == index )
            return items[i];
    }

    return NULL;
}

QTreeWidgetItem *TabTree::findTreeItem(const QString &path) const
{
    QStringList pathComponents = path.split('/');
    QTreeWidgetItem *parentItem = findLastTreeItem(*this, &pathComponents);
    return pathComponents.isEmpty() ? parentItem : NULL;
}

int TabTree::getTabIndex(const QTreeWidgetItem *item) const
{
    return (item == NULL) ? -1 : item->data(0, DataIndex).toInt();
}

QString TabTree::getTabPath(const QTreeWidgetItem *item) const
{
    QString result;
    const QTreeWidgetItem *parent = item;

    while (parent != NULL) {
        const QString part = parent->data(0, DataText).toString();
        result.prepend('/');
        result.prepend(part);
        parent = parent->parent();
    }

    result.chop(1);

    return result;
}

bool TabTree::isTabGroup(const QTreeWidgetItem *item) const
{
    return item != NULL && item->childCount() > 0;
}

bool TabTree::isEmptyTabGroup(const QTreeWidgetItem *item) const
{
    return item->childCount() == 0 && getTabIndex(item) < 0;
}

void TabTree::moveTab(int from, int to)
{
    if (from == to)
        return;

    QTreeWidgetItem *item = findTreeItem(from);
    if (item == NULL)
        return;

    if (from < to)
        shiftIndexesBetween(from + 1, to, -1);
    else
        shiftIndexesBetween(to, from - 1, 1);

    item->setData(0, DataIndex, to);
}

void TabTree::setTabText(int tabIndex, const QString &tabText)
{
    QTreeWidgetItem *item = findTreeItem(tabIndex);
    bool isCurrent = item == currentItem();
    insertTab(tabText, tabIndex, isCurrent);

    item->setData(0, DataIndex, -1);
    if ( isEmptyTabGroup(item) )
        deleteItem(item);
}

void TabTree::setCollapseTabs(const QStringList &collapsedPaths)
{
    foreach (const QString &path, collapsedPaths) {
        QTreeWidgetItem *item = findTreeItem(path);
        if ( isTabGroup(item) )
            item->setExpanded(false);
    }
}

QStringList TabTree::collapsedTabs() const
{
    QStringList tabs;

    foreach ( QTreeWidgetItem *item, findItems(QString(), Qt::MatchContains | Qt::MatchRecursive) ) {
        if ( isTabGroup(item) && !item->isExpanded() )
            tabs.append( getTabPath(item) );
    }

    return tabs;
}

QSize TabTree::sizeHint() const
{
    return minimumSizeHint();
}

void TabTree::mousePressEvent(QMouseEvent *event)
{
    if ( event->button() == Qt::RightButton ) {
        requestTabMenu(event->pos(), event->globalPos());
        event->accept();
    } else {
        QTreeWidget::mousePressEvent(event);
    }
}

void TabTree::contextMenuEvent(QContextMenuEvent *event)
{
    requestTabMenu(event->pos(), event->globalPos());
    event->accept();
}

void TabTree::dropEvent(QDropEvent *event)
{
    QTreeWidgetItem *current = currentItem();
    if (current == NULL)
        return;

    const QString oldPrefix = getTabPath(current);

    blockSignals(true);
    QTreeWidget::dropEvent(event);
    setCurrentItem(current);
    setItemWidgetSelected(current);
    blockSignals(false);

    const QString newPrefix = getTabPath(current);
    const QString afterPrefix = getTabPath(itemAbove(current));
    emit tabMoved(oldPrefix, newPrefix, afterPrefix);

    // Remove empty groups.
    foreach ( QTreeWidgetItem *item, findItems(QString(), Qt::MatchContains | Qt::MatchRecursive) ) {
        if ( isEmptyTabGroup(item) )
            deleteItem(item);
    }

    updateSize();
}

bool TabTree::eventFilter(QObject *obj, QEvent *event)
{
    if ( obj == verticalScrollBar() ) {
        if ( event->type() == QEvent::Show || event->type() == QEvent::Hide )
            updateSize();
    } else if ( event->type() == QEvent::Shortcut ) {
        foreach ( QTreeWidgetItem *item, findItems(QString(), Qt::MatchContains | Qt::MatchRecursive) ) {
            if ( itemWidget(item, 0) == obj ) {
                setCurrentItem(item);
                return true;
            }
        }
    }

    return QTreeWidget::eventFilter(obj, event);
}

void TabTree::rowsInserted(const QModelIndex &parent, int start, int end)
{
    QTreeWidget::rowsInserted(parent, start, end);

    for (int row = start; row <= end; ++row) {
        QList<QTreeWidgetItem *> items;
        items.append( parent.isValid() ? itemFromIndex(parent.child(row, 0))
                                       : topLevelItem(row) );
        while ( !items.isEmpty() ) {
            QTreeWidgetItem *item = items.takeLast();
            labelItem(item);

            const int n = item->childCount();
            items.reserve(items.size() + n);
            for (int i = 0; i < n; ++i)
                items.append(item->child(i));
        }
    }
}

void TabTree::showEvent(QShowEvent *event)
{
    QTreeWidget::showEvent(event);
    updateSize();
}

void TabTree::onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    emit currentTabChanged( getTabIndex(current) );
    setItemWidgetSelected(current);
    setItemWidgetSelected(previous);
}

void TabTree::requestTabMenu(const QPoint &itemPosition, const QPoint &menuPosition)
{
    QTreeWidgetItem *item = itemAt(itemPosition);
    QString tabPath = getTabPath(item);
    emit tabMenuRequested(menuPosition, tabPath);
}

void TabTree::shiftIndexesBetween(int from, int to, int how)
{
    foreach ( QTreeWidgetItem *item, findItems(QString(), Qt::MatchContains | Qt::MatchRecursive) ) {
        const int oldIndex = getTabIndex(item);
        if (oldIndex >= from && (to == -1 || oldIndex <= to))
            item->setData(0, DataIndex, oldIndex + how);
    }
}

void TabTree::updateSize()
{
    const QMargins margins = contentsMargins();
    int w = margins.left() + margins.right();

    if ( verticalScrollBar()->isVisible() )
        w += verticalScrollBar()->width();

    expandAll();

    resizeColumnToContents(0);
    w += sizeHintForColumn(0);

    w += 4;

    setFixedWidth(w);
}

void TabTree::setCurrentTabIndex(int index)
{
    if (index < 0)
        return;

    QTreeWidgetItem *item = findTreeItem(index);
    if (item != NULL)
        setCurrentItem(item);
}

void TabTree::nextTreeItem()
{
    QTreeWidgetItem *item = currentItem();
    if (item != NULL)
        item = itemBelow(item);

    if (item == NULL)
        item = topLevelItem(0);

    if (item != NULL)
        setCurrentItem(item);
}

void TabTree::previousTreeItem()
{
    QTreeWidgetItem *item = currentItem();
    if (item != NULL)
        item = itemAbove(item);

    if (item == NULL) {
        item = topLevelItem( topLevelItemCount() - 1 );
        while ( isTabGroup(item) && item->isExpanded() )
            item = item->child( item->childCount() - 1 );
    }

    if (item != NULL)
        setCurrentItem(item);
}

void TabTree::deleteItem(QTreeWidgetItem *item)
{
    // Recursively remove empty parent item.
    QTreeWidgetItem *parent = item->parent();
    while (parent != NULL && parent->childCount() == 1 && getTabIndex(parent) < 0) {
        item = parent;
        parent = item->parent();
    }

    delete item;
}
