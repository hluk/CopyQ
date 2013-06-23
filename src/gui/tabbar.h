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

#ifndef TABBAR_H
#define TABBAR_H

#include <QTabBar>

class QModelIndex;
class QMouseEvent;
class QTreeWidget;
class QTreeWidgetItem;
class QPoint;

class TabBar : public QTabBar
{
    Q_OBJECT

public:
    explicit TabBar(QWidget *parent = NULL);

    void setTreeModeEnabled(bool enabled);

    bool isTreeModeEnabled() const;

    QSize minimumSizeHint() const;
    QSize sizeHint() const;

    void refresh();

    /** Return current tab (-1 if current is group in tree). */
    int getCurrentTab() const;

public slots:
    void nextTreeItem();
    void previousTreeItem();

protected:
    void tabInserted(int index);
    void tabRemoved(int index);

    void mousePressEvent(QMouseEvent *event);
    void resizeEvent(QResizeEvent *event);

signals:
    void tabMenuRequested(const QPoint &pos, int tab);
    void treeItemSelected(bool isGroup);

private slots:
    void onCurrentChanged(int index);
    void onTreeCurrentChanged(const QModelIndex &index);

private:
    QTreeWidgetItem *findTreeItem(int index);
    void insertTabToTree(int index);
    void removeTabFromTree(int index);
    void updateTreeSize();

    QTreeWidget *m_treeWidget;
};

#endif // TABBAR_H
