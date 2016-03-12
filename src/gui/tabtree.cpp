/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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

#include "common/common.h"
#include "common/mimetypes.h"
#include "gui/iconfactory.h"
#include "gui/iconfont.h"
#include "gui/tabicons.h"

#include <QApplication>
#include <QLabel>
#include <QList>
#include <QMimeData>
#include <QMouseEvent>
#include <QScrollBar>
#include <QHBoxLayout>

namespace {

enum {
    DataIndex = Qt::UserRole,
    DataText,
    DataItemCount
};

void updateItemSize(QTreeWidgetItem *item)
{
    QSize size(0, 0);

    QWidget *w = item->treeWidget()->itemWidget(item, 0);

    if (w) {
        size = w->minimumSizeHint();

        if (!item->icon(0).isNull()) {
            const QSize iconSize = item->treeWidget()->iconSize();
            size = QSize(
                        size.width() + iconSize.width() + 8,
                        qMax(size.height(), iconSize.height())
                        );
        }
    }

    item->setSizeHint(0, size);
}

void setItemWidgetSelected(QTreeWidgetItem *item)
{
    if (item == NULL)
        return;

    QTreeWidget *parent = item->treeWidget();
    if (parent == NULL)
        return;

    QWidget *w = parent->itemWidget(item, 0);

    if (w) {
        QStyle *style = w->style();
        style->unpolish(w);
        style->polish(w);

        bool selected = parent->currentItem() == item;

        foreach (QWidget *child, w->findChildren<QWidget *>()) {
            child->setProperty("CopyQ_selected", selected);
            style->unpolish(child);
            style->polish(child);
        }
    }

    updateItemSize(item);
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

bool canDrop(const QMimeData &data)
{
    return data.hasFormat(mimeItems) || data.hasText() || data.hasImage() || data.hasUrls();
}

QTreeWidgetItem *dropItemsTarget(const QDropEvent &event, const QTreeWidget &parent)
{
    return canDrop( *event.mimeData() ) ? parent.itemAt( event.pos() ) : NULL;
}

int itemLabelPadding()
{
    return iconFontSizePixels() / 4;
}

QLabel *createLabel(const QString &objectName, QWidget *parent)
{
    QLabel *label = new QLabel(parent);
    label->setMargin(itemLabelPadding());
    label->setObjectName(objectName);

    return label;
}

class ItemLabel : public QWidget {
public:
    explicit ItemLabel(QTreeWidgetItem *item)
        : QWidget(item->treeWidget())
        , m_treeWidget(item->treeWidget())
        , m_label(createLabel("tab_tree_item", this))
        , m_labelItemCount(NULL)
        , m_layout(new QHBoxLayout(this))
    {
        m_label->setBuddy(m_treeWidget);
        m_label->installEventFilter(this);

        m_layout->addWidget(m_label);
        m_layout->setMargin(0);
        m_layout->addStretch(1);

        updateFromItem(item);
    }

    void updateFromItem(QTreeWidgetItem *item)
    {
        const QString text = item->data(0, DataText).toString();
        const QString itemCount = item->data(0, DataItemCount).toString();
        setText(text);
        setItemCountLabel(itemCount);
    }

    void setText(const QString &text)
    {
        m_label->setText(text);
    }

    void setItemCountLabel(const QString &itemCount)
    {
        if (itemCount.isEmpty()) {
            delete m_labelItemCount;
            m_labelItemCount = NULL;
        } else {
            if (!m_labelItemCount) {
                m_labelItemCount = createLabel("tab_item_counter", this);
                setDefaultTabItemCounterStyle(m_labelItemCount);
                m_layout->insertWidget(1, m_labelItemCount);
                m_labelItemCount->show();
            }

            m_labelItemCount->setProperty("text", itemCount);
        }
    }

protected:
    bool eventFilter(QObject *, QEvent *event)
    {
        if ( event->type() == QEvent::Shortcut ) {
            foreach ( QTreeWidgetItem *item, m_treeWidget->findItems(QString(), Qt::MatchContains | Qt::MatchRecursive) ) {
                if ( m_treeWidget->itemWidget(item, 0) == this ) {
                    m_treeWidget->setCurrentItem(item);
                    return true;
                }
            }
        }

        return false;
    }

private:
    QTreeWidget *m_treeWidget;
    QLabel *m_label;
    QLabel *m_labelItemCount;
    QHBoxLayout *m_layout;
};

ItemLabel *itemLabel(QTreeWidgetItem *item)
{
    return static_cast<ItemLabel*>( item->treeWidget()->itemWidget(item, 0) );
}

void labelItem(QTreeWidgetItem *item)
{
    ItemLabel *label = itemLabel(item);
    if (label) {
        label->updateFromItem(item);
        return;
    }

    QTreeWidget *parent = item->treeWidget();
    label = new ItemLabel(item);
    label->installEventFilter(parent);
    item->setTextAlignment(0, Qt::AlignCenter);
    parent->setItemWidget(item, 0, label);

    setItemWidgetSelected(item);
}

bool isInside(QWidget *child, QWidget *parent)
{
    const QPoint scrollBarPosition = child->mapTo(parent, QPoint(0,0));
    return parent->contentsRect().contains(scrollBarPosition);
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

    const int x = smallIconSize();
    setIconSize(QSize(x, x));

    setMinimumHeight(fontMetrics().lineSpacing() * 3);
    verticalScrollBar()->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Ignored);

    verticalScrollBar()->installEventFilter(this);

    connect( this, SIGNAL(itemCollapsed(QTreeWidgetItem*)), SLOT(updateSize()) );
    connect( this, SIGNAL(itemExpanded(QTreeWidgetItem*)), SLOT(updateSize()) );
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

        const QIcon icon = getIconForTabName( getTabPath(item) );
        item->setIcon(0, icon);

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
    Q_ASSERT(item);

    if (getTabPath(item) == tabText)
        return;

    const QString itemCount = item->data(0, DataItemCount).toString();
    bool isCurrent = item == currentItem();

    insertTab(tabText, tabIndex, isCurrent);

    // Remove old item if it's an empty group.
    item->setData(0, DataIndex, -1);
    if ( isEmptyTabGroup(item) )
        deleteItem(item);

    item = findTreeItem(tabIndex);
    Q_ASSERT(item);
    Q_ASSERT(getTabPath(item) == tabText);

    if ( !itemCount.isEmpty() )
        setTabItemCount(tabText, itemCount);

    updateItemSize(item);
    updateSize();
}

void TabTree::setTabItemCount(const QString &tabName, const QString &itemCount)
{
    QTreeWidgetItem *item = findTreeItem(tabName);
    if (!item)
        return;

    item->setData(0, DataItemCount, itemCount);

    ItemLabel *label = itemLabel(item);
    Q_ASSERT(label);
    label->setItemCountLabel(itemCount);

    updateItemSize(item);
    updateSize();
}

void TabTree::setCollapsedTabs(const QStringList &collapsedPaths)
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

void TabTree::updateTabIcon(const QString &tabName)
{
    QTreeWidgetItem *item = findTreeItem(tabName);
    if (!item)
        return;

    const QIcon icon = getIconForTabName(tabName);
    item->setIcon(0, icon);
    updateItemSize(item);
    updateSize();
}

void TabTree::updateTabIcons()
{
    foreach ( QTreeWidgetItem *item, findItems(QString(), Qt::MatchContains | Qt::MatchRecursive) )
        updateTabIcon( getTabPath(item) );
}

void TabTree::contextMenuEvent(QContextMenuEvent *event)
{
    requestTabMenu(event->pos(), event->globalPos());
    event->accept();
}

void TabTree::dragEnterEvent(QDragEnterEvent *event)
{
    if ( canDrop(*event->mimeData()) ) {
        event->acceptProposedAction();
    } else {
        QTreeWidget::dragEnterEvent(event);
        // Workaround for QTBUG-44939 (Qt 5.4): Don't ignore successive drag move events.
        event->acceptProposedAction();
    }
}

void TabTree::dragMoveEvent(QDragMoveEvent *event)
{
    if ( dropItemsTarget(*event, *this) )
        event->acceptProposedAction();
    else if ( itemAt(event->pos()) )
        QTreeWidget::dragMoveEvent(event);
    else
        event->ignore();
}

void TabTree::dropEvent(QDropEvent *event)
{
    QTreeWidgetItem *current = currentItem();
    if (current == NULL)
        return;

    QTreeWidgetItem *item = dropItemsTarget(*event, *this);
    if (item) {
        emit dropItems( getTabPath(item), event );
    } else if ( itemAt(event->pos()) ) {
        const QString oldPrefix = getTabPath(current);

        blockSignals(true);
        QTreeWidget::dropEvent(event);
        setCurrentItem(current);
        setItemWidgetSelected(current);
        blockSignals(false);

        // Rename moved item if non-unique.
        QStringList tabs;
        QTreeWidgetItem *parent = current->parent();
        for (int i = 0, count = parent ? parent->childCount() : topLevelItemCount(); i < count; ++i) {
            QTreeWidgetItem *sibling = parent ? parent->child(i) : topLevelItem(i);
            if (sibling != current)
                tabs.append( getTabPath(sibling) );
        }

        QString newPrefix = getTabPath(current);
        if ( tabs.contains(newPrefix) ) {
            renameToUnique(&newPrefix, tabs);
            const QString text = newPrefix.mid( newPrefix.lastIndexOf(QChar('/')) + 1 );
            current->setData(0, DataText, text);
            labelItem(current);
        }

        QList<int> indexes;
        foreach ( QTreeWidgetItem *item, findItems(QString(), Qt::MatchContains | Qt::MatchRecursive) ) {
            // Remove empty groups.
            if ( isEmptyTabGroup(item) ) {
                deleteItem(item);
            } else {
                const int oldIndex = getTabIndex(item);
                if (oldIndex >= 0) {
                    item->setData(0, DataIndex, indexes.size());
                    indexes.append(oldIndex);
                }
            }
        }

        emit tabsMoved(oldPrefix, newPrefix, indexes);

        updateSize();
    } else {
        event->ignore();
    }
}

bool TabTree::eventFilter(QObject *obj, QEvent *event)
{
    if ( obj == verticalScrollBar() ) {
        if ( event->type() == QEvent::Show || event->type() == QEvent::Hide )
            updateSize();
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

void TabTree::updateSize()
{
    doItemsLayout();

    const QMargins margins = contentsMargins();
    int w = margins.left() + margins.right();

    // Some styles put scrollbar outside of parent's contents so there is no need
    // to resize parent widget.
    if ( verticalScrollBar()->isVisible() && isInside(verticalScrollBar(), this) )
        w += verticalScrollBar()->width();

    resizeColumnToContents(0);
    w += sizeHintForColumn(0);
    resizeColumnToContents(1);
    w += sizeHintForColumn(1);

    setFixedWidth(w);
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
