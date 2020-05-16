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

#ifndef ACTIONHANDLER_H
#define ACTIONHANDLER_H

#include <QObject>
#include <QSet>

class Action;
class NotificationDaemon;
class ActionTableModel;

class ActionHandler final : public QObject
{
    Q_OBJECT
public:
    explicit ActionHandler(NotificationDaemon *notificationDaemon, QObject *parent);

    int runningActionCount() const { return m_actions.size() - m_internalActions.size(); }

    void showProcessManagerDialog(QWidget *parent);

    void addFinishedAction(const QString &name);

    QVariantMap actionData(int id) const;
    void setActionData(int id, const QVariantMap &data);

    void internalAction(Action *action);
    bool isInternalActionId(int id) const;

    /** Execute action. */
    void action(Action *action);

    void terminateAction(int id);

private:
    /** Delete finished action and its menu item. */
    void closeAction(Action *action);

    void showActionErrors(Action *action, const QString &message, ushort icon);

    NotificationDaemon *m_notificationDaemon;
    ActionTableModel *m_actionModel;
    QHash<int, Action*> m_actions;
    QSet<int> m_internalActions;
    int m_lastActionId = -1;
};

#endif // ACTIONHANDLER_H
