/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

#ifndef ACTIONHANDLER_H
#define ACTIONHANDLER_H

#include <QMenu>
#include <QObject>
#include <QPointer>
#include <QMap>

class Action;
class ActionDialog;
class ClipboardBrowser;
class MainWindow;
class QModelIndex;

/**
 * Creates action dialog and handles actions created by the dialog.
 */
class ActionHandler : public QObject
{
    Q_OBJECT
public:
    explicit ActionHandler(MainWindow *mainWindow);

    /**
     * Create new action dialog.
     * Dialog is destroyed when closed.
     */
    ActionDialog *createActionDialog(const QStringList &tabs);

    /** Menu with items to kill actions. */
    QMenu &commandMenu() { return m_commandMenu; }

    /** Tray menu with items to kill actions. */
    QMenu &commandTrayMenu() { return m_commandTrayMenu; }

    bool hasRunningAction() const;

    QByteArray getActionData(const QByteArray actionId, const QString &format) const;

signals:
    /** Emitted when last action finishes or first action starts. */
    void hasRunningActionChanged();

private slots:
    /** Execute action. */
    void action(Action *action, const QVariantMap &data);

    /** Called after action was started (creates menu item to kill it). */
    void actionStarted(Action *action);

    /** Delete finished action and its menu item. */
    void closeAction(Action *action);

    void addItems(const QStringList &items, const QString &tabName);
    void addItems(const QStringList &items, const QModelIndex &index);
    void addItem(const QByteArray &data, const QString &format, const QString &tabName);
    void addItem(const QByteArray &data, const QString &format, const QModelIndex &index);

    void disableMenusIfEmpty();

private:
    MainWindow *m_wnd;
    QPointer<Action> m_lastAction;
    QMenu m_commandMenu;
    QMenu m_commandTrayMenu;
    int m_lastActionId;
    QMap<QByteArray, QVariantMap> m_actionData;
};

#endif // ACTIONHANDLER_H
