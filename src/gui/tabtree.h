/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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

#include <QString>
#include <QTreeWidget>

class QMimeData;

class TabTree : public QTreeWidget
{
    Q_OBJECT
public:
    explicit TabTree(QWidget *parent = NULL);

    /** Create tab in @a path with given @a index. */
    void insertTab(const QString &path, int index, bool selected);

    /** Remove tab with given @a index. */
    void removeTab(int index);

    /** Return item with given @a index or NULL if it doesn't exist. */
    QTreeWidgetItem *findTreeItem(int index) const;

    /** Return item with given @a path or NULL if it doesn't exist. */
    QTreeWidgetItem *findTreeItem(const QString &path) const;

    /** Get tab index for @a item (-1 if it's a group). */
    int getTabIndex(const QTreeWidgetItem *item) const;

    /** Return path to item in tree. */
    QString getTabPath(const QTreeWidgetItem *item) const;

    /** Return true only if tab is tab group. */
    bool isTabGroup(const QTreeWidgetItem *item) const;

    /** Return true only if tab is tab group and is empty. */
    bool isEmptyTabGroup(const QTreeWidgetItem *item) const;

    /** Change tab index of tab. */
    void moveTab(int from, int to);

    void setTabText(int tabIndex, const QString &tabText);

    void setTabItemCount(const QString &tabName, const QString &itemCount);

    void setCollapsedTabs(const QStringList &collapsedPaths);

    QStringList collapsedTabs() const;

    QSize sizeHint() const;

    void updateTabIcon(const QString &tabName);

    void updateTabIcons();

public slots:
    void setCurrentTabIndex(int index);
    void nextTreeItem();
    void previousTreeItem();

    void onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);

signals:
    void currentTabChanged(int index);
    void tabMenuRequested(const QPoint &pos, const QString &groupPath);
    void tabsMoved(const QString &oldPrefix, const QString &newPrefix, const QList<int> &indexes);
    void dropItems(const QString &tabName, QDropEvent *event);

protected:
    void contextMenuEvent(QContextMenuEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dragMoveEvent(QDragMoveEvent *event);
    void dropEvent(QDropEvent *event);
    bool eventFilter(QObject *obj, QEvent *event);
    void rowsInserted(const QModelIndex &parent, int start, int end);
    void showEvent(QShowEvent *event);

private slots:
    void updateSize();

private:
    void requestTabMenu(const QPoint &itemPosition, const QPoint &menuPosition);
    void shiftIndexesBetween(int from, int to = -1, int how = -1);
    void deleteItem(QTreeWidgetItem *item);
};

#endif // TABTREE_H
