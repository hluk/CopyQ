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

class ItemOrderList : public QWidget
{
    Q_OBJECT

public:
    class Item {
        friend class ItemOrderList;
    public:
        virtual ~Item() = default;
        virtual QVariant data() const = 0;
    private:
        virtual QWidget *createWidget(QWidget *parent) const = 0;
    };

    using ItemPtr = std::shared_ptr<Item>;

    explicit ItemOrderList(QWidget *parent = nullptr);
    ~ItemOrderList();

    void setAddRemoveButtonsVisible(bool visible);

    void clearItems();

    void appendItem(const QString &label, bool checked, bool highlight, const QIcon &icon, const ItemPtr &item);

    void insertItem(const QString &label, bool checked, bool highlight, const QIcon &icon, const ItemPtr &item, int targetRow);

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

    void setCurrentItemHighlight(bool highlight);

    QString itemLabel(int row) const;

    QList<int> selectedRows() const;
    void setSelectedRows(const QList<int> &selectedRows);

    int rowCount() const;

    void setItemWidgetVisible(int row, bool visible);

    void setDragAndDropValidator(const QRegExp &re);

signals:
    void addButtonClicked();
    void itemSelectionChanged();
    void dropped(const QString &text, int row);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void on_pushButtonUp_clicked();
    void on_pushButtonDown_clicked();
    void on_pushButtonRemove_clicked();
    void on_pushButtonAdd_clicked();

    void on_listWidgetItems_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
    void on_listWidgetItems_itemSelectionChanged();

private:
    struct ItemWidgetPair {
        ItemWidgetPair() {}
        explicit ItemWidgetPair(const ItemPtr &item) : item(item) {}

        ItemPtr item;
        QPointer<QWidget> widget;
    };

    QListWidgetItem *listItem(int row) const;
    void setCurrentItemWidget(QWidget *widget);
    void setItemHighlight(QListWidgetItem *item, bool highlight);
    QWidget *createWidget(QListWidgetItem *item);

    Ui::ItemOrderList *ui;
    QMap<QListWidgetItem*, ItemWidgetPair> m_items;

    QRegExp m_dragAndDropRe;
};

#endif // ITEMORDERLIST_H
