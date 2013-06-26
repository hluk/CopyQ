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

#include <QAction>
#include <QLabel>
#include <QList>
#include <QMouseEvent>
#include <QPair>

namespace {

enum {
    DataIndex = Qt::UserRole,
    DataText
};

void addTreeAction(QTreeWidget *tree, const QList<QKeySequence> &shortcuts,
                   const char *slot, bool widgetContext = true)
{
    QAction *act = new QAction(tree);
    act->setShortcutContext(widgetContext ? Qt::WidgetShortcut : Qt::WindowShortcut);
    act->setShortcuts(shortcuts);
    act->setPriority(QAction::HighPriority);
    tree->addAction(act);
    tree->connect( act, SIGNAL(triggered()), slot );
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

TabTree::TabTree(QWidget *parent) :
    QTreeWidget(parent)
{
    // More consistent behavior for selecting items using arrow keys.
    addTreeAction(this, QList<QKeySequence>() << Qt::Key_Down << Qt::Key_Right,
                  SLOT(nextTreeItem()));
    addTreeAction(this, QList<QKeySequence>() << Qt::Key_Up << Qt::Key_Left,
                  SLOT(previousTreeItem()));
    addTreeAction(this, QList<QKeySequence>() << QKeySequence::NextChild,
                  SLOT(nextTreeItem()), false);
    addTreeAction(this, QList<QKeySequence>() << QKeySequence::PreviousChild,
                  SLOT(previousTreeItem()), false);

    connect( this, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
             this, SLOT(onCurrentItemChanged(QTreeWidgetItem*)) );

    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::InternalMove);
    setDragDropOverwriteMode(false);
}

void TabTree::insertTab(const QString &path, int index)
{
    QStringList pathComponents = path.split('/');
    QTreeWidgetItem *item = findLastTreeItem(*this, &pathComponents);

    foreach (const QString &text, pathComponents) {
        item = (item == NULL) ? new QTreeWidgetItem(this) : new QTreeWidgetItem(item);

        item->setExpanded(true);
        item->setData(0, DataIndex, -1);
        item->setData(0, DataText, text);

        // Create widget and set item height.
        createLabel(item);
    }

    Q_ASSERT(item != NULL);
    item->setData(0, DataIndex, index);
}

void TabTree::removeTab(int index)
{
    QTreeWidgetItem *item = findTreeItem(index);
    Q_ASSERT(item != NULL);

    if (item->childCount() == 0) {
        // Recursively remove empty parent item.
        QTreeWidgetItem *parent = item->parent();
        while (parent != NULL && parent->childCount() == 1 && getTabIndex(parent) < 0) {
            item = parent;
            parent = item->parent();
        }

        delete item;
    } else {
        item->setData(0, DataIndex, -1);
    }

    shiftIndexesBetween(index);
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
    const int index = getTabIndex(current);

    blockSignals(true);
    QTreeWidget::dropEvent(event);
    blockSignals(false);

    current = findTreeItem(index);
    emit tabMoved( oldPrefix, getTabPath(current), getTabPath(itemAbove(current)) );
}

void TabTree::onCurrentItemChanged(QTreeWidgetItem *current)
{
    emit currentTabChanged( getTabIndex(current) );
}

void TabTree::requestTabMenu(const QPoint &itemPosition, const QPoint &menuPosition)
{
    QTreeWidgetItem *item = itemAt(itemPosition);
    if (item == NULL)
        return;

    QString tabPath = getTabPath(item);
    emit tabMenuRequested(menuPosition, tabPath);
}

void TabTree::createLabel(QTreeWidgetItem *item)
{
    QString labelText = item->data(0, DataText).toString();

    // Underline key hint in text.
    const int i = labelText.indexOf('&');
    if (i != -1 && i + 1 < labelText.size()) {
        labelText = labelText.mid(0, i)
                + "<u>"
                + escapeHtml(labelText.at(i+1))
                + "</u>"
                + labelText.mid(i + 2);
    }

    QLabel *label = new QLabel(labelText, this);
    label->setMargin(2);
    label->adjustSize();
    label->setTextInteractionFlags(Qt::NoTextInteraction);
    label->setTextFormat(Qt::RichText);
    item->setSizeHint( 0, label->sizeHint() + QSize(2, 2) );
    setItemWidget(item, 0, label);
}

void TabTree::shiftIndexesBetween(int from, int to)
{
    foreach (QTreeWidgetItem *item, findItems(QString(), Qt::MatchContains | Qt::MatchRecursive) ) {
        const int oldIndex = getTabIndex(item);
        if (oldIndex > from && (to <= 0 || oldIndex < to))
            item->setData(0, DataIndex, oldIndex - 1);
    }
}

void TabTree::setCurrentTabIndex(int index)
{
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
        while ( isTabGroup(item) )
            item = item->child( item->childCount() - 1 );
    }

    if (item != NULL)
        setCurrentItem(item);
}
