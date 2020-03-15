/*
    Copyright (c) 2019, Lukas Holecek <hluk@email.cz>

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
#include "common/timer.h"
#include "gui/clipboardbrowser.h"
#include "gui/commanddialog.h"
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
class QxtGlobalShortcut final {};
#else
#include "../qxt/qxtglobalshortcut.h"
#endif

namespace {

uint monitorCommandStateHash(const QVector<Command> &commands)
{
    uint hash = 0;

    for (const auto &command : commands) {
        if (command.type() == CommandType::Script)
            hash = qHash(command.cmd, hash);
        else if (command.type() == CommandType::Automatic && !command.input.isEmpty())
            hash = qHash(command.input, hash);
    }

    return hash;
}

} // namespace

ClipboardServer::ClipboardServer(QApplication *app, const QString &sessionName)
    : QObject()
    , App(app, sessionName)
    , m_wnd(nullptr)
    , m_shortcutActions()
    , m_ignoreKeysTimer()
{
    setCurrentThreadName("Server");

    const QString serverName = clipboardServerName();
    m_server = new Server(serverName, this);

    if ( m_server->isListening() ) {
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

    if ( sessionName.isEmpty() ) {
        QGuiApplication::setApplicationDisplayName("CopyQ");
    } else {
        QGuiApplication::setApplicationDisplayName(
            QString("CopyQ-%1").arg(sessionName));
    }

    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    QApplication::setQuitOnLastWindowClosed(false);

    m_itemFactory = new ItemFactory(this);
    m_wnd = new MainWindow(m_itemFactory);

    m_itemFactory->loadPlugins();
    if ( !m_itemFactory->hasLoaders() )
        log("No plugins loaded", LogNote);

    connect( m_server, &Server::newConnection,
             this, &ClipboardServer::onClientNewConnection );

    connect( qApp, &QCoreApplication::aboutToQuit,
             this, &ClipboardServer::onAboutToQuit );

    connect( qApp, &QGuiApplication::commitDataRequest, this, &ClipboardServer::onCommitData );
    connect( qApp, &QGuiApplication::saveStateRequest, this, &ClipboardServer::onSaveState );
#if QT_VERSION >= QT_VERSION_CHECK(5,6,0)
    qApp->setFallbackSessionManagementEnabled(false);
#endif

    connect( m_wnd, &MainWindow::requestExit,
             this, &ClipboardServer::maybeQuit );
    connect( m_wnd, &MainWindow::disableClipboardStoringRequest,
             this, &ClipboardServer::onDisableClipboardStoringRequest );
    connect( m_wnd, &MainWindow::sendActionData,
             this, &ClipboardServer::sendActionData );

    loadSettings();

    // notify window if configuration changes
    connect( m_wnd, &MainWindow::configurationChanged,
             this, &ClipboardServer::loadSettings );

    connect( m_wnd, &MainWindow::commandsSaved,
             this, &ClipboardServer::onCommandsSaved );

    m_wnd->loadSettings();
    m_wnd->setCurrentTab(0);
    m_wnd->enterBrowseMode();

    qApp->installEventFilter(this);

    m_server->start();

    // Ignore global shortcut key presses in any widget.
    m_ignoreKeysTimer.setInterval(100);
    m_ignoreKeysTimer.setSingleShot(true);

    initSingleShotTimer(&m_timerClearUnsentActionData, 2000, this, [&]() {
        m_actionDataToSend.clear();
    });

    startMonitoring();

    callback("onStart");
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

    const auto client = findClient(m_monitor->id());
    if (client)
        client->sendMessage(QByteArray(), CommandStop);
}

void ClipboardServer::startMonitoring()
{
    if (m_monitor || m_ignoreNewConnections || !m_wnd->isMonitoringEnabled())
        return;

    COPYQ_LOG("Starting monitor");

    m_monitor = new Action();
    m_monitor->setCommand("copyq --clipboard-access monitorClipboard");
    connect( m_monitor.data(), &QObject::destroyed,
             this, &ClipboardServer::onMonitorFinished );
    m_wnd->runInternalAction(m_monitor);
}

void ClipboardServer::removeGlobalShortcuts()
{
    for (auto it = m_shortcutActions.constBegin(); it != m_shortcutActions.constEnd(); ++it)
        delete it.key();
    m_shortcutActions.clear();
}

void ClipboardServer::onCommandsSaved(const QVector<Command> &commands)
{
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

    const auto hash = monitorCommandStateHash(commands);
    if ( m_monitor && hash != m_monitorCommandsStateHash ) {
        m_monitorCommandsStateHash = hash;
        stopMonitoring();
        startMonitoring();
    }
}

void ClipboardServer::onAboutToQuit()
{
    COPYQ_LOG("Closing server.");

    // Avoid calling onExit multiple times.
    // (QCoreApplication::aboutToQuit() signal can be emitted multiple times
    // when system is shutting down.)
    if (m_exitting)
        return;
    m_exitting = true;

    callback("onExit");
    waitForCallbackToFinish();

    m_ignoreNewConnections = true;

    terminateClients(10000);
    m_server->close(); // No new connections can be open.
    terminateClients(5000);

    m_wnd->saveTabs();
}

void ClipboardServer::onCommitData(QSessionManager &sessionManager)
{
    COPYQ_LOG("Got commit data request from session manager.");

    const bool cancel = sessionManager.allowsInteraction() && !askToQuit();
    sessionManager.release();

    if (cancel) {
        sessionManager.cancel();
        startMonitoring();
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

    const QString lastSessionIdPrefix = "last_session_id_for_";
    const auto lastSessionIdKey = lastSessionIdPrefix + sessionName;
    const auto lastSessionId = settings.value(lastSessionIdKey).toString();
    const auto lastSessionNameKey = "session_" + lastSessionId;
    settings.setValue(lastSessionIdKey, sessionNameKey);

    // Remove no longer valid sessions from configuration.
    QSet<QString> validSessions;
    for (const QString &key : settings.childKeys()) {
        if ( key.startsWith(lastSessionIdPrefix) )
            validSessions.insert( settings.value(key).toString() );
    }

    for (const QString &key : settings.childKeys()) {
        if ( !key.startsWith(lastSessionIdPrefix) && !validSessions.contains(key) )
            settings.remove(key);
    }
}

void ClipboardServer::onDisableClipboardStoringRequest(bool disabled)
{
    stopMonitoring();
    if (!disabled)
        startMonitoring();
}

void ClipboardServer::maybeQuit()
{
    if ( askToQuit() )
        exit();
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

        // Close the message box automatically after all running commands finish.
        QTimer timerCheckRunningCommands;
        timerCheckRunningCommands.setInterval(1000);
        connect( &timerCheckRunningCommands, &QTimer::timeout,
                 this, [&]() {
                    if ( !hasRunningCommands() )
                        messageBox.accept();
                 });
        timerCheckRunningCommands.start();

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

void ClipboardServer::terminateClients(int waitMs)
{
    for (auto it = m_clients.constBegin(); it != m_clients.constEnd(); ++it) {
        const auto &clientData = it.value();
        if (clientData.isValid())
            clientData.client->sendMessage(QByteArray(), CommandStop);
    }

    waitForClientsToFinish(waitMs);
    emit closeClients();
    waitForClientsToFinish(waitMs / 2);
}

void ClipboardServer::waitForClientsToFinish(int waitMs)
{
    SleepTimer t(waitMs);
    while ( !m_clients.isEmpty() && t.sleep() ) {}
}

void ClipboardServer::waitForCallbackToFinish()
{
    if (m_callback) {
        COPYQ_LOG("Waiting for callback to finish");
        m_callback->waitForFinished();
        COPYQ_LOG("Callback finished");
    }
}

void ClipboardServer::callback(const QString &scriptFunction)
{
    waitForCallbackToFinish();
    COPYQ_LOG( QString("Starting callback: %1").arg(scriptFunction) );
    m_callback = new Action();
    m_callback->setCommand(QStringList() << "copyq" << scriptFunction);
    m_wnd->runInternalAction(m_callback);
}

ClientSocketPtr ClipboardServer::findClient(int actionId)
{
    for (auto it = m_clients.constBegin(); it != m_clients.constEnd(); ++it) {
        const auto &clientData = it.value();
        if ( clientData.isValid() && clientData.proxy->actionId() == actionId )
            return clientData.client;
    }

    return nullptr;
}

void ClipboardServer::sendActionData(int actionId, const QByteArray &bytes)
{
    const auto client = findClient(actionId);
    if (client) {
        client->sendMessage(bytes, CommandData);
    } else {
        m_actionDataToSend[actionId] = bytes;
        m_timerClearUnsentActionData.start();
    }
}

void ClipboardServer::onClientNewConnection(const ClientSocketPtr &client)
{
    auto proxy = new ScriptableProxy(m_wnd);
    connect( client.get(), &ClientSocket::destroyed,
             proxy, &ScriptableProxy::safeDeleteLater );
    connect( proxy, &ScriptableProxy::sendMessage,
             client.get(), &ClientSocket::sendMessage );

    m_clients.insert( client->id(), ClientData(client, proxy) );
    connect( this, &ClipboardServer::closeClients,
             client.get(), &ClientSocket::close );
    connect( client.get(), &ClientSocket::messageReceived,
             this, &ClipboardServer::onClientMessageReceived );
    connect( client.get(), &ClientSocket::disconnected,
             this, &ClipboardServer::onClientDisconnected );
    connect( client.get(), &ClientSocket::disconnected,
             proxy, &ScriptableProxy::clientDisconnected );
    connect( client.get(), &ClientSocket::connectionFailed,
             this, &ClipboardServer::onClientConnectionFailed );
    client->start();

    if (m_ignoreNewConnections) {
        COPYQ_LOG("Ignoring new client while exiting");
        client->sendMessage(QByteArray(), CommandStop);
    }
}

void ClipboardServer::onClientMessageReceived(
        const QByteArray &message, int messageCode, ClientSocketId clientId)
{
    switch (messageCode) {
    case CommandFunctionCall: {
        const auto &clientData = m_clients.value(clientId);
        if (!clientData.isValid())
            return;

        clientData.proxy->callFunction(message);
        break;
    }
    case CommandReceiveData: {
        const auto &clientData = m_clients.value(clientId);
        if (!clientData.isValid())
            return;

        const int actionId = clientData.proxy->actionId();
        const QByteArray bytes = m_actionDataToSend.take(actionId);
        clientData.client->sendMessage(bytes, CommandData);
        break;
    }
    default:
        log(QString("Unhandled command status: %1").arg(messageCode));
        break;
    }
}

void ClipboardServer::onClientDisconnected(ClientSocketId clientId)
{
    m_clients.remove(clientId);
}

void ClipboardServer::onClientConnectionFailed(ClientSocketId clientId)
{
    log("Client connection failed", LogWarning);
    m_clients.remove(clientId);
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

    connect( s, &QxtGlobalShortcut::activated,
             this, &ClipboardServer::shortcutActivated );

    m_shortcutActions[s] = command;
#endif
}

bool ClipboardServer::eventFilter(QObject *object, QEvent *ev)
{
    const QEvent::Type type = ev->type();

    if ( type == QEvent::KeyPress
         || type == QEvent::Shortcut
         || type == QEvent::ShortcutOverride )
    {
        if ( m_ignoreKeysTimer.isActive() ) {
            ev->accept();
            return true;
        }

        m_wnd->updateShortcuts();

        // Close menu on Escape key and give focus back to search edit or browser.
        if (type == QEvent::KeyPress) {
            QKeyEvent *keyevent = static_cast<QKeyEvent *>(ev);
            QMenu *menu = qobject_cast<QMenu*>(object);
            if (menu && keyevent->key() == Qt::Key_Escape) {
                menu->close();
                if (m_wnd->browseMode())
                    m_wnd->enterBrowseMode();
                else
                    m_wnd->enterSearchMode();
            }
        // Omit overriding arrow keys in text editors.
        } else if ( type == QEvent::ShortcutOverride &&
                    object->property("textInteractionFlags")
                    .value<Qt::TextInteractionFlags>()
                    .testFlag(Qt::TextSelectableByKeyboard) )
        {
            QKeyEvent *keyevent = static_cast<QKeyEvent *>(ev);
            if ( keyevent->key() == Qt::Key_Left
                 || keyevent->key() == Qt::Key_Right
                 || keyevent->key() == Qt::Key_Up
                 || keyevent->key() == Qt::Key_Down)
            {
                ev->accept();
                return true;
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
