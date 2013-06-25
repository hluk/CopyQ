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

#ifndef TABWIDGET_H
#define TABWIDGET_H

#include <QTabWidget>

class QPoint;
class TabBar;

class TabWidget : public QTabWidget
{
    Q_OBJECT

public:
    explicit TabWidget(QWidget *parent = NULL);

    void refreshTabBar();

    /** Return current tab (-1 if current is group in tree). */
    int getCurrentTab() const;

    /** Return path to current group in tree (empty string if tree mode is disabled). */
    QString getCurrentTabPath() const;

    /** Return true only if tree mode is enabled and tab is tab group. */
    bool isTabGroup(const QString &tab) const;

public slots:
    void nextTab();
    void previousTab();
    void setTabBarDisabled(bool disabled);
    void setTabBarHidden(bool hidden);
    void setTreeModeEnabled(bool enabled);

signals:
    void tabMoved(int from, int to);
    void tabMenuRequested(const QPoint &pos, int tab);
    void tabMenuRequested(const QPoint &pos, const QString &groupPath);

private slots:
    void onTreeItemSelected(bool isGroup);

private:
    TabBar *m_bar;
};

#endif // TABWIDGET_H
