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

#include "tabtree.h"

#include "common/common.h"
#include "common/display.h"
#include "common/mimetypes.h"
#include "common/timer.h"
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
#include <QTreeWidgetItemIterator>

namespace {

enum {
    DataText = Qt::UserRole,
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
    if (item == nullptr)
        return;

    QTreeWidget *parent = item->treeWidget();
    if (parent == nullptr)
        return;

    QWidget *w = parent->itemWidget(item, 0);

    if (w) {
        QStyle *style = w->style();
        style->unpolish(w);
        style->polish(w);

        bool selected = parent->currentItem() == item;

        for (auto child : w->findChildren<QWidget *>()) {
            child->setProperty("CopyQ_selected", selected);
            style->unpolish(child);
            style->polish(child);
        }
    }

    updateItemSize(item);
}

QTreeWidgetItem *findLastTreeItem(const QTreeWidget &tree, QStringList *pathComponents)
{
    QTreeWidgetItem *parentItem = nullptr;

    if ( !pathComponents->isEmpty() ) {
        const QString &text = pathComponents->first();

        for (int i = 0; i < tree.topLevelItemCount(); ++i) {
            if ( tree.topLevelItem(i)->data(0, DataText).toString() == text ) {
                parentItem = tree.topLevelItem(i);
                break;
            }
        }
    }

    if (parentItem != nullptr) {
        pathComponents->pop_front();

        while ( !pathComponents->isEmpty() ) {
            const QString &text = pathComponents->first();
            QTreeWidgetItem *item = nullptr;

            for (int i = 0; i < parentItem->childCount(); ++i) {
                if ( parentItem->child(i)->data(0, DataText).toString() == text ) {
                    item = parentItem->child(i);
                    break;
                }
            }

            if (item == nullptr)
                break;

            parentItem = item;
            pathComponents->pop_front();
        }
    }

    return parentItem;
}

QTreeWidgetItem *dropItemsTarget(const QDropEvent &event, const QTreeWidget &parent)
{
    return canDropToTab(event) ? parent.itemAt( event.pos() ) : nullptr;
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

class ItemLabel final : public QWidget {
public:
    explicit ItemLabel(QTreeWidgetItem *item)
        : QWidget(item->treeWidget())
        , m_treeWidget(item->treeWidget())
        , m_label(createLabel("tab_tree_item", this))
        , m_labelItemCount(nullptr)
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
            m_labelItemCount = nullptr;
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
    bool eventFilter(QObject *, QEvent *event) override
    {
        if ( event->type() == QEvent::Shortcut ) {
            for ( QTreeWidgetItemIterator it(m_treeWidget->topLevelItem(0)); *it; ++it ) {
                auto item = *it;
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
    connect( this, &QTreeWidget::currentItemChanged,
             this, &TabTree::onCurrentItemChanged );

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

    connect( this, &QTreeWidget::itemCollapsed, this, &TabTree::updateSize );
    connect( this, &QTreeWidget::itemExpanded, this, &TabTree::updateSize );

    initSingleShotTimer( &m_timerUpdate, 0, this, &TabTree::doUpdateSize );
}

QString TabTree::getCurrentTabPath() const
{
    return getTabPath( currentItem() );
}

bool TabTree::isTabGroup(const QString &tab) const
{
    return isTabGroup( findTreeItem(tab) );
}

QString TabTree::tabName(int tabIndex) const
{
    return getTabPath( findTreeItem(tabIndex) );
}

void TabTree::setTabName(int tabIndex, const QString &tabName)
{
    QTreeWidgetItem *item = findTreeItem(tabIndex);
    Q_ASSERT(item);

    if (getTabPath(item) == tabName)
        return;

    const QString itemCount = item->data(0, DataItemCount).toString();
    insertTab(tabIndex, tabName);
    if (item == currentItem())
        setCurrentTab(tabIndex);

    // Remove old item if it's an empty group.
    m_tabs.removeOne(item);
    if ( isEmptyTabGroup(item) )
        deleteItem(item);

    item = findTreeItem(tabIndex);
    Q_ASSERT(item);
    Q_ASSERT(getTabPath(item) == tabName);

    if ( !itemCount.isEmpty() )
        setTabItemCount(tabName, itemCount);

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

void TabTree::insertTab(int index, const QString &path)
{
    QStringList pathComponents = path.split('/');
    QTreeWidgetItem *item = findLastTreeItem(*this, &pathComponents);
    const bool selectTab = topLevelItemCount() == 0;

    for (const auto &text : pathComponents) {
        QTreeWidgetItem *parent = item;

        if (parent != nullptr) {
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
        item->setData(0, DataText, text);

        const QIcon icon = getIconForTabName( getTabPath(item) );
        item->setIcon(0, icon);

        labelItem(item);
    }

    Q_ASSERT(item != nullptr);
    m_tabs.insert(index, item);

    if (selectTab)
        setCurrentItem(item);

    updateSize();
}

void TabTree::removeTab(int index)
{
    QTreeWidgetItem *item = findTreeItem(index);
    if (item == nullptr)
        return;

    m_tabs.removeOne(item);
    if (item->childCount() == 0)
        deleteItem(item);

    updateSize();
}

void TabTree::moveTab(int from, int to)
{
    if (from == to)
        return;

    QTreeWidgetItem *item = findTreeItem(from);
    if (item == nullptr)
        return;

    m_tabs.removeOne(item);
    m_tabs.insert(to, item);
}

void TabTree::updateCollapsedTabs(QStringList *tabs) const
{
    tabs->clear();
    for ( QTreeWidgetItemIterator it(topLevelItem(0)); *it; ++it ) {
        auto item = *it;
        if ( isTabGroup(item) && !item->isExpanded() )
            tabs->append( getTabPath(item) );
    }
}

void TabTree::setCollapsedTabs(const QStringList &collapsedPaths)
{
    for (const auto &path : collapsedPaths) {
        QTreeWidgetItem *item = findTreeItem(path);
        if ( isTabGroup(item) )
            item->setExpanded(false);
    }
}

void TabTree::updateTabIcons()
{
    for ( QTreeWidgetItemIterator it(topLevelItem(0)); *it; ++it )
        updateTabIcon( getTabPath(*it) );
}

void TabTree::nextTab()
{
    QTreeWidgetItem *item = currentItem();
    if (item != nullptr)
        item = itemBelow(item);

    if (item == nullptr)
        item = topLevelItem(0);

    if (item != nullptr)
        setCurrentItem(item);
}

void TabTree::previousTab()
{
    QTreeWidgetItem *item = currentItem();
    if (item != nullptr)
        item = itemAbove(item);

    if (item == nullptr) {
        item = topLevelItem( topLevelItemCount() - 1 );
        while ( isTabGroup(item) && item->isExpanded() )
            item = item->child( item->childCount() - 1 );
    }

    if (item != nullptr)
        setCurrentItem(item);
}

void TabTree::setCurrentTab(int index)
{
    if (index < 0)
        return;

    QTreeWidgetItem *item = findTreeItem(index);
    if (item != nullptr)
        setCurrentItem(item);
}

void TabTree::adjustSize()
{
    updateSize();
}

QTreeWidgetItem *TabTree::findTreeItem(int index) const
{
    return m_tabs.value(index);
}

QTreeWidgetItem *TabTree::findTreeItem(const QString &path) const
{
    QStringList pathComponents = path.split('/');
    QTreeWidgetItem *parentItem = findLastTreeItem(*this, &pathComponents);
    return pathComponents.isEmpty() ? parentItem : nullptr;
}

int TabTree::getTabIndex(const QTreeWidgetItem *item) const
{
    return (item == nullptr) ? -1 : m_tabs.indexOf( const_cast<QTreeWidgetItem*>(item) );
}

QString TabTree::getTabPath(const QTreeWidgetItem *item) const
{
    QString result;
    const QTreeWidgetItem *parent = item;

    while (parent != nullptr) {
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
    return item != nullptr && item->childCount() > 0;
}

bool TabTree::isEmptyTabGroup(const QTreeWidgetItem *item) const
{
    return item->childCount() == 0 && getTabIndex(item) < 0;
}

QSize TabTree::sizeHint() const
{
    return minimumSizeHint();
}

void TabTree::contextMenuEvent(QContextMenuEvent *event)
{
    requestTabMenu(event->pos(), event->globalPos());
    event->accept();
}

void TabTree::dragEnterEvent(QDragEnterEvent *event)
{
    if ( canDropToTab(*event) ) {
        acceptDrag(event);
    } else {
        QTreeWidget::dragEnterEvent(event);
        // Workaround for QTBUG-44939 (Qt 5.4): Don't ignore successive drag move events.
        event->acceptProposedAction();
    }
}

void TabTree::dragMoveEvent(QDragMoveEvent *event)
{
    if ( dropItemsTarget(*event, *this) )
        acceptDrag(event);
    else if ( itemAt(event->pos()) )
        QTreeWidget::dragMoveEvent(event);
    else
        event->ignore();
}

void TabTree::dropEvent(QDropEvent *event)
{
    const auto current = currentItem();
    if (current == nullptr)
        return;

    const auto targetItem = dropItemsTarget(*event, *this);
    if (targetItem) {
        acceptDrag(event);
        emit dropItems( getTabPath(targetItem), event->mimeData() );
    } else if ( itemAt(event->pos()) ) {
        const QString oldPrefix = getTabPath(current);

        blockSignals(true);
        QTreeWidget::dropEvent(event);
        setCurrentItem(current);
        setItemWidgetSelected(current);
        blockSignals(false);

        // Rename moved item if non-unique.
        QStringList uniqueTabNames;
        const auto parent = current->parent();
        for (int i = 0, count = parent ? parent->childCount() : topLevelItemCount(); i < count; ++i) {
            QTreeWidgetItem *sibling = parent ? parent->child(i) : topLevelItem(i);
            if (sibling != current)
                uniqueTabNames.append( getTabPath(sibling) );
        }

        auto newPrefix = getTabPath(current);
        if ( uniqueTabNames.contains(newPrefix) ) {
            renameToUnique(&newPrefix, uniqueTabNames);
            const QString text = newPrefix.mid( newPrefix.lastIndexOf(QChar('/')) + 1 );
            current->setData(0, DataText, text);
            labelItem(current);
        }

        QList<QTreeWidgetItem*> newTabs;
        QList<int> indexes;
        for ( QTreeWidgetItemIterator it(topLevelItem(0)); *it; ++it ) {
            auto item = *it;
            // Remove empty groups.
            if ( isEmptyTabGroup(item) ) {
                deleteItem(item);
            } else {
                const int oldIndex = getTabIndex(item);
                if (oldIndex != -1) {
                    newTabs.append(item);
                    indexes.append(oldIndex);
                }
            }
        }

        m_tabs = std::move(newTabs);
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

    QList<QTreeWidgetItem *> items;

    for (int row = start; row <= end; ++row) {
        items.clear();
        items.append( parent.isValid() ? itemFromIndex(model()->index(row, 0, parent))
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
    m_timerUpdate.start();
}

void TabTree::doUpdateSize()
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

void TabTree::requestTabMenu(QPoint itemPosition, QPoint menuPosition)
{
    QTreeWidgetItem *item = itemAt(itemPosition);
    QString tabPath = getTabPath(item);
    emit tabTreeMenuRequested(menuPosition, tabPath);
}

void TabTree::deleteItem(QTreeWidgetItem *item)
{
    // Recursively remove empty parent item.
    QTreeWidgetItem *parent = item->parent();
    while (parent != nullptr && parent->childCount() == 1 && getTabIndex(parent) < 0) {
        item = parent;
        parent = item->parent();
    }

    delete item;
}
