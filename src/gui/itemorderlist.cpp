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

#include "gui/itemorderlist.h"
#include "ui_itemorderlist.h"

#include "gui/iconfactory.h"
#include "gui/iconfont.h"
#include "gui/icons.h"

#include <QDragEnterEvent>
#include <QMenu>
#include <QMimeData>
#include <QScrollBar>

namespace {

void deleteWidget(const QPointer<QWidget> &object)
{
    if (object)
        object->deleteLater();
}

} // namespace

ItemOrderList::ItemOrderList(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ItemOrderList)
{
    ui->setupUi(this);

    connect(ui->pushButtonUp, &QToolButton::clicked,
            this, &ItemOrderList::onPushButtonUpClicked);
    connect(ui->pushButtonDown, &QToolButton::clicked,
            this, &ItemOrderList::onPushButtonDownClicked);
    connect(ui->pushButtonRemove, &QToolButton::clicked,
            this, &ItemOrderList::onPushButtonRemoveClicked);
    connect(ui->pushButtonAdd, &QToolButton::clicked,
            this, &ItemOrderList::onPushButtonAddClicked);

    connect(ui->listWidgetItems, &QListWidget::currentItemChanged,
            this, &ItemOrderList::onListWidgetItemsCurrentItemChanged);
    connect(ui->listWidgetItems, &QListWidget::itemSelectionChanged,
            this, &ItemOrderList::onListWidgetItemsItemSelectionChanged);
    connect(ui->listWidgetItems, &QListWidget::itemChanged,
            this, &ItemOrderList::onListWidgetItemsItemChanged);

    ui->pushButtonRemove->hide();
    ui->pushButtonAdd->hide();
    setFocusProxy(ui->listWidgetItems);
    ui->listWidgetItems->setFocus();
    setCurrentItemWidget(nullptr);

    setEditable(false);
    setItemsMovable(false);
}

ItemOrderList::~ItemOrderList()
{
    delete ui;
}

void ItemOrderList::setEditable(bool editable)
{
    ui->pushButtonRemove->setVisible(editable);
    ui->pushButtonAdd->setVisible(editable);
    ui->listWidgetItems->setSelectionMode(
        editable
            ? QAbstractItemView::ExtendedSelection
            : QAbstractItemView::SingleSelection);
}

void ItemOrderList::setItemsMovable(bool movable)
{
    ui->pushButtonUp->setVisible(movable);
    ui->pushButtonDown->setVisible(movable);
    ui->listWidgetItems->setDragEnabled(movable);
}

void ItemOrderList::clearItems()
{
    ui->listWidgetItems->clear();
    for (const auto &pair : m_items)
        deleteWidget(pair.widget);
    m_items.clear();
}

void ItemOrderList::appendItem(
        const QString &label, const QIcon &icon,
        const ItemPtr &item, CheckState state)
{
    insertItem(label, icon, item, -1, state);
}

void ItemOrderList::insertItem(
        const QString &label, const QIcon &icon,
        const ItemPtr &item, int targetRow, CheckState state)
{
    QListWidget *list = ui->listWidgetItems;
    auto listItem = new QListWidgetItem(icon, label);
    if (state != NotCheckable)
        listItem->setCheckState(state == Checked ? Qt::Checked : Qt::Unchecked);
    m_items[listItem] = ItemWidgetPair(item, state == Checked);

    const int row = targetRow >= 0 ? qMin(list->count(), targetRow) : list->count();
    list->insertItem(row, listItem);

    if ( list->currentItem() == nullptr )
        list->setCurrentRow(row);
}

void ItemOrderList::removeRow(int row)
{
    auto item = listItem(row);
    removeItem(item);
}

QWidget *ItemOrderList::widget(int row) const
{
    return m_items[listItem(row)].widget;
}

QVariant ItemOrderList::data(int row) const
{
    return m_items[listItem(row)].item->data();
}

int ItemOrderList::itemCount() const
{
    return m_items.size();
}

