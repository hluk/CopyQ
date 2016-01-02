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

#ifndef TABBAR_H
#define TABBAR_H

#include <QTabBar>

class QMimeData;
class QModelIndex;
class QMouseEvent;
class QPoint;

class TabBar : public QTabBar
{
    Q_OBJECT

public:
    explicit TabBar(QWidget *parent = NULL);

    void updateTabIcon(const QString &tabName);

    void updateTabIcons();

    void setTabItemCount(const QString &tabName, const QString &itemCount);

signals:
    void tabMenuRequested(const QPoint &pos, int tab);
    void tabRenamed(const QString &newName, int index);
    void dropItems(const QString &tabName, QDropEvent *event);

protected:
    void contextMenuEvent(QContextMenuEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dragMoveEvent(QDragMoveEvent *event);
    void dropEvent(QDropEvent *event);
    void tabInserted(int index);

private slots:
    void onCurrentChanged();

private:
    void updateTabStyle(int index);
};

#endif // TABBAR_H
