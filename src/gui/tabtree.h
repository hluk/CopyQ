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

#ifndef TABTREE_H
#define TABTREE_H

#include <QTreeWidget>

class TabTree : public QTreeWidget
{
    Q_OBJECT
public:
    explicit TabTree(QWidget *parent = NULL);

    /** Create tab in @a path with given @a index. */
    void insertTab(const QString &path, int index);

    /** Remove tab with given @a index. */
    void removeTab(int index);

    /** Return item with given @a index or NULL if it doesn't exist. */
    QTreeWidgetItem *findTreeItem(int index) const;

    /** Return item with given @a path or NULL if it doesn't exist. */
    QTreeWidgetItem *findTreeItem(const QString &path) const;

    /** Get tab index for @a item (-1 if it's a group). */
    int getTabIndex(const QTreeWidgetItem *item) const;

    /** Return path to item in tree (empty string if tree mode is disabled). */
    QString getTabPath(const QTreeWidgetItem *item) const;

    /** Return true only if tab is tab group. */
    bool isTabGroup(const QTreeWidgetItem *item) const;

signals:
    void currentTabChanged(int index);
    void tabMenuRequested(const QPoint &pos, const QString &groupPath);
    void tabMoved(const QString &oldPrefix, const QString &newPrefix, const QString &afterPrefix);

protected:
    void mousePressEvent(QMouseEvent *event);
    void contextMenuEvent(QContextMenuEvent *event);
    void dropEvent(QDropEvent *event);

public slots:
    void setCurrentTabIndex(int index);
    void nextTreeItem();
    void previousTreeItem();

private slots:
    void onCurrentItemChanged(QTreeWidgetItem *current);

private:
    void requestTabMenu(const QPoint &itemPosition, const QPoint &menuPosition);
    void createLabel(QTreeWidgetItem *item);
    void shiftIndexesBetween(int from, int to = -1);
};

#endif // TABTREE_H
