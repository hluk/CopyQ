// SPDX-License-Identifier: GPL-3.0-or-later

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

    void setTabIcon(QTreeWidgetItem *item, const QString &icon);
    void setTabIcon(const QString &tabName, const QString &icon) override;

    void insertTab(int index, const QString &path) override;
    void removeTab(int index) override;

    void updateCollapsedTabs(QList<QString> *collapsedTabs) const override;
    void setCollapsedTabs(const QList<QString> &collapsedTabs) override;

    void updateTabIcons(const QHash<QString, QString> &tabIcons) override;

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
