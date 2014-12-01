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

#include "clipboardserver.h"

#include "app/remoteprocess.h"
#include "common/arguments.h"
#include "common/clientsocket.h"
#include "common/client_server.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/monitormessagecode.h"
#include "gui/clipboardbrowser.h"
#include "gui/commanddialog.h"
#include "gui/configtabshortcuts.h"
#include "gui/configurationmanager.h"
#include "gui/iconfactory.h"
#include "gui/mainwindow.h"
#include "item/itemfactory.h"
#include "item/serialize.h"
#include "scriptable/scriptableworker.h"

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QSessionManager>
#include <QThread>

#ifdef NO_GLOBAL_SHORTCUTS
struct QxtGlobalShortcut {};
#else
#include "../qxt/qxtglobalshortcut.h"
#endif

namespace {

QString newClipboardMonitorServerName()
{
    static int monitorProcessId = 0;
    return serverName( "m" + QString::number(monitorProcessId) + "_"
                       + QString::number(QDateTime::currentMSecsSinceEpoch()) );
}

} // namespace

ClipboardServer::ClipboardServer(int &argc, char **argv, const QString &sessionName)
    : QObject()
    , App(createPlatformNativeInterface()->createServerApplication(argc, argv),
          sessionName, true)
    , m_wnd(NULL)
    , m_monitor(NULL)
    , m_shortcutActions()
    , m_shortcutBlocker()
    , m_clientThreads()
    , m_ignoreKeysTimer()
{
    Server *server = new Server( clipboardServerName(), this );
    if ( !server->isListening() ) {
        log( tr("CopyQ server is already running."), LogWarning );
        exit(0);
        return;
    }

    createSessionMutex();

    QApplication::setQuitOnLastWindowClosed(false);

    m_wnd = new MainWindow;

    connect( server, SIGNAL(newConnection(Arguments,ClientSocket*)),
             this, SLOT(doCommand(Arguments,ClientSocket*)) );

    connect( QCoreApplication::instance(), SIGNAL(aboutToQuit()),
             this, SLOT(onAboutToQuit()));

    connect( qApp, SIGNAL(commitDataRequest(QSessionManager&)),
             this, SLOT(onCommitData(QSessionManager&)) );

    connect( m_wnd, SIGNAL(changeClipboard(QVariantMap)),
             this, SLOT(changeClipboard(QVariantMap)) );

    connect( m_wnd, SIGNAL(requestExit()),
             this, SLOT(maybeQuit()) );

    loadSettings();

    // notify window if configuration changes
    connect( ConfigurationManager::instance(), SIGNAL(configurationChanged()),
             this, SLOT(loadSettings()) );

#ifndef NO_GLOBAL_SHORTCUTS
    connect( m_wnd, SIGNAL(commandsSaved()),
             this, SLOT(createGlobalShortcuts()) );
    createGlobalShortcuts();
#endif

    // Allow to run at least few client and internal threads concurrently.
    m_clientThreads.setMaxThreadCount( qMax(m_clientThreads.maxThreadCount(), 8) );

    // run clipboard monitor
    startMonitoring();

    QCoreApplication::instance()->installEventFilter(this);

    server->start();

    // Ignore global shortcut key presses in any widget.
    initSingleShotTimer( &m_ignoreKeysTimer, 0, this, SLOT(onIgnoreKeysTimeout()) );
}

ClipboardServer::~ClipboardServer()
{
    removeGlobalShortcuts();
    delete m_wnd;
}

void ClipboardServer::stopMonitoring()
{
    if (m_monitor == NULL)
        return;

    COPYQ_LOG("Clipboard Monitor: Terminating");

    m_monitor->disconnect();
    delete m_monitor;
    m_monitor = NULL;

    COPYQ_LOG("Clipboard Monitor: Terminated");
}

void ClipboardServer::startMonitoring()
{
    COPYQ_LOG("Starting monitor.");

    if ( m_monitor == NULL ) {
        m_monitor = new RemoteProcess(this);
        connect( m_monitor, SIGNAL(newMessage(QByteArray)),
                 this, SLOT(newMonitorMessage(QByteArray)) );
        connect( m_monitor, SIGNAL(connectionError()),
                 this, SLOT(monitorConnectionError()) );
        connect( m_monitor, SIGNAL(connected()),
                 this, SLOT(loadMonitorSettings()) );

        const QString name = newClipboardMonitorServerName();
        m_monitor->start( name, QStringList("monitor") << name );
    }
}

void ClipboardServer::loadMonitorSettings()
{
    if ( !isMonitoring() ) {
        COPYQ_LOG("Cannot configure monitor!");
        return;
    }

    COPYQ_LOG("Configuring monitor.");

    ConfigurationManager *cm = ConfigurationManager::instance();

    QVariantMap settings;
    settings["formats"] = cm->itemFactory()->formatsToSave();
#ifdef COPYQ_WS_X11
    settings["copy_clipboard"] = cm->value("copy_clipboard");
    settings["copy_selection"] = cm->value("copy_selection");
    settings["check_selection"] = cm->value("check_selection");
#endif

    QByteArray settingsData;
    QDataStream settingsOut(&settingsData, QIODevice::WriteOnly);
    settingsOut << settings;

    m_monitor->writeMessage(settingsData, MonitorSettings);
}

bool ClipboardServer::isMonitoring()
{
    return m_monitor != NULL && m_monitor->isConnected();
}

void ClipboardServer::removeGlobalShortcuts()
{
    foreach (QxtGlobalShortcut *s, m_shortcutActions.keys())
        delete s;
    m_shortcutActions.clear();
}

