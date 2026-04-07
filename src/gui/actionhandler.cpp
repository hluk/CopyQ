// SPDX-License-Identifier: GPL-3.0-or-later

#include "actionhandler.h"

#include "common/appconfig.h"
#include "common/action.h"
#include "common/actiontablemodel.h"
#include "common/commandstatus.h"
#include "common/common.h"
#include "common/display.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/textdata.h"
#include "platform/platformnativeinterface.h"
#include "gui/actionhandlerdialog.h"
#include "gui/icons.h"
#include "gui/notification.h"
#include "gui/notificationdaemon.h"
#include "gui/mainwindow.h"
#include "item/serialize.h"

#include <QDialog>
#include <QTimer>

#include <cmath>

namespace {

QString actionDescription(const Action &action)
{
    const auto name = action.name();
    if ( !name.isEmpty() )
        return QStringLiteral("Command \"%1\"").arg(name);

    return action.commandLine();
}

} // namespace

QStringList ActionHandler::copyqStats() const
{
    QStringList lines;
    for (auto it = m_actions.constBegin(); it != m_actions.constEnd(); ++it) {
        constexpr int maxDescriptionLength = 150;
        QString desc = actionDescription(*it.value()).simplified();
        desc.replace(QLatin1Char('\''), QLatin1Char('"'));
        if (desc.size() > maxDescriptionLength)
            desc = desc.left(maxDescriptionLength) + QStringLiteral("...");

        const QList<qint64> pids = it.value()->processIds();
        QStringList pidParts;
        for (const qint64 pid : pids) {
            const qint64 rss = platformNativeInterface()->processResidentMemoryBytes(pid);
            if (rss >= 0) {
                pidParts.append(QStringLiteral("pid=%1 rss=%2 (%3)")
                    .arg(pid).arg(rss).arg(formatDataSize(rss)));
            } else {
                pidParts.append(QStringLiteral("pid=%1").arg(pid));
            }
        }
        lines.append( QStringLiteral("ACTION '%1' | %2").arg(desc, pidParts.join(QStringLiteral(", "))) );
    }
    return lines;
}

ActionHandler::ActionHandler(NotificationDaemon *notificationDaemon, QObject *parent)
    : QObject(parent)
    , m_notificationDaemon(notificationDaemon)
    , m_actionModel(new ActionTableModel(this))
{
}

void ActionHandler::setMaxRowCount(uint rows)
{
    m_actionModel->setMaxRowCount(rows);
}

void ActionHandler::showProcessManagerDialog(QWidget *parent)
{
    auto dialog = new ActionHandlerDialog(this, m_actionModel, parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->show();
}

void ActionHandler::addFinishedAction(const QString &name)
{
    m_actionModel->actionFinished(name);
}

QVariantMap ActionHandler::actionData(int id) const
{
    const auto action = m_actions.value(id);
    return action ? action->data() : QVariantMap();
}

void ActionHandler::setActionData(int id, const QVariantMap &data)
{
    const auto action = m_actions.value(id);
    if (action)
        action->setData(data);
}

void ActionHandler::internalAction(Action *action)
{
    this->action(action);
    if ( m_actions.contains(action->id()) )
        m_internalActions.insert(action->id());
}

bool ActionHandler::isInternalActionId(int id) const
{
    return m_internalActions.contains(id);
}

void ActionHandler::action(Action *action)
{
    action->setParent(this);

    connect( action, &Action::actionStarted,
             m_actionModel, &ActionTableModel::actionStarted );
    connect( action, &Action::actionFinished,
             this, &ActionHandler::closeAction );

    const int id = m_actionModel->actionAboutToStart(action);
    action->setId(id);
    m_actions.insert(id, action);

    COPYQ_LOG( QStringLiteral("Executing: %1").arg(actionDescription(*action)) );
    action->start();
}

void ActionHandler::terminateAction(int id)
{
    Action *action = m_actions.value(id);
    if (!action || !action->isRunning())
        return;

    action->requestTerminate();
    const int terminateTimeout =
        AppConfig().option<Config::terminate_action_timeout_ms>();
    QTimer::singleShot(terminateTimeout, action, &Action::requestKill);
}

Action *ActionHandler::findAction(int id) const
{
    auto it = m_actions.find(id);
    return it != m_actions.end() ? it.value() : nullptr;
}

void ActionHandler::closeAction(Action *action)
{
    m_actions.remove(action->id());
    const bool isInternal = m_internalActions.remove(action->id());

    if ( action->actionFailed() ) {
        const auto msg = tr("Error: %1").arg(action->errorString());
        showActionErrors(action, msg, IconCircleExclamation);
#ifdef Q_OS_WIN
    // FIXME: Ignore specific exit code for clipboard monitor on Windows when logging out.
    } else if ( action->exitCode() == 1073807364 ) {
        COPYQ_LOG( QStringLiteral("Exit code %1 (on logout?) with command: %2")
                   .arg(action->exitCode())
                   .arg(action->commandLine()) );
#elif defined(Q_OS_UNIX)
    // Ignore SIGINT (2) exit code for internal actions.
    } else if ( isInternal && action->exitCode() == 128 + 2 ) {
        COPYQ_LOG( QStringLiteral("Internal action interrupted on SIGINT: %1")
                  .arg(actionDescription(*action)) );
#endif
    } else if ( isInternal && action->exitCode() == CommandStop ) {
        COPYQ_LOG( QStringLiteral("Internal action interrupted: %1")
                  .arg(actionDescription(*action)) );
    } else if ( action->exitCode() != 0 ) {
        const auto msg = tr("Exit code: %1").arg(action->exitCode());
        showActionErrors(action, msg, IconCircleXmark);
    }

    m_actionModel->actionFinished(action);
    Q_ASSERT(runningActionCount() >= 0);

    action->deleteLater();
}

void ActionHandler::showActionErrors(Action *action, const QString &message, ushort icon)
{
    m_actionModel->actionFailed(action, message);
#if QT_VERSION >= QT_VERSION_CHECK(6,10,0)
    QtPrivate::QHashCombine hash(0);
#else
    QtPrivate::QHashCombine hash;
#endif
    const auto notificationId = QString::number( hash(hash(0, action->commandLine()), message) );
    if ( m_notificationDaemon->findNotification(notificationId) )
        return;

    auto msg = message;

    if ( !action->errorOutput().isEmpty() )
        msg.append( "\n" + action->errorOutput() );

    const int maxWidthPoints =
            AppConfig().option<Config::notification_maximum_width>();
    const QString command = action->commandLine();
    const QString name = action->name().isEmpty()
            ? command.simplified()
            : action->name();
    const QString format = tr("Command %1").arg(quoteString("%1"));
    const QString title = elideText(name, QFont(), format, pointsToPixels(maxWidthPoints));

    // Print command with line numbers.
    int lineNumber = 0;
    const auto lines = command.split("\n");
    const auto lineNumberWidth = static_cast<int>(std::log10(lines.size())) + 1;
    for (const auto &line : lines)
        msg.append(QStringLiteral("\n%1. %2").arg(++lineNumber, lineNumberWidth).arg(line));

    auto logMsg = QStringLiteral("%1: %2").arg(title, msg);
    logMsg.replace(QLatin1Char('\n'), QLatin1String(" | "));
    log(logMsg, LogWarning);

    auto notification = m_notificationDaemon->createNotification(notificationId);
    notification->setTitle(title);
    notification->setMessage(msg, Qt::PlainText);
    notification->setIcon(icon);
    notification->setUrgency(Notification::Urgency::High);
    notification->setPersistency(Notification::Persistency::Persistent);
}
