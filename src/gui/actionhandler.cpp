/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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
#include "gui/actiondialog.h"
#include "gui/processmanagerdialog.h"
#include "gui/clipboardbrowser.h"
#include "gui/mainwindow.h"
#include "item/serialize.h"

#include <QDateTime>
#include <QModelIndex>

#include <cmath>

ActionHandler::ActionHandler(MainWindow *mainWindow)
    : QObject(mainWindow)
    , m_wnd(mainWindow)
    , m_actionCounter(0)
    , m_activeActionDialog(new ProcessManagerDialog(mainWindow))
{
    Q_ASSERT(mainWindow);
}

ActionDialog *ActionHandler::createActionDialog(const QStringList &tabs)
{
    auto actionDialog = new ActionDialog(m_wnd);
    actionDialog->setAttribute(Qt::WA_DeleteOnClose, true);
    actionDialog->setOutputTabs(tabs, QString());
    actionDialog->setCommand(m_lastActionDialogCommand);

    connect( actionDialog, SIGNAL(accepted(Action*)),
             this, SLOT(action(Action*)) );
    connect( actionDialog, SIGNAL(closed(ActionDialog*)),
             this, SLOT(actionDialogClosed(ActionDialog*)) );

    return actionDialog;
}

bool ActionHandler::hasRunningAction() const
{
    return m_actionCounter > 0;
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

void ActionHandler::action(Action *action)
{
    action->setParent(this);

    const auto id = ++m_lastActionId;
    action->setId(id);
    m_actions.insert(id, action);

    m_lastAction = action;

    connect( action, SIGNAL(newItems(QStringList, QString, QString)),
             this, SLOT(addItems(QStringList, QString, QString)) );
    connect( action, SIGNAL(newItem(QByteArray, QString, QString)),
             this, SLOT(addItem(QByteArray, QString, QString)) );
    connect( action, SIGNAL(changeItem(QByteArray, QString, QModelIndex)),
             this, SLOT(changeItem(QByteArray, QString, QModelIndex)) );
    connect( action, SIGNAL(actionStarted(Action*)),
             this, SLOT(actionStarted(Action*)) );
    connect( action, SIGNAL(actionFinished(Action*)),
             this, SLOT(closeAction(Action*)) );
    connect( action, SIGNAL(actionError(Action*)),
             this, SLOT(closeAction(Action*)) );

    ++m_actionCounter;

    if ( !action->outputFormat().isEmpty() && action->outputTab().isEmpty() )
        action->setOutputTab(m_currentTabName);

    m_activeActionDialog->actionAboutToStart(action);
    COPYQ_LOG( QString("Executing: %1").arg(action->command()) );
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

    QString msg;

    QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information;

    if ( action->actionFailed() ) {
        msg = tr("Error: %1\n").arg(action->errorString()) + action->errorOutput();
        icon = QSystemTrayIcon::Critical;
    } else if ( action->exitCode() != 0 ) {
        if ( !action->ignoreExitCode() ) {
            msg = tr("Exit code: %1\n").arg(action->exitCode()) + "\n" + action->errorOutput();
            icon = QSystemTrayIcon::Warning;
        }
    } else if ( !action->inputFormats().isEmpty() ) {
        const QModelIndex index = action->index();
        ClipboardBrowser *c = m_wnd->browserForItem(index);
        if (c) {
            QStringList removeFormats = action->inputFormats();
            removeFormats.removeAll( action->outputFormat() );

            if ( !removeFormats.isEmpty() )
                c->model()->setData(index, removeFormats, contentType::removeFormats);
        }
    }

    if ( !msg.isEmpty() ) {
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
        const auto lineNumberWidth = std::log10(lines.size()) + 1;
        for (const auto &line : lines)
            msg.append(QString("\n%1. %2").arg(++lineNumber, lineNumberWidth).arg(line));

        log(title + "\n" + msg);
        m_wnd->showMessage(title, msg, icon);
    }

    m_activeActionDialog->actionFinished(action);
    Q_ASSERT(m_actionCounter > 0);
    --m_actionCounter;

    emit runningActionsCountChanged();

    action->deleteLater();
}

void ActionHandler::actionDialogClosed(ActionDialog *dialog)
{
    m_lastActionDialogCommand = dialog->command();
}

void ActionHandler::addItems(const QStringList &items, const QString &format, const QString &tabName)
{
    ClipboardBrowser *c = tabName.isEmpty() ? m_wnd->browser() : m_wnd->tab(tabName);
    for (const auto &item : items)
        c->add( createDataMap(format, item) );

    if (m_lastAction) {
        if (m_lastAction == sender())
            c->setCurrent(items.size() - 1);
        m_lastAction = nullptr;
    }
}

void ActionHandler::addItem(const QByteArray &data, const QString &format, const QString &tabName)
{
    ClipboardBrowser *c = tabName.isEmpty() ? m_wnd->browser() : m_wnd->tab(tabName);
    c->add( createDataMap(format, data) );

    if (m_lastAction) {
        if (m_lastAction == sender())
            c->setCurrent(0);
        m_lastAction = nullptr;
    }
}

void ActionHandler::changeItem(const QByteArray &data, const QString &format, const QModelIndex &index)
{
    ClipboardBrowser *c = m_wnd->browserForItem(index);
    if (c == nullptr)
        return;

    QVariantMap dataMap;
    if (format == mimeItems)
        deserializeData(&dataMap, data);
    else
        dataMap.insert(format, data);
    c->model()->setData(index, dataMap, contentType::updateData);
}
