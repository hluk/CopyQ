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

#include "gui/itemorderlist.h"
#include "ui_itemorderlist.h"

#include "gui/configurationmanager.h"
#include "gui/iconfactory.h"

#include <QMenu>

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

void ItemOrderList::clearItems()
{
    ui->listWidgetItems->clear();
    foreach ( QWidget *w, m_itemWidgets.values() )
        ui->stackedWidget->removeWidget(w);
    m_itemWidgets.clear();
}

void ItemOrderList::appendItem(const QString &label, bool checked, const QIcon &icon, QWidget *widget)
{
    QListWidgetItem *item = new QListWidgetItem(icon, label, ui->listWidgetItems);
    item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    m_itemWidgets[item] = widget;
    ui->stackedWidget->addWidget(widget);
}

QWidget *ItemOrderList::itemWidget(int row) const
{
    return m_itemWidgets[item(row)];
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
    const QColor color = getDefaultIconColor<QPushButton>();

    // Command button icons.
    ui->toolButtonAdd->setIcon( getIcon("list-add", IconPlus,
                                        getDefaultIconColor<QToolButton>()) );
    ui->pushButtonRemove->setIcon( getIcon("list-remove", IconMinus, color) );
    ui->pushButtonDown->setIcon( getIcon("go-down", IconArrowDown, color) );
    ui->pushButtonUp->setIcon( getIcon("go-up", IconArrowUp, color) );
}

void ItemOrderList::setCurrentItem(int row)
{
    QListWidgetItem *currentItem = item(row);
    ui->listWidgetItems->setCurrentItem(currentItem, QItemSelectionModel::ClearAndSelect);
    m_itemWidgets[currentItem]->setFocus();
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
