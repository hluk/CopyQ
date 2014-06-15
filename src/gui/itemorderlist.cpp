/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

#include "gui/configurationmanager.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"

#include <QMenu>
#include <QScrollArea>
#include <QScrollBar>
#include <QDragEnterEvent>

ItemOrderList::ItemOrderList(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ItemOrderList)
{
    ui->setupUi(this);
    ui->pushButtonRemove->hide();
    ui->toolButtonAdd->hide();
    setFocusProxy(ui->listWidgetItems);
    setCurrentItemWidget(NULL);
}

ItemOrderList::~ItemOrderList()
{
    delete ui;
}

void ItemOrderList::setAddMenu(QMenu *menu)
{
    Q_ASSERT(menu != NULL);
    ui->pushButtonRemove->show();
    ui->toolButtonAdd->show();
    ui->toolButtonAdd->setMenu(menu);
}

bool ItemOrderList::hasAddMenu() const
{
    return ui->toolButtonAdd->menu() != NULL;
}

void ItemOrderList::clearItems()
{
    ui->listWidgetItems->clear();
    foreach ( QWidget *w, m_itemWidgets.values() )
        ui->stackedWidget->removeWidget(w);
    m_itemWidgets.clear();
}

void ItemOrderList::appendItem(const QString &label, bool checked, const QIcon &icon, QWidget *widget)
{
    insertItem(label, checked, icon, widget, -1);
}

void ItemOrderList::insertItem(const QString &label, bool checked, const QIcon &icon,
                               QWidget *widget, int targetRow)
{
    QListWidget *list = ui->listWidgetItems;
    QListWidgetItem *item = new QListWidgetItem(icon, label);
    const int row = targetRow >= 0 ? qMin(list->count(), targetRow) : list->count();
    list->insertItem(row, item);
    item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);

    QScrollArea *area = new QScrollArea(ui->stackedWidget);
    area->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    area->setWidget(widget);
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);

    m_itemWidgets[item] = area;
    ui->stackedWidget->addWidget(area);

    // Resize list to minimal size.
    const int w = list->sizeHintForColumn(0)
                + list->verticalScrollBar()->sizeHint().width() + 4;
    list->setMaximumWidth(w);

    if ( list->currentItem() == NULL )
        list->setCurrentRow(row);
}

QWidget *ItemOrderList::itemWidget(int row) const
{
    return m_itemWidgets[item(row)]->widget();
}

int ItemOrderList::itemCount() const
{
    return ui->listWidgetItems->count();
}

bool ItemOrderList::isItemChecked(int row) const
{
    return item(row)->checkState() == Qt::Checked;
}

void ItemOrderList::updateIcons()
{
    static const QColor color = getDefaultIconColor(*ui->pushButtonRemove, QPalette::Window);
    static const QColor color2 = getDefaultIconColor(*ui->toolButtonAdd, QPalette::Window);

    // Command button icons.
    ui->toolButtonAdd->setIcon( getIcon("list-add", IconPlus, color2, color2) );
    ui->pushButtonRemove->setIcon( getIcon("list-remove", IconMinus, color, color) );
    ui->pushButtonDown->setIcon( getIcon("go-down", IconArrowDown, color, color) );
    ui->pushButtonUp->setIcon( getIcon("go-up", IconArrowUp, color, color) );
}

int ItemOrderList::currentRow() const
{
    return ui->listWidgetItems->currentIndex().row();
}

void ItemOrderList::setCurrentItem(int row)
{
    QListWidgetItem *currentItem = item(row);
    ui->listWidgetItems->setCurrentItem(currentItem, QItemSelectionModel::ClearAndSelect);
    QWidget *widget = m_itemWidgets[currentItem];
    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    widget->setFocus();
}

void ItemOrderList::setCurrentItemIcon(const QIcon &icon)
{
    QListWidgetItem *current = ui->listWidgetItems->currentItem();
    if(current != NULL)
        current->setIcon(icon);
}

void ItemOrderList::setCurrentItemLabel(const QString &label)
{
    QListWidgetItem *current = ui->listWidgetItems->currentItem();
    if(current != NULL)
        current->setText(label);
}

QString ItemOrderList::itemLabel(int row) const
{
    return item(row)->text();
}

QList<int> ItemOrderList::selectedRows() const
{
    QList<int> rows;
    foreach (QListWidgetItem *item, ui->listWidgetItems->selectedItems())
        rows.append(ui->listWidgetItems->row(item));
    return rows;
}

void ItemOrderList::setSelectedRows(const QList<int> &selectedRows)
{
    ui->listWidgetItems->clearSelection();
    ui->listWidgetItems->setCurrentItem(NULL);

    foreach (int row, selectedRows) {
        if (row >= 0 && row < rowCount()) {
            QListWidgetItem *item = ui->listWidgetItems->item(row);
            if ( ui->listWidgetItems->currentItem() == NULL )
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
    foreach (QListWidgetItem *item, ui->listWidgetItems->selectedItems())
        delete item;
}

void ItemOrderList::on_toolButtonAdd_triggered(QAction *action)
{
    emit addButtonClicked(action);
}

void ItemOrderList::on_listWidgetItems_currentItemChanged(QListWidgetItem *current, QListWidgetItem *)
{
    setCurrentItemWidget( m_itemWidgets.value(current, NULL) );
}

void ItemOrderList::on_listWidgetItems_itemSelectionChanged()
{
    const QItemSelectionModel *sel = ui->listWidgetItems->selectionModel();
    ui->pushButtonRemove->setEnabled( sel->hasSelection() );
    emit itemSelectionChanged();
}

QListWidgetItem *ItemOrderList::item(int row) const
{
    Q_ASSERT(row >= 0 && row < itemCount());
    return ui->listWidgetItems->item(row);
}

void ItemOrderList::setCurrentItemWidget(QWidget *widget)
{
    if (widget == NULL) {
        ui->stackedWidget->hide();
    } else {
        ui->stackedWidget->setCurrentWidget(widget);
        ui->stackedWidget->show();
    }
}