bool ItemOrderList::isItemChecked(int row) const
{
    return listItem(row)->checkState() == Qt::Checked;
}

int ItemOrderList::currentRow() const
{
    return ui->listWidgetItems->currentIndex().row();
}

void ItemOrderList::setCurrentItem(int row)
{
    QListWidgetItem *currentItem = listItem(row);
    ui->listWidgetItems->setCurrentItem(currentItem, QItemSelectionModel::ClearAndSelect);
    QWidget *widget = createWidget(currentItem);
    widget->setFocus();
}

void ItemOrderList::setCurrentItemIcon(const QIcon &icon)
{
    QListWidgetItem *current = ui->listWidgetItems->currentItem();
    if(current != nullptr)
        current->setIcon(icon);
}

void ItemOrderList::setCurrentItemLabel(const QString &label)
{
    QListWidgetItem *current = ui->listWidgetItems->currentItem();
    if(current != nullptr)
        current->setText(label);
}

void ItemOrderList::setItemIcon(int row, const QIcon &icon)
{
    QListWidgetItem *item = listItem(row);
    if(item != nullptr)
        item->setIcon(icon);
}

QString ItemOrderList::itemLabel(int row) const
{
    return listItem(row)->text();
}

QList<int> ItemOrderList::selectedRows() const
{
    QList<int> rows;
    for (auto item : ui->listWidgetItems->selectedItems())
        rows.append(ui->listWidgetItems->row(item));
    return rows;
}

void ItemOrderList::setSelectedRows(const QList<int> &selectedRows)
{
    ui->listWidgetItems->clearSelection();
    ui->listWidgetItems->setCurrentItem(nullptr);

    for (int row : selectedRows) {
        if (row >= 0 && row < rowCount()) {
            QListWidgetItem *item = ui->listWidgetItems->item(row);
            if ( ui->listWidgetItems->currentItem() == nullptr )
                ui->listWidgetItems->setCurrentItem(item);
            else
                item->setSelected(true);
        }
    }
}

int ItemOrderList::rowCount() const
{
    return ui->listWidgetItems->count();
}

void ItemOrderList::setItemWidgetVisible(int row, bool visible)
{
    QListWidgetItem *item = ui->listWidgetItems->item(row);
    Q_ASSERT(item);
    item->setHidden(!visible);
}

void ItemOrderList::setDragAndDropValidator(const QRegularExpression &re)
{
    m_dragAndDropRe = re;
    setAcceptDrops(m_dragAndDropRe.isValid());
}

void ItemOrderList::setWiderIconsEnabled(bool wider)
{
    const auto height = iconFontSizePixels();
    const auto width = wider ? height * 3/2 : height;
    ui->listWidgetItems->setIconSize( QSize(width, height) );
}

void ItemOrderList::keyPressEvent(QKeyEvent *event)
{
    if ( event->matches(QKeySequence::NextChild) ) {
         nextPreviousItem(1);
         event->accept();
         return;
    }

    if ( event->matches(QKeySequence::PreviousChild) ) {
         nextPreviousItem(-1);
         event->accept();
         return;
    }

    QWidget::keyPressEvent(event);
}

void ItemOrderList::dragEnterEvent(QDragEnterEvent *event)
{
    const QString text = event->mimeData()->text();
    if ( text.contains(m_dragAndDropRe) )
        event->acceptProposedAction();
}

void ItemOrderList::dropEvent(QDropEvent *event)
{
    event->accept();

    QListWidget *list = ui->listWidgetItems;
    const QPoint pos = list->mapFromParent(event->pos());

    const int s = list->spacing();
    QModelIndex index = list->indexAt(pos);
    if ( !index.isValid() )
        index = list->indexAt( pos + QPoint(s, - 2 * s) );

    emit dropped( event->mimeData()->text(), index.row() );
}

