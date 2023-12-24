// SPDX-License-Identifier: GPL-3.0-or-later

#include "clipboardserver.h"

#include "common/action.h"
#include "common/appconfig.h"
#include "common/clientsocket.h"
#include "common/client_server.h"
#include "common/commandstatus.h"
#include "common/config.h"
#include "common/display.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/shortcuts.h"
#include "common/sleeptimer.h"
#include "common/timer.h"
#include "common/textdata.h"
#include "gui/actionhandler.h"
#include "gui/clipboardbrowser.h"
#include "gui/commanddialog.h"
#include "gui/iconfactory.h"
#include "gui/mainwindow.h"
#include "gui/notificationbutton.h"
#include "gui/notificationdaemon.h"
#include "item/itemfactory.h"
#include "item/itemstore.h"
#include "item/serialize.h"
#include "scriptable/scriptableproxy.h"

#include <QAction>
#include <QApplication>
#include <QApplicationStateChangeEvent>
#include <QFile>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QSessionManager>
#include <QStyleFactory>
#include <QTextEdit>

#ifdef COPYQ_GLOBAL_SHORTCUTS
#include "../qxt/qxtglobalshortcut.h"
#else
class QxtGlobalShortcut final {};
#endif

#include <memory>

namespace {

uint monitorCommandStateHash(const QVector<Command> &commands)
{
    uint seed = 0;
    QtPrivate::QHashCombine hash;

    for (const auto &command : commands) {
        if (command.type() == CommandType::Script)
            seed = hash(seed, command.cmd);
        else if (command.type() == CommandType::Automatic && !command.input.isEmpty())
            seed = hash(seed, command.input);
    }

    return seed;
}

void setTabWidth(QTextEdit *editor, int spaces)
{
    const QLatin1Char space(' ');
    const QFontMetricsF metrics(editor->fontMetrics());

#if QT_VERSION >= QT_VERSION_CHECK(5,11,0)
    const qreal width = metrics.horizontalAdvance(QString(spaces, space));
#else
    const qreal width = metrics.width(space) * spaces;
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5,10,0)
    editor->setTabStopDistance(width);
#else
    editor->setTabStopWidth(width);
#endif
}

} // namespace

