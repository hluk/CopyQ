/*
    Copyright (c) 2018, Lukas Holecek <hluk@email.cz>

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

#include "common/command.h"

#include <QDateTime>
#include <QMenu>
#include <QObject>
#include <QPointer>
#include <QMap>
#include <QSet>

class Action;
class ActionDialog;
class ProcessManagerDialog;
class QDialog;
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

    int runningActionCount() const { return m_actions.size() - m_internalActions.size(); }

    /** Open dialog with active commands. */
    void showProcessManagerDialog();

    void addFinishedAction(const QString &name);

    QVariantMap actionData(int id) const;
    void setActionData(int id, const QVariantMap &data);

    void internalAction(Action *action);
    bool isInternalActionId(int id) const;

public slots:
    /** Execute action. */
    void action(Action *action);

signals:
    /** Emitted new action starts or ends. */
    void runningActionsCountChanged();

private slots:
    /** Called after action was started (creates menu item to kill it). */
    void actionStarted(Action *action);

    /** Delete finished action and its menu item. */
    void closeAction(Action *action);

private:
    void showActionErrors(Action *action, const QString &message, ushort icon);

    MainWindow *m_wnd;
    ProcessManagerDialog *m_activeActionDialog;
    QHash<int, Action*> m_actions;
    QSet<int> m_internalActions;
    int m_lastActionId = -1;
};

#endif // ACTIONHANDLER_H
