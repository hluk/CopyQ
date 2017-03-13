/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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
    ui->pushButtonRemove->hide();
    ui->pushButtonAdd->hide();
    setFocusProxy(ui->listWidgetItems);
    setCurrentItemWidget(nullptr);
}

ItemOrderList::~ItemOrderList()
{
    delete ui;
}

void ItemOrderList::setAddRemoveButtonsVisible(bool visible)
{
    ui->pushButtonRemove->setVisible(visible);
    ui->pushButtonAdd->setVisible(visible);
}

void ItemOrderList::clearItems()
{
    ui->listWidgetItems->clear();
    for ( const auto &pair : m_items.values() )
        deleteWidget(pair.widget);
    m_items.clear();
}

void ItemOrderList::appendItem(const QString &label, bool checked, bool highlight, const QIcon &icon, const ItemPtr &item)
{
    insertItem(label, checked, highlight, icon, item, -1);
}

void ItemOrderList::insertItem(const QString &label, bool checked, bool highlight, const QIcon &icon,
                               const ItemPtr &item, int targetRow)
{
    QListWidget *list = ui->listWidgetItems;
    auto listItem = new QListWidgetItem(icon, label);
    const int row = targetRow >= 0 ? qMin(list->count(), targetRow) : list->count();
    list->insertItem(row, listItem);
    listItem->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    setItemHighlight(listItem, highlight);

    m_items[listItem] = ItemWidgetPair(item);

    // Resize list to minimal size.
    const int w = list->sizeHintForColumn(0)
                + list->verticalScrollBar()->sizeHint().width() + 4;
    list->setMaximumWidth(w);

    if ( list->currentItem() == nullptr )
        list->setCurrentRow(row);
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

void ItemOrderList::setCurrentItemHighlight(bool highlight)
{
    QListWidgetItem *current = ui->listWidgetItems->currentItem();
    if(current != nullptr)
        setItemHighlight(current, highlight);
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
    ui->listWidgetItems->setItemHidden(item, !visible);
}

void ItemOrderList::setDragAndDropValidator(const QRegExp &re)
{
    m_dragAndDropRe = re;
    setAcceptDrops(m_dragAndDropRe.isValid());
}

void ItemOrderList::dragEnterEvent(QDragEnterEvent *event)
{
    const QString text = event->mimeData()->text();
    if ( m_dragAndDropRe.indexIn(text) != -1 )
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

    QWidget::showEvent(event);
}

void ItemOrderList::on_pushButtonUp_clicked()
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

void ItemOrderList::on_pushButtonDown_clicked()
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

void ItemOrderList::on_pushButtonRemove_clicked()
{
    for (auto item : ui->listWidgetItems->selectedItems()) {
        ItemWidgetPair pair = m_items.take(item);
        deleteWidget(pair.widget);
        delete item;
    }
}

void ItemOrderList::on_pushButtonAdd_clicked()
{
    emit addButtonClicked();
}

void ItemOrderList::on_listWidgetItems_currentItemChanged(QListWidgetItem *current, QListWidgetItem *)
{
    setCurrentItemWidget( current ? createWidget(current) : nullptr );
}

void ItemOrderList::on_listWidgetItems_itemSelectionChanged()
{
    const QItemSelectionModel *sel = ui->listWidgetItems->selectionModel();
    ui->pushButtonRemove->setEnabled( sel->hasSelection() );
    emit itemSelectionChanged();
}

QListWidgetItem *ItemOrderList::listItem(int row) const
{
    Q_ASSERT(row >= 0 && row < itemCount());
    return ui->listWidgetItems->item(row);
}

void ItemOrderList::setCurrentItemWidget(QWidget *widget)
{
    // Reparent current widget so it's safely deleted.
    QWidget *currentWidget = ui->scrollArea->takeWidget();
    if (currentWidget != nullptr) {
        currentWidget->setParent(this);
        currentWidget->hide();
    }

    if (widget != nullptr) {
        ui->scrollArea->setWidget(widget);
        widget->show();
    }
}

void ItemOrderList::setItemHighlight(QListWidgetItem *item, bool highlight)
{
    QFont font = item->font();
    font.setBold(highlight);
    item->setFont(font);
}

QWidget *ItemOrderList::createWidget(QListWidgetItem *item)
{
    ItemWidgetPair &pair = m_items[item];
    if (!pair.widget)
        pair.widget = pair.item->createWidget(ui->scrollArea);
    return pair.widget;
}