void ItemOrderList::showEvent(QShowEvent *event)
{
    if ( ui->pushButtonAdd->icon().isNull() ) {
        ui->pushButtonAdd->setIcon( getIcon("list-add", IconPlus) );
        ui->pushButtonRemove->setIcon( getIcon("list-remove", IconMinus) );
        ui->pushButtonDown->setIcon( getIcon("go-down", IconArrowDown) );
        ui->pushButtonUp->setIcon( getIcon("go-up", IconArrowUp) );
    }

    // Resize list to minimal size.
    QListWidget *list = ui->listWidgetItems;
    const int w = list->sizeHintForColumn(0)
                + list->verticalScrollBar()->sizeHint().width() + 4;
    const auto sizes = ui->splitter->sizes();
    ui->splitter->setSizes({w, sizes[0] + sizes[1] - w});

    QWidget::showEvent(event);
}

void ItemOrderList::nextPreviousItem(int d)
{
    QListWidget *list = ui->listWidgetItems;
    const int rowCount = list->count();
    if (rowCount < 2)
        return;

    const int row = list->currentRow();
    const int nextRow = (row + d + rowCount) % rowCount;
    list->setCurrentRow(nextRow, QItemSelectionModel::ClearAndSelect);
}

void ItemOrderList::onPushButtonUpClicked()
{
    QListWidget *list = ui->listWidgetItems;
    const int row = list->currentRow();
    if (row < 1)
        return;

    list->blockSignals(true);
    list->insertItem(row - 1, list->takeItem(row));
    list->setCurrentRow(row - 1);
    list->blockSignals(false);
}

void ItemOrderList::onPushButtonDownClicked()
{
    QListWidget *list = ui->listWidgetItems;
    const int row = list->currentRow();
    if (row < 0 || row == list->count() - 1)
        return;

    list->blockSignals(true);
    list->insertItem(row + 1, list->takeItem(row));
    list->setCurrentRow(row + 1);
    list->blockSignals(false);
}

void ItemOrderList::onPushButtonRemoveClicked()
{
    for (auto item : ui->listWidgetItems->selectedItems())
        removeItem(item);
}

void ItemOrderList::onPushButtonAddClicked()
{
    emit addButtonClicked();
}

void ItemOrderList::onListWidgetItemsCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *)
{
    setCurrentItemWidget( current ? createWidget(current) : nullptr );
}

void ItemOrderList::onListWidgetItemsItemSelectionChanged()
{
    const QItemSelectionModel *sel = ui->listWidgetItems->selectionModel();
    ui->pushButtonRemove->setEnabled( sel->hasSelection() );
    emit itemSelectionChanged();
}

void ItemOrderList::onListWidgetItemsItemChanged(QListWidgetItem *item)
{
    const auto row = ui->listWidgetItems->row(item);
    const bool checked = isItemChecked(row);

    if ( m_items[item].lastCheckedState != checked ) {
        m_items[item].lastCheckedState = checked;
        emit itemCheckStateChanged(row, checked);
    }
}

QListWidgetItem *ItemOrderList::listItem(int row) const
{
    Q_ASSERT(row >= 0 && row < itemCount());
    return ui->listWidgetItems->item(row);
}

void ItemOrderList::setCurrentItemWidget(QWidget *widget)
{
    QLayoutItem *layoutItem =  ui->widgetLayout->takeAt(0);
    if (layoutItem)
        layoutItem->widget()->hide();
    delete layoutItem;

    if (widget) {
        ui->widgetLayout->addWidget(widget);
        ui->widgetParent->setFocusProxy(widget);
        widget->show();
    }
}

QWidget *ItemOrderList::createWidget(QListWidgetItem *item)
{
    ItemWidgetPair &pair = m_items[item];
    if (!pair.widget)
        pair.widget = pair.item->createWidget(this);
    return pair.widget;
}

void ItemOrderList::removeItem(QListWidgetItem *item)
{
    ItemWidgetPair pair = m_items.take(item);
    deleteWidget(pair.widget);
    delete item;
}
