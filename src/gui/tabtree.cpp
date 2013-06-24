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
#include <QMouseEvent>

namespace {

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

enum {
    DataIndex = Qt::UserRole,
    DataText
};

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
}

void TabTree::insertTab(const QString &path, int index)
{
    const QStringList pathComponents = path.split('/');
    QTreeWidgetItem *parentItem = NULL;
    QTreeWidgetItem *item = NULL;

    foreach (const QString &text, pathComponents) {
        item = NULL;

        if (parentItem == NULL) {
            for (int i = 0; i < topLevelItemCount(); ++i) {
                if ( topLevelItem(i)->data(0, DataText).toString() == text ) {
                    item = topLevelItem(i);
                    break;
                }
            }
        } else {
            parentItem->setExpanded(true);
            for (int i = 0; i < parentItem->childCount(); ++i) {
                if ( parentItem->child(i)->data(0, DataText).toString() == text ) {
                    item = parentItem->child(i);
                    break;
                }
            }
        }

        if (item == NULL) {
            item = (parentItem == NULL) ? new QTreeWidgetItem(this)
                                        : new QTreeWidgetItem(parentItem);

            item->setData(0, DataIndex, -1);
            item->setData(0, DataText, text);

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
            QLabel *label = new QLabel(labelText, this);
            label->setMargin(2);
            label->adjustSize();
            item->setSizeHint( 0, label->sizeHint() + QSize(2, 2) );
            setItemWidget(item, 0, label);
        }

        parentItem = item;
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

    // Shift greater indexes.
    QList<QTreeWidgetItem *> items = findItems(QString(), Qt::MatchContains | Qt::MatchRecursive);
    for (int i = items.size() - 1; i >= 0; --i) {
        item = items[i];
        const int oldIndex = getTabIndex(item);
        if (oldIndex > index)
            item->setData(0, DataIndex, oldIndex - 1);
    }
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
        if ( !result.isEmpty() )
            result.prepend('/');
        result.prepend(part);
        parent = parent->parent();
    }

    return result;
}

bool TabTree::isTabGroup(const QTreeWidgetItem *item) const
{
    return item != NULL && item->childCount() > 0;
}

void TabTree::mousePressEvent(QMouseEvent *event)
{
    if ( event->button() == Qt::RightButton ) {
        QTreeWidgetItem *item = itemAt(event->pos());
        QString tabPath = getTabPath(item);
        if ( isTabGroup(item) )
            tabPath.append('/');
        emit tabMenuRequested( event->globalPos(), tabPath );
        event->accept();
    } else {
        QTreeWidget::mousePressEvent(event);
    }
}

void TabTree::onCurrentItemChanged(QTreeWidgetItem *current)
{
    emit currentTabChanged( getTabIndex(current) );
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
