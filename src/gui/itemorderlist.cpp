// SPDX-License-Identifier: GPL-3.0-or-later

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

void setItemId(QListWidgetItem *item, int id)
{
    item->setData(Qt::UserRole, id);
}

int itemId(QListWidgetItem *item)
{
    return item->data(Qt::UserRole).toInt();
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
    connect(ui->pushButtonTop, &QToolButton::clicked,
            this, &ItemOrderList::onPushButtonTopClicked);
    connect(ui->pushButtonBottom, &QToolButton::clicked,
            this, &ItemOrderList::onPushButtonBottomClicked);
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
    ui->pushButtonTop->setVisible(movable);
    ui->pushButtonBottom->setVisible(movable);
    ui->listWidgetItems->setDragEnabled(movable);
}

void ItemOrderList::clearItems()
{
    ui->listWidgetItems->clear();
    for (const auto &pair : m_items)
        deleteWidget(pair.second.widget);
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

    ++m_lastItemId;
    setItemId(listItem, m_lastItemId);
    m_items.insert({m_lastItemId, ItemWidgetPair(item, state == Checked)});

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
    const int id = itemId(listItem(row));
    Q_ASSERT(m_items.find(id) != m_items.end());
    return m_items.at(id).widget;
}

QVariant ItemOrderList::data(int row) const
{
    const int id = itemId(listItem(row));
    Q_ASSERT(m_items.find(id) != m_items.end());
    return m_items.at(id).item->data();
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
        ui->pushButtonTop->setIcon( getIcon("go-top", IconAnglesUp) );
        ui->pushButtonBottom->setIcon( getIcon("go-bottom", IconAnglesDown) );
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
    const int row = ui->listWidgetItems->currentRow();
    if (row >= 1)
        moveTab(row, row - 1);
}

void ItemOrderList::onPushButtonDownClicked()
{
    QListWidget *list = ui->listWidgetItems;
    const int row = list->currentRow();
    if (row >= 0 && row + 1 < list->count())
        moveTab(row, row + 1);
}

void ItemOrderList::onPushButtonTopClicked()
{
    const int row = ui->listWidgetItems->currentRow();
    if (row >= 1)
        moveTab(row, 0);
}

void ItemOrderList::onPushButtonBottomClicked()
{
    QListWidget *list = ui->listWidgetItems;
    const int row = ui->listWidgetItems->currentRow();
    if (row >= 0 && row + 1 < list->count())
        moveTab(row, list->count() - 1);
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
    const int id = itemId(item);
    if (id == 0)
        return;

    const auto row = ui->listWidgetItems->row(item);
    const bool checked = item->checkState() == Qt::Checked;

    Q_ASSERT(m_items.find(id) != m_items.end());
    ItemWidgetPair &pair = m_items.at(id);
    if ( pair.lastCheckedState != checked ) {
        pair.lastCheckedState = checked;
        emit itemCheckStateChanged(row, checked);
    }
}

void ItemOrderList::moveTab(int row, int targetRow)
{
    QListWidget *list = ui->listWidgetItems;
    list->blockSignals(true);
    list->insertItem(targetRow, list->takeItem(row));
    list->setCurrentRow(targetRow);
    list->blockSignals(false);
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
    const int id = itemId(item);
    Q_ASSERT(m_items.find(id) != m_items.end());
    ItemWidgetPair &pair = m_items.at(id);
    if (!pair.widget)
        pair.widget = pair.item->createWidget(this);
    return pair.widget;
}

void ItemOrderList::removeItem(QListWidgetItem *item)
{
    const int id = itemId(item);
    Q_ASSERT(m_items.find(id) != m_items.end());
    ItemWidgetPair pair = m_items.at(id);
    m_items.erase(id);
    deleteWidget(pair.widget);
    delete item;
}
