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

#include "clipboardserver.h"

#include "common/action.h"
#include "common/clientsocket.h"
#include "common/client_server.h"
#include "common/commandstatus.h"
#include "common/display.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/shortcuts.h"
#include "common/sleeptimer.h"
#include "gui/clipboardbrowser.h"
#include "gui/commanddialog.h"
#include "gui/configtabshortcuts.h"
#include "gui/iconfactory.h"
#include "gui/mainwindow.h"
#include "item/itemfactory.h"
#include "item/serialize.h"
#include "scriptable/scriptableproxy.h"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QSessionManager>

#ifdef NO_GLOBAL_SHORTCUTS
class QxtGlobalShortcut {};
#else
#include "../qxt/qxtglobalshortcut.h"
#endif

namespace {

bool hasScriptCommand(const QVector<Command> &commands)
{
    for (const auto &command : commands) {
        if (command.type() == CommandType::Script)
            return true;
    }

    return false;
}

} // namespace

ClipboardServer::ClipboardServer(QApplication *app, const QString &sessionName)
    : QObject()
    , App("Server", app, sessionName)
    , m_wnd(nullptr)
    , m_shortcutActions()
    , m_ignoreKeysTimer()
{
    const QString serverName = clipboardServerName();
    auto server = new Server(serverName, this);

    if ( server->isListening() ) {
        ::createSessionMutex();
        restoreSettings(true);
        COPYQ_LOG("Server \"" + serverName + "\" started.");
    } else {
        restoreSettings(false);
        COPYQ_LOG("Server \"" + serverName + "\" already running!");
        log( tr("CopyQ server is already running."), LogWarning );
        exit(0);
        return;
    }

    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    QApplication::setQuitOnLastWindowClosed(false);

    m_itemFactory = new ItemFactory(this);
    m_wnd = new MainWindow(m_itemFactory);

    m_itemFactory->loadPlugins();
    if ( !m_itemFactory->hasLoaders() )
        log("No plugins loaded", LogNote);

    m_wnd->loadSettings();
    m_wnd->setCurrentTab(0);
    m_wnd->enterBrowseMode();

    connect( server, SIGNAL(newConnection(ClientSocketPtr)),
             this, SLOT(onClientNewConnection(ClientSocketPtr)) );

    connect( qApp, SIGNAL(aboutToQuit()),
             this, SLOT(onAboutToQuit()));

    connect( qApp, SIGNAL(commitDataRequest(QSessionManager&)),
             this, SLOT(onCommitData(QSessionManager&)) );

    connect( qApp, SIGNAL(saveStateRequest(QSessionManager&)),
             this, SLOT(onSaveState(QSessionManager&)) );

    connect( m_wnd, SIGNAL(requestExit()),
             this, SLOT(maybeQuit()) );
    connect( m_wnd, SIGNAL(disableClipboardStoringRequest(bool)),
             this, SLOT(onDisableClipboardStoringRequest(bool)) );

    loadSettings();

    // notify window if configuration changes
    connect( m_wnd, SIGNAL(configurationChanged()),
             this, SLOT(loadSettings()) );

    connect( m_wnd, SIGNAL(commandsSaved()),
             this, SLOT(onCommandsSaved()) );
    onCommandsSaved();

    // run clipboard monitor
    startMonitoring();

    qApp->installEventFilter(this);

    server->start();

    // Ignore global shortcut key presses in any widget.
    m_ignoreKeysTimer.setInterval(100);
    m_ignoreKeysTimer.setSingleShot(true);
}

ClipboardServer::~ClipboardServer()
{
    removeGlobalShortcuts();
    delete m_wnd;
}

void ClipboardServer::stopMonitoring()
{
    if (!m_monitor)
        return;

    COPYQ_LOG("Terminating monitor");

    disconnect( m_monitor, SIGNAL(destroyed()),
                this, SLOT(onMonitorFinished()) );

    for (auto it = m_clients.constBegin(); it != m_clients.constEnd(); ++it) {
        const auto actionId = it.value().proxy->actionId();
        if ( actionId == m_monitor->id() ) {
            const auto client = it.key();
            client->sendMessage(QByteArray(), CommandFinished);
            break;
        }
    }

    m_monitor->waitForFinished(1000);
    m_monitor->terminate();
    m_monitor = nullptr;

    COPYQ_LOG("Monitor terminated");
}

