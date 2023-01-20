// SPDX-License-Identifier: GPL-3.0-or-later

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

    void setTabIcon(int index, const QString &icon);
    void setTabIcon(const QString &tabName, const QString &icon) override;

    void insertTab(int index, const QString &tabName) override;
    void removeTab(int index) override;

    void updateCollapsedTabs(QList<QString> *) const override {}
    void setCollapsedTabs(const QList<QString> &) override {}

    void updateTabIcons(const QHash<QString, QString> &tabIcons) override;

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
