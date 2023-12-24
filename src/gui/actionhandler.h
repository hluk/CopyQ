// SPDX-License-Identifier: GPL-3.0-or-later

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

    void setMaxRowCount(uint rows);

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