void ClipboardServer::startMonitoring()
{
    if (m_monitor || !m_enableMonitor || !m_wnd->isMonitoringEnabled())
        return;

    COPYQ_LOG("Starting monitor");

    m_monitor = new Action();
    m_monitor->setCommand("copyq monitorClipboard");
    connect( m_monitor, SIGNAL(destroyed()),
             this, SLOT(onMonitorFinished()) );
    m_wnd->runInternalAction(m_monitor);
}

void ClipboardServer::removeGlobalShortcuts()
{
    for (auto it = m_shortcutActions.constBegin(); it != m_shortcutActions.constEnd(); ++it)
        delete it.key();
    m_shortcutActions.clear();
}

void ClipboardServer::onCommandsSaved()
{
    const auto commands = loadEnabledCommands();

#ifndef NO_GLOBAL_SHORTCUTS
    removeGlobalShortcuts();

    QList<QKeySequence> usedShortcuts;

    for (const auto &command : commands) {
        if (command.type() & CommandType::GlobalShortcut) {
            for (const auto &shortcutText : command.globalShortcuts) {
                QKeySequence shortcut(shortcutText, QKeySequence::PortableText);
                if ( !shortcut.isEmpty() && !usedShortcuts.contains(shortcut) ) {
                    usedShortcuts.append(shortcut);
                    createGlobalShortcut(shortcut, command);
                }
            }
        }
    }
#endif

    if ( m_monitor && hasScriptCommand(commands) ) {
        stopMonitoring();
        startMonitoring();
    }
}

void ClipboardServer::onAboutToQuit()
{
    COPYQ_LOG("Closing server.");

    m_wnd->saveTabs();

    stopMonitoring();

    emit terminateClients();
}

void ClipboardServer::onCommitData(QSessionManager &sessionManager)
{
    COPYQ_LOG("Got commit data request from session manager.");

    const bool cancel = sessionManager.allowsInteraction() && !askToQuit();
    sessionManager.release();

    if (cancel) {
        sessionManager.cancel();
    } else {
        m_wnd->saveTabs();

        // WORKAROUND: This is required to exit application from
        //             installer, otherwise main window is only
        //             minimized after this when tray is disabled.
        m_wnd->hide();
        exit();
    }
}

void ClipboardServer::onSaveState(QSessionManager &sessionManager)
{
    COPYQ_LOG("Got save state request from session manager.");

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "copyq", "copyq_no_session");
    const auto sessionNameKey = "session_" + sessionManager.sessionId();
    const auto sessionName = qApp->property("CopyQ_session_name").toString();
    settings.setValue(sessionNameKey, sessionName);

    // Remove last session name from configuration.
    const auto lastSessionIdKey = "last_session_id_for_" + sessionName;
    const auto lastSessionId = settings.value(lastSessionIdKey).toString();
    if ( !lastSessionId.isEmpty() ) {
        const auto lastSessionNameKey = "session_" + lastSessionId;
        settings.remove(lastSessionNameKey);
    }
    settings.setValue(lastSessionIdKey, sessionNameKey);
}

void ClipboardServer::onDisableClipboardStoringRequest(bool disabled)
{
    stopMonitoring();
    if (!disabled)
        startMonitoring();
}

void ClipboardServer::maybeQuit()
{
    m_enableMonitor = false;
    stopMonitoring();

    // Wait a moment for commands to finish.
    for ( int i = 0; i < 50 && hasRunningCommands(); ++i )
        waitFor(50);

    if (askToQuit()) {
        emit terminateClients();
        QCoreApplication::exit();
    } else {
        m_enableMonitor = true;
        startMonitoring();
    }
}

bool ClipboardServer::askToQuit()
{
    if ( !m_wnd->maybeCloseCommandDialog() )
        return false;

    if ( hasRunningCommands() ) {
        QMessageBox messageBox( QMessageBox::Warning, tr("Cancel Active Commands"),
                                tr("Cancel active commands and exit?"), QMessageBox::NoButton,
                                m_wnd );

        messageBox.addButton(tr("Cancel Exiting"), QMessageBox::RejectRole);
        messageBox.addButton(tr("Exit Anyway"), QMessageBox::AcceptRole);

        return messageBox.exec() == QMessageBox::Accepted;
    }

    return true;
}

bool ClipboardServer::hasRunningCommands() const
{
    if ( m_wnd->hasRunningAction() )
        return true;

    for (auto it = m_clients.constBegin(); it != m_clients.constEnd(); ++it) {
        const auto actionId = it.value().proxy->actionId();
        if ( !m_wnd->isInternalActionId(actionId) )
            return true;
    }

    return false;
}

