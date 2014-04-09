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

#include "actionhandler.h"

#include "common/action.h"
#include "common/common.h"
#include "common/contenttype.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "gui/actiondialog.h"
#include "gui/clipboardbrowser.h"
#include "gui/mainwindow.h"
#include "item/serialize.h"

#include <QModelIndex>

ActionHandler::ActionHandler(MainWindow *mainWindow)
    : QObject(mainWindow)
    , m_wnd(mainWindow)
    , m_commandMenu(tr("Co&mmands"))
    , m_commandTrayMenu(m_commandMenu.title())
    , m_lastActionId(0)
    , m_actionData()
{
    Q_ASSERT(mainWindow);
    m_commandMenu.setEnabled(false);
    m_commandTrayMenu.menuAction()->setVisible(false);
}

ActionDialog *ActionHandler::createActionDialog(const QStringList &tabs)
{
    ActionDialog *actionDialog = new ActionDialog(m_wnd);
    actionDialog->setAttribute(Qt::WA_DeleteOnClose, true);
    actionDialog->setOutputTabs(tabs, QString());

    connect( actionDialog, SIGNAL(accepted(Action*,QVariantMap)),
             this, SLOT(action(Action*,QVariantMap)) );

    return actionDialog;
}

bool ActionHandler::hasRunningAction() const
{
    return !m_commandMenu.isEmpty();
}

QByteArray ActionHandler::getActionData(const QByteArray actionId, const QString &format) const
{
    const QVariantMap dataMap = m_actionData.value(actionId);
    return format == "?" ? QStringList(dataMap.keys()).join("\n").toUtf8() + '\n'
                         : dataMap.value(format).toByteArray();
}

void ActionHandler::action(Action *action, const QVariantMap &data)
{
    action->setParent(this);

    m_lastAction = action;

    connect( action, SIGNAL(newItems(QStringList, QString)),
             this, SLOT(addItems(QStringList, QString)) );
    connect( action, SIGNAL(newItems(QStringList, QModelIndex)),
             this, SLOT(addItems(QStringList, QModelIndex)) );
    connect( action, SIGNAL(newItem(QByteArray, QString, QString)),
             this, SLOT(addItem(QByteArray, QString, QString)) );
    connect( action, SIGNAL(newItem(QByteArray, QString, QModelIndex)),
             this, SLOT(addItem(QByteArray, QString, QModelIndex)) );
    connect( action, SIGNAL(actionStarted(Action*)),
             this, SLOT(actionStarted(Action*)) );
    connect( action, SIGNAL(actionFinished(Action*)),
             this, SLOT(closeAction(Action*)) );
    connect( action, SIGNAL(actionError(Action*)),
             this, SLOT(closeAction(Action*)) );

    // Set data for action (can be accessed using 'data' command).
    const QByteArray actionId = QByteArray::number(++m_lastActionId);
    qputenv("COPYQ_ACTION_ID", actionId);
    m_actionData.insert(actionId, data);
    action->setProperty("COPYQ_ACTION_ID", actionId);

    log( tr("Executing: %1").arg(action->command()) );
    action->start();
}

void ActionHandler::actionStarted(Action *action)
{
    bool hadRunningAction = hasRunningAction();

    QString text = tr("KILL") + " " + action->command();
    QString tooltip = tr("<b>COMMAND:</b>") + "<br />" + escapeHtml(text) + "<br />" +
                      tr("<b>INPUT:</b>") + "<br />" +
                      escapeHtml( QString::fromUtf8(action->input()) );

    QAction *act = new QAction(action);
    act->setToolTip(tooltip);
    act->setWhatsThis(tooltip);
    act->setText( elideText(text, act->font(), QString(), true) );

    connect( act, SIGNAL(triggered()),
             action, SLOT(terminate()) );
    connect( act, SIGNAL(destroyed()),
             this, SLOT(disableMenusIfEmpty()) );

    m_commandMenu.addAction(act);
    m_commandTrayMenu.addAction(act);

    if (!hadRunningAction) {
        m_commandMenu.setEnabled(true);
        m_commandTrayMenu.menuAction()->setVisible(true);
        emit hasRunningActionChanged();
    }
}

void ActionHandler::closeAction(Action *action)
{
    QString msg;

    QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information;

    if ( action->actionFailed() || action->exitStatus() != QProcess::NormalExit ) {
        msg += tr("Error: %1\n").arg(action->errorString()) + action->errorOutput();
        icon = QSystemTrayIcon::Critical;
    } else if ( action->exitCode() != 0 ) {
        msg += tr("Exit code: %1\n").arg(action->exitCode()) + action->errorOutput();
        icon = QSystemTrayIcon::Warning;
    } else if ( !action->inputFormats().isEmpty() ) {
        QModelIndex index = action->index();
        ClipboardBrowser *c = m_wnd->browserForItem(index);
        if (c) {
            QStringList removeFormats;
            if ( action->inputFormats().size() > 1 ) {
                QVariantMap data;
                deserializeData( &data, action->input() );
                removeFormats = data.keys();
            } else {
                removeFormats.append( action->inputFormats()[0] );
            }

            removeFormats.removeAll( action->outputFormat() );
            if ( !removeFormats.isEmpty() )
                c->model()->setData(index, removeFormats, contentType::removeFormats);
        }
    }

    if ( !msg.isEmpty() )
        m_wnd->showMessage( tr("Command %1").arg(quoteString(action->command())), msg, icon );

    m_actionData.remove( action->property("COPYQ_ACTION_ID").toByteArray() );
    action->deleteLater();
}

void ActionHandler::addItems(const QStringList &items, const QString &tabName)
{
    ClipboardBrowser *c = tabName.isEmpty() ? m_wnd->browser() : m_wnd->createTab(tabName);
    ClipboardBrowser::Lock lock(c);
    foreach (const QString &item, items)
        c->add(item);

    if (m_lastAction) {
        if (m_lastAction == sender())
            c->setCurrent(items.size() - 1);
        m_lastAction = NULL;
    }
}

void ActionHandler::addItems(const QStringList &items, const QModelIndex &index)
{
    ClipboardBrowser *c = m_wnd->browserForItem(index);
    if (c == NULL)
        return;

    QVariantMap dataMap;
    dataMap.insert( mimeText, items.join(QString()).toUtf8() );
    c->model()->setData(index, dataMap, contentType::updateData);
}

void ActionHandler::addItem(const QByteArray &data, const QString &format, const QString &tabName)
{
    ClipboardBrowser *c = tabName.isEmpty() ? m_wnd->browser() : m_wnd->createTab(tabName);
    c->add( createDataMap(format, data) );

    if (m_lastAction) {
        if (m_lastAction == sender())
            c->setCurrent(0);
        m_lastAction = NULL;
    }
}

void ActionHandler::addItem(const QByteArray &data, const QString &format, const QModelIndex &index)
{
    ClipboardBrowser *c = m_wnd->browserForItem(index);
    if (c == NULL)
        return;

    QVariantMap dataMap;
    if (format == mimeItems)
        deserializeData(&dataMap, data);
    else
        dataMap.insert(format, data);
    c->model()->setData(index, dataMap, contentType::updateData);
}

void ActionHandler::disableMenusIfEmpty()
{
    if (!hasRunningAction()) {
        m_commandMenu.setEnabled(false);
        m_commandTrayMenu.menuAction()->setVisible(false);
        emit hasRunningActionChanged();
    }
}
