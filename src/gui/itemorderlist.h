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

#ifndef ITEMORDERLIST_H
#define ITEMORDERLIST_H

#include <QMap>
#include <QListWidgetItem>
#include <QPointer>
#include <QWidget>

#include <memory>

namespace Ui {
class ItemOrderList;
}

class QAbstractButton;
class QMenu;
class QScrollArea;

class ItemOrderList final : public QWidget
{
    Q_OBJECT

public:
    enum CheckState {
        Checked,
        Unchecked,
        NotCheckable
    };

    class Item {
        friend class ItemOrderList;
    public:
        Item() = default;
        virtual ~Item() = default;
        virtual QVariant data() const = 0;
        Item(const Item &) = delete;
        Item &operator=(const Item &) = delete;
    private:
        virtual QWidget *createWidget(QWidget *parent) = 0;
    };

    using ItemPtr = std::shared_ptr<Item>;

    explicit ItemOrderList(QWidget *parent = nullptr);
    ~ItemOrderList();

    void setEditable(bool editable);
    void setItemsMovable(bool movable);

    void clearItems();

    void appendItem(const QString &label, const QIcon &icon, const ItemPtr &item, CheckState state = NotCheckable);

    void insertItem(const QString &label, const QIcon &icon, const ItemPtr &item, int targetRow, CheckState state = NotCheckable);

    void removeRow(int row);

    /// Returns widget created by Item::createWidget() given @a row
    /// (could be nullptr is not yet created).
    QWidget *widget(int row) const;

    /// Returns Item::data() for item in given @a row.
    QVariant data(int row) const;

    int itemCount() const;

    bool isItemChecked(int row) const;

    int currentRow() const;

    void setCurrentItem(int row);

    void setCurrentItemIcon(const QIcon &icon);

    void setCurrentItemLabel(const QString &label);

    void setItemIcon(int row, const QIcon &icon);

    QString itemLabel(int row) const;

    QList<int> selectedRows() const;
    void setSelectedRows(const QList<int> &selectedRows);

    int rowCount() const;

    void setItemWidgetVisible(int row, bool visible);

    void setDragAndDropValidator(const QRegularExpression &re);

    /// Make icons wider so icon tag can be bigger.
    void setWiderIconsEnabled(bool wider);

signals:
    void addButtonClicked();
    void itemSelectionChanged();
    void dropped(const QString &text, int row);
    void itemCheckStateChanged(int row, bool checked);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void nextPreviousItem(int d);

    void onPushButtonUpClicked();
    void onPushButtonDownClicked();
    void onPushButtonRemoveClicked();
    void onPushButtonAddClicked();

    void onListWidgetItemsCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
    void onListWidgetItemsItemSelectionChanged();
    void onListWidgetItemsItemChanged(QListWidgetItem *item);

    struct ItemWidgetPair {
        ItemWidgetPair() {}
        explicit ItemWidgetPair(const ItemPtr &item, bool checked)
            : item(item)
            , lastCheckedState(checked)
        {}

        ItemPtr item;
        QPointer<QWidget> widget;
        bool lastCheckedState = true;
    };

    QListWidgetItem *listItem(int row) const;
    void setCurrentItemWidget(QWidget *widget);
    QWidget *createWidget(QListWidgetItem *item);
    void removeItem(QListWidgetItem *item);

    Ui::ItemOrderList *ui;
    QMap<QListWidgetItem*, ItemWidgetPair> m_items;

    QRegularExpression m_dragAndDropRe;
};

#endif // ITEMORDERLIST_H
