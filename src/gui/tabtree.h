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

#ifndef TABTREE_H
#define TABTREE_H

#include "gui/tabswidgetinterface.h"

#include <QList>
#include <QString>
#include <QTimer>
#include <QTreeWidget>

class QMimeData;
class QTreeWidgetItem;

class TabTree final : public QTreeWidget, public TabsWidgetInterface
{
    Q_OBJECT
public:
    explicit TabTree(QWidget *parent = nullptr);

    QString getCurrentTabPath() const override;

    bool isTabGroup(const QString &tab) const override;

    QString tabName(int tabIndex) const override;
    void setTabName(int tabIndex, const QString &tabName) override;

    void setTabItemCount(const QString &tabName, const QString &itemCount) override;

    void updateTabIcon(const QString &tabName) override;

    void insertTab(int index, const QString &path) override;
    void removeTab(int index) override;
    void moveTab(int from, int to) override;

    void updateCollapsedTabs(QStringList *collapsedTabs) const override;
    void setCollapsedTabs(const QStringList &collapsedTabs) override;

    void updateTabIcons() override;

    void nextTab() override;
    void previousTab() override;

    void setCurrentTab(int index) override;

    void adjustSize() override;

    /** Return item with given @a index or nullptr if it doesn't exist. */
    QTreeWidgetItem *findTreeItem(int index) const;

    /** Return item with given @a path or nullptr if it doesn't exist. */
    QTreeWidgetItem *findTreeItem(const QString &path) const;

    /** Get tab index for @a item (-1 if it's a group). */
    int getTabIndex(const QTreeWidgetItem *item) const;

    /** Return path to item in tree. */
    QString getTabPath(const QTreeWidgetItem *item) const;

    /** Return true only if tab is tab group. */
    bool isTabGroup(const QTreeWidgetItem *item) const;

    /** Return true only if tab is tab group and is empty. */
    bool isEmptyTabGroup(const QTreeWidgetItem *item) const;

    QSize sizeHint() const override;

signals:
    void currentTabChanged(int index);
    void tabTreeMenuRequested(const QPoint &pos, const QString &groupPath);
    void tabsMoved(const QString &oldPrefix, const QString &newPrefix, const QList<int> &indexes);
    void dropItems(const QString &tabName, const QMimeData *data);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void rowsInserted(const QModelIndex &parent, int start, int end) override;
    void showEvent(QShowEvent *event) override;

private:
    void onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void updateSize();
    void doUpdateSize();

    void requestTabMenu(QPoint itemPosition, QPoint menuPosition);
    void deleteItem(QTreeWidgetItem *item);

    QTimer m_timerUpdate;
    QList<QTreeWidgetItem*> m_tabs;
};

#endif // TABTREE_H
