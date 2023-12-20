// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ITEMORDERLIST_H
#define ITEMORDERLIST_H

#include <QListWidgetItem>
#include <QPointer>
#include <QWidget>

#include <memory>
#include <unordered_map>

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
    void onPushButtonTopClicked();
    void onPushButtonBottomClicked();
    void onPushButtonRemoveClicked();
    void onPushButtonAddClicked();

    void onListWidgetItemsCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
    void onListWidgetItemsItemSelectionChanged();
    void onListWidgetItemsItemChanged(QListWidgetItem *item);

    void moveTab(int row, int targetRow);

    struct ItemWidgetPair {
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
    std::unordered_map<int, ItemWidgetPair> m_items;
    int m_lastItemId = 0;

    QRegularExpression m_dragAndDropRe;
};

#endif // ITEMORDERLIST_H
