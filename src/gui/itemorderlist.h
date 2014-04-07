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

#ifndef ITEMORDERLIST_H
#define ITEMORDERLIST_H

#include <QMap>
#include <QListWidgetItem>
#include <QWidget>

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
    explicit ItemOrderList(QWidget *parent = 0);
    ~ItemOrderList();

    void setAddMenu(QMenu *menu);

    bool hasAddMenu() const;

    void clearItems();

    void appendItem(const QString &label, bool checked, const QIcon &icon, QWidget *widget);

    QWidget *itemWidget(int row) const;

    int itemCount() const;

    bool isItemChecked(int row) const;

    void updateIcons();

    void setCurrentItem(int row);

    void setCurrentItemIcon(const QIcon &icon);

    void setCurrentItemLabel(const QString &label);

    QString itemLabel(int row) const;

    QList<int> selectedRows() const;
    void setSelectedRows(const QList<int> &selectedRows);

    int rowCount() const;

    void setItemWidgetVisible(int row, bool visible);

signals:
    void addButtonClicked(QAction *action);
    void itemSelectionChanged();

private slots:
    void on_pushButtonUp_clicked();
    void on_pushButtonDown_clicked();
    void on_pushButtonRemove_clicked();
    void on_toolButtonAdd_triggered(QAction *action);

    void on_listWidgetItems_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
    void on_listWidgetItems_itemSelectionChanged();

private:
    QListWidgetItem *item(int row) const;
    void setCurrentItemWidget(QWidget *widget);

    Ui::ItemOrderList *ui;
    QMap<QListWidgetItem*, QScrollArea*> m_itemWidgets;
};

#endif // ITEMORDERLIST_H