void ClipboardServer::createGlobalShortcuts()
{
    removeGlobalShortcuts();

    QList<QKeySequence> usedShortcuts;

    foreach ( const Command &command, loadCommands() ) {
        foreach (const QString &shortcutText, command.globalShortcuts) {
            QKeySequence shortcut(shortcutText, QKeySequence::PortableText);
            if ( !shortcut.isEmpty() && !usedShortcuts.contains(shortcut) ) {
                usedShortcuts.append(shortcut);
                createGlobalShortcut(shortcut, command);
            }
        }
    }
}

void ClipboardServer::onAboutToQuit()
{
    COPYQ_LOG("Closing server.");

    m_wnd->saveTabs();

    if( isMonitoring() )
        stopMonitoring();

    COPYQ_LOG( QString("Active client threads: %1").arg(m_clientThreads.activeThreadCount()) );

    COPYQ_LOG("Terminating remaining threads.");
    emit terminateClientThreads();
    while ( !m_clientThreads.waitForDone(0) )
        QApplication::processEvents();
}

void ClipboardServer::onCommitData(QSessionManager &sessionManager)
{
    bool cancel = sessionManager.allowsInteraction() && !askToQuit();
    sessionManager.release();

    if (cancel)
        sessionManager.cancel();
    else
        m_wnd->saveTabs();
}

void ClipboardServer::maybeQuit()
{
    if (askToQuit())
        QCoreApplication::exit();
}

void ClipboardServer::onIgnoreKeysTimeout()
{
    m_wnd->setEnabled(true);
}

bool ClipboardServer::askToQuit()
{
    if ( m_clientThreads.activeThreadCount() > 0 || m_wnd->hasRunningAction() ) {
        QMessageBox messageBox( QMessageBox::Warning, tr("Cancel Active Commands"),
                                tr("Cancel active commands and exit?"), QMessageBox::NoButton,
                                m_wnd );

        messageBox.addButton(tr("Cancel Exiting"), QMessageBox::RejectRole);
        messageBox.addButton(tr("Exit Anyway"), QMessageBox::AcceptRole);

        messageBox.exec();
        return messageBox.result() == QMessageBox::Accepted;
    }

    return true;
}

void ClipboardServer::doCommand(const Arguments &args, ClientSocket *client)
{
    // Worker object without parent needs to be deleted afterwards!
    // There is no parent so as it's possible to move the worker to another thread.
    // QThreadPool takes ownership and worker will be automatically deleted
    // after run() (see QRunnable::setAutoDelete()).
    ScriptableWorker *worker = new ScriptableWorker(m_wnd, args, client);

    // Terminate worker at application exit.
    connect( this, SIGNAL(terminateClientThreads()),
             client, SLOT(close()) );

    // Add client thread to pool.
    m_clientThreads.start(worker);
}

void ClipboardServer::newMonitorMessage(const QByteArray &message)
{
    if ( m_wnd->isClipboardStoringDisabled() )
        return;

    QVariantMap data;

    if ( !deserializeData(&data, message) ) {
        log("Failed to read message from monitor.", LogError);
        return;
    }

    m_wnd->clipboardChanged(data);
}

void ClipboardServer::monitorConnectionError()
{
    stopMonitoring();
    startMonitoring();
}

void ClipboardServer::changeClipboard(const QVariantMap &data)
{
    if ( !isMonitoring() ) {
        COPYQ_LOG("Cannot send message to monitor!");
        return;
    }

    COPYQ_LOG("Sending message to monitor.");

    m_monitor->writeMessage( serializeData(data), MonitorChangeClipboard );
}

void ClipboardServer::createGlobalShortcut(const QKeySequence &shortcut, const Command &command)
{
#ifdef NO_GLOBAL_SHORTCUTS
    Q_UNUSED(shortcut);
    Q_UNUSED(script);
#else
    QxtGlobalShortcut *s = new QxtGlobalShortcut(shortcut, this);
    connect( s, SIGNAL(activated(QxtGlobalShortcut*)),
             this, SLOT(shortcutActivated(QxtGlobalShortcut*)) );

    // Create special dummy QAction so that it blocks global shortcuts in active windows.
    QAction *act = new QAction(s);
    act->setShortcut(shortcut);
    act->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    act->setPriority(QAction::HighPriority);
    m_shortcutBlocker.addAction(act);

    m_shortcutActions[s] = command;
#endif
}

bool ClipboardServer::eventFilter(QObject *object, QEvent *ev)
{
    // Close menu on Escape key and give focus back to search edit or browser.
    if (ev->type() == QEvent::KeyPress) {
        QKeyEvent *keyevent = static_cast<QKeyEvent *>(ev);
        if (keyevent->key() == Qt::Key_Escape) {
            QMenu *menu = qobject_cast<QMenu*>(object);
            if (menu != NULL) {
                menu->close();
                m_wnd->enterBrowseMode(m_wnd->browseMode());
            }
        }
    } else if (ev->type() == QEvent::Paint) {
        ConfigurationManager::instance()->iconFactory()->setActivePaintDevice(object);
    }

    return false;
}

void ClipboardServer::loadSettings()
{
    // reload clipboard monitor configuration
    if ( isMonitoring() )
        loadMonitorSettings();
}

void ClipboardServer::shortcutActivated(QxtGlobalShortcut *shortcut)
{
    m_ignoreKeysTimer.start();
    m_wnd->setEnabled(false);
    if ( m_shortcutActions.contains(shortcut) )
        m_wnd->action(QVariantMap(), m_shortcutActions[shortcut]);
}
