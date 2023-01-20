// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TABSWIDGETINTERFACE_H
#define TABSWIDGETINTERFACE_H

#include <QtContainerFwd>

class QString;

class TabsWidgetInterface {
public:
    TabsWidgetInterface() = default;

    virtual ~TabsWidgetInterface() = default;

    /** Return path to current group in tree (empty string if this is not a tree). */
    virtual QString getCurrentTabPath() const = 0;

    /** Return true only if tree mode is enabled and tab is tab group. */
    virtual bool isTabGroup(const QString &tab) const = 0;

    /** Return path of tab in tree or label in tab bar. */
    virtual QString tabName(int tabIndex) const = 0;

    virtual void setTabName(int tabIndex, const QString &tabName) = 0;

    virtual void setTabItemCount(const QString &tabName, const QString &itemCount) = 0;

    virtual void setTabIcon(const QString &tabName, const QString &icon) = 0;

    virtual void insertTab(int tabIndex, const QString &tabName) = 0;

    /** Remove tab with given @a index. */
    virtual void removeTab(int index) = 0;

    virtual void updateCollapsedTabs(QList<QString> *collapsedTabs) const = 0;

    virtual void setCollapsedTabs(const QList<QString> &collapsedTabs) = 0;

    virtual void updateTabIcons(const QHash<QString, QString> &tabIcons) = 0;

    virtual void nextTab() = 0;
    virtual void previousTab() = 0;

    virtual void setCurrentTab(int index) = 0;

    virtual void adjustSize() = 0;

    TabsWidgetInterface(const TabsWidgetInterface &) = delete;
    TabsWidgetInterface &operator=(const TabsWidgetInterface &) = delete;
};

#endif // TABSWIDGETINTERFACE_H