ClipboardServer::ClipboardServer(QApplication *app, const QString &sessionName)
    : QObject()
    , App(app, sessionName)
    , m_wnd(nullptr)
    , m_shortcutActions()
    , m_ignoreKeysTimer()
{
    setLogLabel("Server");

    m_server = new Server(clipboardServerName(), this);

    if ( m_server->isListening() ) {
        App::installTranslator();
        qApp->setLayoutDirection(QLocale().textDirection());
    } else {
        App::installTranslator();
        if ( canUseStandardOutput() ) {
            log( tr("CopyQ server is already running."), LogWarning );
        }
        exit(0);
        return;
    }

    if ( sessionName.isEmpty() ) {
        QGuiApplication::setApplicationDisplayName(QStringLiteral("CopyQ"));
    } else {
        QGuiApplication::setApplicationDisplayName(
            QStringLiteral("CopyQ-%1").arg(sessionName));
    }

    QGuiApplication::setDesktopFileName(QStringLiteral("com.github.hluk.copyq"));

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
#endif

    QApplication::setQuitOnLastWindowClosed(false);

    ensureSettingsDirectoryExists();

    m_sharedData = std::make_shared<ClipboardBrowserShared>();
    m_sharedData->itemFactory = new ItemFactory(this);
    m_sharedData->notifications = new NotificationDaemon(this);
    m_sharedData->actions = new ActionHandler(m_sharedData->notifications, this);
    m_wnd = new MainWindow(m_sharedData);

    connect( m_sharedData->notifications, &NotificationDaemon::notificationButtonClicked,
             this, &ClipboardServer::onNotificationButtonClicked );

    m_sharedData->itemFactory->loadPlugins();
    if ( !m_sharedData->itemFactory->hasLoaders() )
        log("No plugins loaded", LogNote);

    connect( m_server, &Server::newConnection,
             this, &ClipboardServer::onClientNewConnection );

    connect( qApp, &QCoreApplication::aboutToQuit,
             this, &ClipboardServer::onAboutToQuit );

    connect( qApp, &QGuiApplication::commitDataRequest, this, &ClipboardServer::onCommitData );
    connect( qApp, &QGuiApplication::saveStateRequest, this, &ClipboardServer::onSaveState );
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    qApp->setFallbackSessionManagementEnabled(false);
#endif

    connect( m_wnd, &MainWindow::requestExit,
             this, &ClipboardServer::maybeQuit );
    connect( m_wnd, &MainWindow::disableClipboardStoringRequest,
             this, &ClipboardServer::onDisableClipboardStoringRequest );
    connect( m_wnd, &MainWindow::sendActionData,
             this, &ClipboardServer::sendActionData );

    // notify window if configuration changes
    connect( m_wnd, &MainWindow::configurationChanged,
             this, &ClipboardServer::loadSettings );

    connect( m_wnd, &MainWindow::commandsSaved,
             this, &ClipboardServer::onCommandsSaved );

    m_server->start();

    {
        AppConfig appConfig;
        loadSettings(&appConfig);
    }

    m_wnd->setCurrentTab(0);
    m_wnd->enterBrowseMode();

    qApp->installEventFilter(this);

    // Ignore global shortcut key presses in any widget.
    m_ignoreKeysTimer.setInterval(100);
    m_ignoreKeysTimer.setSingleShot(true);

    initSingleShotTimer(&m_timerClearUnsentActionData, 2000, this, [&]() {
        m_actionDataToSend.clear();
    });

    initSingleShotTimer(&m_timerCleanItemFiles, 120000, this, &ClipboardServer::cleanDataFiles);

    initSingleShotTimer(&m_updateThemeTimer, 1000, this, [this](){
        AppConfig appConfig;
        loadSettings(&appConfig);
    });

    setClipboardMonitorRunning(false);
    startMonitoring();

    callback(QStringLiteral("onStart"));
}

ClipboardServer::~ClipboardServer()
{
    removeGlobalShortcuts();

    delete m_wnd;
    m_wnd = nullptr;

    delete m_sharedData->actions;
    m_sharedData->actions = nullptr;

    delete m_sharedData->notifications;
    m_sharedData->notifications = nullptr;

    delete m_sharedData->itemFactory;
    m_sharedData->itemFactory = nullptr;
}

void ClipboardServer::stopMonitoring()
{
    if (!m_monitor)
        return;

    COPYQ_LOG("Terminating monitor");
    setClipboardMonitorRunning(false);

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
    m_monitor->setCommand(QStringLiteral("copyq --clipboard-access monitorClipboard"));
    connect( m_monitor.data(), &QObject::destroyed,
             this, &ClipboardServer::onMonitorFinished );
    m_sharedData->actions->internalAction(m_monitor);
}

void ClipboardServer::removeGlobalShortcuts()
{
    for (auto it = m_shortcutActions.constBegin(); it != m_shortcutActions.constEnd(); ++it)
        delete it.key();
    m_shortcutActions.clear();
}

