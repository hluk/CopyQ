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

#ifndef TABWIDGET_H
#define TABWIDGET_H

#include <QBoxLayout>
#include <QMap>
#include <QWidget>

class QAbstractScrollArea;
class QMainWindow;
class QMimeData;
class QPoint;
class QStackedWidget;
class QToolBar;
class TabBar;
class TabTree;

class TabWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TabWidget(QWidget *parent = NULL);

    /** Return path to current group in tree (empty string if tree mode is disabled). */
    QString getCurrentTabPath() const;

    /** Return true only if tree mode is enabled and tab is tab group. */
    bool isTabGroup(const QString &tab) const;

    /** Return true only if tree mode is enabled. */
    bool isTreeModeEnabled() const;

    /** Return current tab (-1 if current is group in tree). */
    int currentIndex() const;

    QWidget *widget(int tabIndex);

    QWidget *currentWidget() { return widget( currentIndex() ); }

    /** Return number of tabs. */
    int count() const;

    /** Return path of tab in tree or label in tab bar. */
    QString tabText(int tabIndex) const;

    void setTabText(int tabIndex, const QString &tabName);

    void setTabItemCountVisible(bool visible);

    void updateTabIcon(const QString &tabName);

    void insertTab(int tabIndex, QWidget *widget, const QString &tabText);

    void addTab(QWidget *widget, const QString &tabText) { insertTab( count(), widget, tabText); }

    void removeTab(int tabIndex);

    /** Return tab names. */
    QStringList tabs() const;

    void moveTab(int from, int to);

    void addToolBars(QMainWindow *mainWindow);

    void saveTabInfo();

    void loadTabInfo();

    void updateTabs();

    QAbstractScrollArea *tabTree();

public slots:
    void setCurrentIndex(int tabIndex);
    void nextTab();
    void previousTab();
    void setTabBarHidden(bool hidden);
    void setTreeModeEnabled(bool enabled);
    void setTabItemCount(const QString &tabName, int itemCount);

signals:
    /// Tabs moved in tab bar.
    void tabMoved(int from, int to);
    /// Tabs moved in tab tree (@a oldPrefix path changed to @a newPrefix).
    void tabsMoved(const QString &oldPrefix, const QString &newPrefix);
    void tabMenuRequested(const QPoint &pos, int tab);
    void tabMenuRequested(const QPoint &pos, const QString &groupPath);
    void tabRenamed(const QString &newName, int index);
    void currentChanged(int tabIndex, int oldTabIndex);
    void tabCloseRequested(int);
    void dropItems(const QString &tabName, QDropEvent *event);

protected:
    bool eventFilter(QObject *object, QEvent *event);

private slots:
    void onTreeItemSelected(bool isGroup);
    void onTabMoved(int from, int to);
    void onTabsMoved(const QString &oldPrefix, const QString &newPrefix, const QList<int> &indexes);
    void onToolBarOrientationChanged(Qt::Orientation orientation);

private:
    void createTabBar();
    void createTabTree();
    void updateToolBar();
    void updateTabItemCount(const QString &name);
    void updateSize();
    QString itemCountLabel(const QString &name);

    QToolBar *m_toolBar;
    QToolBar *m_toolBarTree;
    TabBar *m_tabBar;
    TabTree *m_tabTree;
    QStackedWidget *m_stackedWidget;
    bool m_hideTabBar;

    QStringList m_collapsedTabs;
    QMap<QString, int> m_tabItemCounters;

    bool m_showTabItemCount;
};

#endif // TABWIDGET_H