void ClipboardServer::onClientNewConnection(const ClientSocketPtr &client)
{
    auto proxy = new ScriptableProxy(m_wnd, client.get());
    m_clients.insert( client.get(), ClientData(client, proxy) );
    connect( this, &ClipboardServer::terminateClients,
             client.get(), &ClientSocket::close );
    connect( client.get(), &ClientSocket::messageReceived,
             this, &ClipboardServer::onClientMessageReceived );
    connect( client.get(), &ClientSocket::disconnected,
             this, &ClipboardServer::onClientDisconnected );
    connect( client.get(), &ClientSocket::connectionFailed,
             this, &ClipboardServer::onClientConnectionFailed );
    client->start();
}

void ClipboardServer::onClientMessageReceived(
        const QByteArray &message, int messageCode, ClientSocket *client)
{
    Q_UNUSED(client);
    switch (messageCode) {
    case CommandFunctionCall: {
        auto proxy = m_clients.value(client).proxy;
        if (!proxy)
            return;
        const auto result = proxy->callFunction(message);
        client->sendMessage(result, CommandFunctionCallReturnValue);
        break;
    }
    default:
        log(QString("Unhandled command status: %1").arg(messageCode));
        break;
    }
}

void ClipboardServer::onClientDisconnected(ClientSocket *client)
{
    m_clients.remove(client);
}

void ClipboardServer::onClientConnectionFailed(ClientSocket *client)
{
    log("Client connection failed", LogWarning);
    m_clients.remove(client);
}

void ClipboardServer::onMonitorFinished()
{
    COPYQ_LOG("Monitor finished");
    stopMonitoring();
    startMonitoring();
}

void ClipboardServer::createGlobalShortcut(const QKeySequence &shortcut, const Command &command)
{
#ifdef NO_GLOBAL_SHORTCUTS
    Q_UNUSED(shortcut);
    Q_UNUSED(command);
#else
    auto s = new QxtGlobalShortcut(shortcut, this);
    if (!s->isValid()) {
        log(QString("Failed to set global shortcut \"%1\" for command \"%2\".")
            .arg(shortcut.toString(),
                 command.name),
            LogWarning);
        delete s;
        return;
    }

    connect( s, SIGNAL(activated(QxtGlobalShortcut*)),
             this, SLOT(shortcutActivated(QxtGlobalShortcut*)) );

    m_shortcutActions[s] = command;
#endif
}

bool ClipboardServer::eventFilter(QObject *object, QEvent *ev)
{
    const QEvent::Type type = ev->type();

    if ( m_ignoreKeysTimer.isActive()
         && (type == QEvent::KeyPress
             || type == QEvent::Shortcut
             || type == QEvent::ShortcutOverride) )
    {
        ev->accept();
        return true;
    }

    if ( type == QEvent::KeyPress
         || type == QEvent::Shortcut
         || type == QEvent::ShortcutOverride )
    {
        m_wnd->updateShortcuts();
    }

    // Close menu on Escape key and give focus back to search edit or browser.
    if (type == QEvent::KeyPress) {
        QKeyEvent *keyevent = static_cast<QKeyEvent *>(ev);
        if (keyevent->key() == Qt::Key_Escape) {
            QMenu *menu = qobject_cast<QMenu*>(object);
            if (menu != nullptr) {
                menu->close();
                if (m_wnd->browseMode())
                    m_wnd->enterBrowseMode();
                else
                    m_wnd->enterSearchMode();
            }
        }
    } else if (type == QEvent::Paint) {
        setActivePaintDevice(object);
    }

    return false;
}

void ClipboardServer::loadSettings()
{
    if (m_monitor) {
        stopMonitoring();
        startMonitoring();
    }
}

void ClipboardServer::shortcutActivated(QxtGlobalShortcut *shortcut)
{
#ifdef NO_GLOBAL_SHORTCUTS
    Q_UNUSED(shortcut);
#else
    m_ignoreKeysTimer.start();

    const QMap<QxtGlobalShortcut*, Command>::const_iterator it =
            m_shortcutActions.constFind(shortcut);
    if ( it != m_shortcutActions.constEnd() ) {
        QVariantMap data;
        const QString shortcutText = portableShortcutText(shortcut->shortcut());
        data.insert(mimeShortcut, shortcutText.toUtf8());
        m_wnd->action(data, it.value(), QModelIndex());
    }
#endif
}
