// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TABWIDGET_H
#define TABWIDGET_H

#include <QHash>
#include <QTabBar>
#include <QWidget>

#include <memory>

class QMainWindow;
class QMimeData;
class QPoint;
class QSettings;
class QStackedWidget;
class QToolBar;
class TabsWidgetInterface;

class TabWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit TabWidget(QWidget *parent = nullptr);

    /** Return path to current group in tree (empty string if tree mode is disabled). */
    QString getCurrentTabPath() const;

    /** Return true only if tree mode is enabled and tab is tab group. */
    bool isTabGroup(const QString &tab) const;

    /** Return true only if tab froup is selected in tree mode. */
    bool isTabGroupSelected() const;

    /** Return current tab. */
    int currentIndex() const;

    QWidget *widget(int tabIndex) const;

    QWidget *currentWidget() const { return widget( currentIndex() ); }

    /** Return number of tabs. */
    int count() const;

    /** Return path of tab in tree or label in tab bar. */
    QString tabName(int tabIndex) const;

    void setTabName(int tabIndex, const QString &tabName);

    void setTabItemCountVisible(bool visible);

    void setTabIcon(const QString &tabName, const QString &icon);

    void insertTab(int tabIndex, QWidget *widget, const QString &tabName);

    void addTab(QWidget *widget, const QString &tabName) { insertTab( count(), widget, tabName); }

    void removeTab(int tabIndex);

    /** Return tab names. */
    QStringList tabs() const;

    void addToolBars(QMainWindow *mainWindow);

    void saveTabInfo();

    void loadTabInfo();

    void updateTabs(QSettings &settings);

    QToolBar *toolBar() const { return m_toolBarCurrent; }

    void setCurrentIndex(int tabIndex);
    void nextTab();
    void previousTab();
    void setTabBarHidden(bool hidden);
    void setTreeModeEnabled(bool enabled);
    void setTabItemCount(const QString &tabName, int itemCount);

    void setTabsOrder(const QStringList &tabs);

signals:
    /// Tabs moved in tab bar.
    void tabMoved(int from, int to);
    /// Tabs moved in tab tree (@a oldPrefix path changed to @a newPrefix).
    void tabsMoved(const QString &oldPrefix, const QString &newPrefix);
    void tabBarMenuRequested(const QPoint &pos, int tab);
    void tabTreeMenuRequested(const QPoint &pos, const QString &groupPath);
    void tabRenamed(const QString &newName, int index);
    void currentChanged(int tabIndex, int oldTabIndex);
    void tabCloseRequested(int);
    void dropItems(const QString &tabName, const QMimeData *data);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void onTabMoved(int from, int to);
    void onTabsMoved(const QString &oldPrefix, const QString &newPrefix, const QList<int> &indexes);
    void onToolBarOrientationChanged(Qt::Orientation orientation);
    void onTreeItemClicked();

    void createTabBar();
    void createTabTree();
    void updateToolBar();
    void updateTabItemCount(const QString &name);
    QString itemCountLabel(const QString &name);

    void setTreeModeEnabled(bool enabled, const QStringList &tabs);

    QToolBar *m_toolBar;
    QToolBar *m_toolBarTree;
    QToolBar *m_toolBarCurrent;
    TabsWidgetInterface *m_tabs = nullptr;
    QTabBar *m_tabBar = nullptr;
    QStackedWidget *m_stackedWidget;
    bool m_hideTabBar;

    QStringList m_collapsedTabs;
    QHash<QString, int> m_tabItemCounters;

    bool m_showTabItemCount;

    bool m_ignoreCurrentTabChanges = false;
};

#endif // TABWIDGET_H
