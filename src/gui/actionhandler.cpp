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

#include "actionhandler.h"

#include "common/appconfig.h"
#include "common/action.h"
#include "common/common.h"
#include "common/contenttype.h"
#include "common/display.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/textdata.h"
#include "gui/icons.h"
#include "gui/notification.h"
#include "gui/processmanagerdialog.h"
#include "gui/clipboardbrowser.h"
#include "gui/mainwindow.h"
#include "item/serialize.h"

#include <QDateTime>
#include <QModelIndex>

#include <cmath>

namespace {

QString actionDescription(const Action &action)
{
    const auto name = action.name();
    if ( !name.isEmpty() )
        return QString("Command \"%1\"").arg(name);

    return action.command();
}

} // namespace

ActionHandler::ActionHandler(MainWindow *mainWindow)
    : QObject(mainWindow)
    , m_wnd(mainWindow)
    , m_activeActionDialog(new ProcessManagerDialog(mainWindow))
{
    Q_ASSERT(mainWindow);
}

void ActionHandler::showProcessManagerDialog()
{
    m_activeActionDialog->show();
}

void ActionHandler::addFinishedAction(const QString &name)
{
    m_activeActionDialog->actionFinished(name);
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

    const auto id = ++m_lastActionId;
    action->setId(id);
    m_actions.insert(id, action);

    connect( action, SIGNAL(actionStarted(Action*)),
             this, SLOT(actionStarted(Action*)) );
    connect( action, SIGNAL(actionFinished(Action*)),
             this, SLOT(closeAction(Action*)) );

    m_activeActionDialog->actionAboutToStart(action);
    COPYQ_LOG( QString("Executing: %1").arg(actionDescription(*action)) );
    action->start();
}

void ActionHandler::actionStarted(Action *action)
{
    m_activeActionDialog->actionStarted(action);
    emit runningActionsCountChanged();
}

void ActionHandler::closeAction(Action *action)
{
    m_actions.remove(action->id());
    m_internalActions.remove(action->id());

    if ( action->actionFailed() ) {
        const auto msg = tr("Error: %1").arg(action->errorString());
        showActionErrors(action, msg, IconExclamationCircle);
    } else if ( action->exitCode() != 0 ) {
        const auto msg = tr("Exit code: %1").arg(action->exitCode());
        showActionErrors(action, msg, IconTimesCircle);
    }

    m_activeActionDialog->actionFinished(action);
    Q_ASSERT(runningActionCount() >= 0);

    emit runningActionsCountChanged();

    action->deleteLater();
}

void ActionHandler::showActionErrors(Action *action, const QString &message, ushort icon)
{
    auto msg = message;

    if ( !action->errorOutput().isEmpty() )
        msg.append( "\n" + action->errorOutput() );

    const int maxWidthPoints =
            AppConfig().option<Config::notification_maximum_width>();
    const QString command = action->command()
            .replace("copyq eval --", "copyq:");
    const QString name = action->name().isEmpty()
            ? QString(command).replace('\n', " ")
            : action->name();
    const QString format = tr("Command %1").arg(quoteString("%1"));
    const QString title = elideText(name, QFont(), format, pointsToPixels(maxWidthPoints));

    // Print command with line numbers.
    int lineNumber = 0;
    const auto lines = command.split("\n");
    const auto lineNumberWidth = static_cast<int>(std::log10(lines.size())) + 1;
    for (const auto &line : lines)
        msg.append(QString("\n%1. %2").arg(++lineNumber, lineNumberWidth).arg(line));

    log(title + "\n" + msg);

    auto notification = m_wnd->createNotification();
    notification->setTitle(title);
    notification->setMessage(msg);
    notification->setIcon(icon);
}