void ClipboardServer::onCommandsSaved(const QVector<Command> &commands)
{
#ifdef COPYQ_GLOBAL_SHORTCUTS
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

    callback(QStringLiteral("onExit"));
    waitForCallbackToFinish();

    m_ignoreNewConnections = true;

    terminateClients(10000);
    m_server->close(); // No new connections can be open.
    terminateClients(5000);

    m_wnd->saveTabs();
    cleanDataFiles();
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
    if (!m_sharedData->actions)
        return false;

    if ( m_sharedData->actions->runningActionCount() > 0 )
        return true;

    for (auto it = m_clients.constBegin(); it != m_clients.constEnd(); ++it) {
        const auto actionId = it.value().proxy->actionId();
        if ( !m_sharedData->actions->isInternalActionId(actionId) )
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
    if (!m_sharedData->actions)
        return;

    waitForCallbackToFinish();
    COPYQ_LOG( QStringLiteral("Starting callback: %1").arg(scriptFunction) );
    m_callback = new Action();
    m_callback->setCommand(QStringList() << "copyq" << scriptFunction);
    m_sharedData->actions->internalAction(m_callback);
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

void ClipboardServer::cleanDataFiles()
{
    COPYQ_LOG("Cleaning unused item files");
    ::cleanDataFiles( m_wnd->tabs() );
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
        log(QStringLiteral("Unhandled command status: %1").arg(messageCode));
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
    if (!m_monitor)
        setClipboardMonitorRunning(false);
    stopMonitoring();
    startMonitoring();
}

void ClipboardServer::onNotificationButtonClicked(const NotificationButton &button)
{
    if (!m_sharedData->actions)
        return;

    const QString mimeNotificationData = COPYQ_MIME_PREFIX "notification-data";

    QVariantMap data;
    data.insert(mimeNotificationData, button.data);

    auto act = new Action();
    act->setCommand( button.script, QStringList(getTextData(data)) );
    act->setInputWithFormat(data, mimeNotificationData);
    act->setData(data);
    m_sharedData->actions->action(act);
}

void ClipboardServer::createGlobalShortcut(const QKeySequence &shortcut, const Command &command)
{
#ifndef COPYQ_GLOBAL_SHORTCUTS
    Q_UNUSED(shortcut)
    Q_UNUSED(command)
#else
    auto s = new QxtGlobalShortcut(shortcut, this);
    if (!s->isValid()) {
        log(QStringLiteral("Failed to set global shortcut \"%1\" for command \"%2\".")
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
                    (object->metaObject()->className() == QLatin1String("QLineEdit")
                     || object->property("textInteractionFlags")
                        .value<Qt::TextInteractionFlags>()
                        .testFlag(Qt::TextSelectableByKeyboard)) )
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
    } else if (type == QEvent::FontChange) {
        QTextEdit *editor = qobject_cast<QTextEdit*>(object);
        if (editor)
            setTabWidth(editor, m_textTabSize);
    } else if ( type == QEvent::ApplicationStateChange && m_saveOnDeactivate ) {
        const auto stateChangeEvent = static_cast<QApplicationStateChangeEvent*>(ev);
        const auto state = stateChangeEvent->applicationState();
        if (state != Qt::ApplicationActive) {
            COPYQ_LOG( QStringLiteral("Saving items on application state change (%1)").arg(state) );
            m_wnd->saveTabs();
            m_timerCleanItemFiles.start();
        }
    } else if (type == QEvent::ThemeChange) {
        if ( !m_updateThemeTimer.isActive() )
            COPYQ_LOG("Got theme change event");
        m_updateThemeTimer.start();
    }

    return false;
}

void ClipboardServer::loadSettings(AppConfig *appConfig)
{
    if (!m_sharedData->itemFactory)
        return;

    COPYQ_LOG("Loading configuration");

    QSettings &settings = appConfig->settings();

    m_sharedData->itemFactory->loadItemFactorySettings(&settings);

    const QString styleName = appConfig->option<Config::style>();
    if ( !styleName.isEmpty() ) {
        log( QStringLiteral("Style: %1").arg(styleName) );
        QStyle *style = QStyleFactory::create(styleName);
        if (style) {
            QApplication::setStyle(style);
        } else {
            const QString styles = QStyleFactory::keys().join(QLatin1String(", "));
            log( QStringLiteral("Failed to set style, valid are: %1").arg(styles), LogWarning );
        }
    }

    settings.beginGroup(QStringLiteral("Theme"));
    m_sharedData->theme.loadTheme(settings);
    settings.endGroup();

    m_sharedData->editor = appConfig->option<Config::editor>();
    m_sharedData->maxItems = appConfig->option<Config::maxitems>();
    m_sharedData->textWrap = appConfig->option<Config::text_wrap>();
    m_sharedData->viMode = appConfig->option<Config::vi>();
    m_sharedData->saveOnReturnKey = !appConfig->option<Config::edit_ctrl_return>();
    m_sharedData->moveItemOnReturnKey = appConfig->option<Config::move>();
    m_sharedData->showSimpleItems = appConfig->option<Config::show_simple_items>();
    m_sharedData->numberSearch = appConfig->option<Config::number_search>();
    m_sharedData->minutesToExpire = appConfig->option<Config::expire_tab>();
    m_sharedData->saveDelayMsOnItemAdded = appConfig->option<Config::save_delay_ms_on_item_added>();
    m_sharedData->saveDelayMsOnItemModified = appConfig->option<Config::save_delay_ms_on_item_modified>();
    m_sharedData->saveDelayMsOnItemRemoved = appConfig->option<Config::save_delay_ms_on_item_removed>();
    m_sharedData->saveDelayMsOnItemMoved = appConfig->option<Config::save_delay_ms_on_item_moved>();
    m_sharedData->saveDelayMsOnItemEdited = appConfig->option<Config::save_delay_ms_on_item_edited>();
    m_sharedData->rowIndexFromOne = appConfig->option<Config::row_index_from_one>();

    m_sharedData->actions->setMaxRowCount(appConfig->option<Config::max_process_manager_rows>());

    m_wnd->loadSettings(settings, appConfig);

    m_textTabSize = appConfig->option<Config::text_tab_width>();
    m_saveOnDeactivate = appConfig->option<Config::save_on_app_deactivated>();

    if (m_monitor) {
        stopMonitoring();
        startMonitoring();
    }

    m_sharedData->notifications->setNativeNotificationsEnabled(
        appConfig->option<Config::native_notifications>() );
    m_sharedData->notifications->setNotificationOpacity(
        m_sharedData->theme.color(QStringLiteral("notification_bg")).alphaF() );
    m_sharedData->notifications->setNotificationStyleSheet(
        m_sharedData->theme.getNotificationStyleSheet() );

    int id = appConfig->option<Config::notification_position>();
    NotificationDaemon::Position position;
    switch (id) {
    case 0: position = NotificationDaemon::Top; break;
    case 1: position = NotificationDaemon::Bottom; break;
    case 2: position = NotificationDaemon::TopRight; break;
    case 3: position = NotificationDaemon::BottomRight; break;
    case 4: position = NotificationDaemon::BottomLeft; break;
    default: position = NotificationDaemon::TopLeft; break;
    }
    m_sharedData->notifications->setPosition(position);

    const int x = appConfig->option<Config::notification_horizontal_offset>();
    const int y = appConfig->option<Config::notification_vertical_offset>();
    m_sharedData->notifications->setOffset(x, y);

    const int w = appConfig->option<Config::notification_maximum_width>();
    const int h = appConfig->option<Config::notification_maximum_height>();
    m_sharedData->notifications->setMaximumSize(w, h);

    m_sharedData->notifications->updateNotificationWidgets();

    m_updateThemeTimer.stop();

    COPYQ_LOG("Configuration loaded");
}

void ClipboardServer::shortcutActivated(QxtGlobalShortcut *shortcut)
{
#ifndef COPYQ_GLOBAL_SHORTCUTS
    Q_UNUSED(shortcut)
#else
    m_ignoreKeysTimer.start();

    const QMap<QxtGlobalShortcut*, Command>::const_iterator it =
            m_shortcutActions.constFind(shortcut);
    if ( it != m_shortcutActions.constEnd() ) {
        const QString shortcutText = portableShortcutText(shortcut->shortcut());
        const Command &command = it.value();

        // If global shortcut for a menu command is triggered when the main window
        // is active, the command will be executed as if it has been trigger from
        // menu - i.e. with item selection and item data available.
        if ( command.inMenu && m_wnd->isActiveWindow() && m_wnd->triggerMenuCommand(command, shortcutText) ) {
            COPYQ_LOG("Global shortcut command triggered as a menu command");
        } else {
            QVariantMap data;
            data.insert(mimeShortcut, shortcutText.toUtf8());
            m_wnd->action(data, command, QModelIndex());
        }
    }
#endif
}
