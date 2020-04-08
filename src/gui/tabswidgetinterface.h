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

#ifndef TABSWIDGETINTERFACE_H
#define TABSWIDGETINTERFACE_H

class QString;
class QStringList;

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

    virtual void updateTabIcon(const QString &tabName) = 0;

    virtual void insertTab(int tabIndex, const QString &tabName) = 0;

    /** Remove tab with given @a index. */
    virtual void removeTab(int index) = 0;

    /** Change tab index of tab. */
    virtual void moveTab(int from, int to) = 0;

    virtual void updateCollapsedTabs(QStringList *collapsedTabs) const = 0;

    virtual void setCollapsedTabs(const QStringList &collapsedTabs) = 0;

    virtual void updateTabIcons() = 0;

    virtual void nextTab() = 0;
    virtual void previousTab() = 0;

    virtual void setCurrentTab(int index) = 0;

    virtual void adjustSize() = 0;

    TabsWidgetInterface(const TabsWidgetInterface &) = delete;
    TabsWidgetInterface &operator=(const TabsWidgetInterface &) = delete;
};

#endif // TABSWIDGETINTERFACE_H
