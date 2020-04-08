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

#ifndef TABBAR_H
#define TABBAR_H

#include "gui/tabswidgetinterface.h"

#include <QTabBar>

class QMimeData;
class QModelIndex;
class QMouseEvent;
class QPoint;

class TabBar final : public QTabBar, public TabsWidgetInterface
{
    Q_OBJECT

public:
    explicit TabBar(QWidget *parent = nullptr);

    QString getCurrentTabPath() const override;

    bool isTabGroup(const QString &) const override { return false; }

    QString tabName(int tabIndex) const override;
    void setTabName(int tabIndex, const QString &tabName) override;

    void setTabItemCount(const QString &tabName, const QString &itemCount) override;

    void updateTabIcon(const QString &tabName) override;

    void insertTab(int index, const QString &tabName) override;
    void removeTab(int index) override;
    void moveTab(int from, int to) override;

    void updateCollapsedTabs(QStringList *) const override {}
    void setCollapsedTabs(const QStringList &) override {}

    void updateTabIcons() override;

    void nextTab() override;
    void previousTab() override;

    void setCurrentTab(int index) override;

    virtual void adjustSize() override;

signals:
    void tabBarMenuRequested(const QPoint &pos, int tab);
    void tabRenamed(const QString &newName, int index);
    void dropItems(const QString &tabName, const QMimeData *data);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void tabInserted(int index) override;

private:
    void onCurrentChanged();

    void updateTabStyle(int index);
};

#endif // TABBAR_H
