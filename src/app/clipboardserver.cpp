/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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
#include "gui/clipboardbrowser.h"
#include "gui/configurationmanager.h"
#include "gui/mainwindow.h"
#include "item/clipboarditem.h"
#include "item/itemfactory.h"
#include "scriptable/scriptableworker.h"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMenu>
#include <QMimeData>
#include <QThread>

#ifdef NO_GLOBAL_SHORTCUTS
struct QxtGlobalShortcut {};
#else
#include "../qxt/qxtglobalshortcut.h"
#endif

ClipboardServer::ClipboardServer(int &argc, char **argv, const QString &sessionName)
    : QObject()
    , App(new QApplication(argc, argv), sessionName)
    , m_server(NULL)
    , m_wnd(NULL)
    , m_monitor(NULL)
    , m_checkclip(false)
    , m_lastHash(0)
    , m_shortcutActions()
    , m_clientThreads()
{
    // listen
    m_server = newServer( clipboardServerName(), this );
    if ( !m_server->isListening() )
        return;

    // don't exit when all windows closed
    QApplication::setQuitOnLastWindowClosed(false);

    // main window
    m_wnd = new MainWindow;
    m_wnd->setSessionName(sessionName);

    // listen
    connect( m_server, SIGNAL(newConnection()),
             this, SLOT(newConnection()) );

    connect( m_wnd, SIGNAL(destroyed()),
             QApplication::instance(), SLOT(quit()) );
    connect( m_wnd, SIGNAL(changeClipboard(const ClipboardItem*)),
             this, SLOT(changeClipboard(const ClipboardItem*)));

    loadSettings();

    // notify window if configuration changes
    ConfigurationManager *cm = ConfigurationManager::instance();
    connect( cm, SIGNAL(configurationChanged()),
             this, SLOT(loadSettings()) );

    // hash of the last clipboard data
    bool ok;
    m_lastHash = cm->value("_last_hash").toUInt(&ok);
    if (!ok)
        m_lastHash = 0;

    // run clipboard monitor
    startMonitoring();

    QCoreApplication::instance()->installEventFilter(this);
}

ClipboardServer::~ClipboardServer()
{
    emit terminateClientThreads();
    m_clientThreads.waitForDone();

    if( isMonitoring() )
        stopMonitoring();

    delete m_wnd;

    // delete shortcuts manually
    foreach (QxtGlobalShortcut *s, m_shortcutActions.keys())
        delete s;
    m_shortcutActions.clear();
}

bool ClipboardServer::isListening() const
{
    return m_server->isListening();
}

void ClipboardServer::monitorStateChanged(QProcess::ProcessState newState)
{
    COPYQ_LOG( QString("Monitor state changed: %1").arg(newState) );

    if (newState == QProcess::NotRunning) {
        monitorStandardError();

        QString msg = tr("Clipboard monitor crashed!");
        log(msg, LogError);
        m_wnd->showError(msg);

        // restart clipboard monitor
        stopMonitoring();
        startMonitoring();
    } else if (newState == QProcess::Starting) {
        log( tr("Clipboard Monitor: Starting") );
    } else if (newState == QProcess::Running) {
        log( tr("Clipboard Monitor: Started") );
    }
}

void ClipboardServer::monitorStandardError()
{
    if (m_monitor == NULL)
        return;

    const QByteArray stderrOutput = m_monitor->process().readAllStandardError().trimmed();
    if ( stderrOutput.isEmpty() )
        return;

    foreach( const QByteArray &line, stderrOutput.split('\n') )
        log( tr("Clipboard Monitor: ") + line, LogNote );
}

void ClipboardServer::stopMonitoring()
{
    if (m_monitor == NULL)
        return;

    ConfigurationManager *cm = ConfigurationManager::instance();
    cm->setValue("_last_hash", m_lastHash);

    log( tr("Clipboard Monitor: Terminating") );

    m_monitor->disconnect();
    m_monitor->process().disconnect();
    m_monitor->closeConnection();
    m_monitor->deleteLater();
    m_monitor = NULL;

    log( tr("Clipboard Monitor: Terminated") );
}

void ClipboardServer::startMonitoring()
{
    COPYQ_LOG("Starting monitor.");

    if ( m_monitor == NULL ) {
        m_monitor = new RemoteProcess(this);
        connect( &m_monitor->process(), SIGNAL(stateChanged(QProcess::ProcessState)),
                 this, SLOT(monitorStateChanged(QProcess::ProcessState)) );
        connect( &m_monitor->process(), SIGNAL(readyReadStandardError()),
                 this, SLOT(monitorStandardError()) );
        connect( m_monitor, SIGNAL(newMessage(QByteArray)),
                 this, SLOT(newMonitorMessage(QByteArray)) );
        connect( m_monitor, SIGNAL(connectionError()),
                 this, SLOT(monitorConnectionError()) );

        const QString name = clipboardMonitorServerName();
        m_monitor->start( name, QStringList("monitor") << name );
        if ( !m_monitor->isConnected() ) {
            disconnect();
            delete m_monitor;
            m_monitor = NULL;
            log( tr("Cannot start clipboard monitor!"), LogError );
            exit(10);
            return;
        }

        loadMonitorSettings();
    }
    m_wnd->browser(0)->setAutoUpdate(true);
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
    m_checkclip = cm->value("check_clipboard").toBool();
#ifdef COPYQ_WS_X11
    settings["copy_clipboard"] = cm->value("copy_clipboard");
    settings["copy_selection"] = cm->value("copy_selection");
    settings["check_selection"] = cm->value("check_selection");
#endif

    QByteArray settings_data;
    QDataStream settings_out(&settings_data, QIODevice::WriteOnly);
    settings_out << settings;

    ClipboardItem item;
    item.setData(mimeApplicationSettings, settings_data);

    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);
    out << item;
    m_monitor->writeMessage(msg);
}

bool ClipboardServer::isMonitoring()
{
    return m_monitor != NULL && m_monitor->isConnected();
}

void ClipboardServer::newConnection()
{
    QLocalSocket* client( m_server->nextPendingConnection() );

#ifdef COPYQ_LOG_DEBUG
    quintptr id = client->socketDescriptor();
    COPYQ_LOG( QString("New client connection %1").arg(id) );
    COPYQ_LOG( QString("%1: Receiving message from client.").arg(id) );
#endif

    Arguments args;
    QByteArray msg;

    if ( readMessage(client, &msg) ) {
        QDataStream in(msg);
        in >> args;

        COPYQ_LOG( QString("%1: Message received from client.").arg(id) );

        // try to handle command
        doCommand(args, client);
    } else {
        const QString error = client->errorString();
        log( tr("Cannot read message from client! (%1)").arg(error), LogError );
        client->deleteLater();
    }
}

void ClipboardServer::sendMessage(QLocalSocket* client, const QByteArray &message, int exitCode)
{
#ifdef COPYQ_LOG_DEBUG
    quintptr id = client->socketDescriptor();
#endif

    COPYQ_LOG( QString("%1: Sending message to client (exit code: %2).")
               .arg(id)
               .arg(exitCode) );

    if ( client->state() == QLocalSocket::ConnectedState ) {
        QByteArray msg;
        QDataStream out(&msg, QIODevice::WriteOnly);
        out << exitCode;
        out.writeRawData( message.constData(), message.length() );
        if ( !writeMessage(client, msg) ) {
            COPYQ_LOG( QString("%1: Failed to send message to client!").arg(id) );
        } else if (exitCode == CommandFinished) {
            connect(client, SIGNAL(disconnected()),
                    client, SLOT(deleteLater()));
            COPYQ_LOG( QString("%1: Disconnected from client.").arg(id) );
        } else if (exitCode == CommandExit) {
            client->flush();
            QApplication::exit(0);
        }
        COPYQ_LOG( QString("%1: Message send to client.").arg(id) );
    } else {
        COPYQ_LOG( QString("%1: Client disconnected!").arg(id) );
    }
}

void ClipboardServer::newMonitorMessage(const QByteArray &message)
{
    COPYQ_LOG("Receiving message from monitor.");

    ClipboardItem item;
    QDataStream in(message);
    in >> item;

#ifdef COPYQ_WS_X11
    if ( item.data()->data(mimeClipboardMode) != "selection" )
        m_wnd->clipboardChanged(&item);
#else
    m_wnd->clipboardChanged(&item);
#endif

    if ( m_checkclip && !item.isEmpty() && m_lastHash != item.dataHash() ) {
        m_lastHash = item.dataHash();
        m_wnd->addToTab( item.data(), QString(), true );
    }

    COPYQ_LOG("Message received from monitor.");
}

void ClipboardServer::monitorConnectionError()
{
    stopMonitoring();
    startMonitoring();
}

void ClipboardServer::changeClipboard(const ClipboardItem *item)
{
    if ( !isMonitoring() ) {
        COPYQ_LOG("Cannot send message to monitor!");
        return;
    }

    COPYQ_LOG("Sending message to monitor.");

    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);
    out << *item;
    m_monitor->writeMessage(msg);
    m_lastHash = item->dataHash();
}

void ClipboardServer::doCommand(const Arguments &args, QLocalSocket *client)
{
    // Worker object without parent needs to be deleted afterwards!
    // There is no parent so as it's possible to move the worker to another thread.
    ScriptableWorker *worker = new ScriptableWorker(m_wnd, args, client);

    // Delete worker after it's finished.
    connect(worker, SIGNAL(finished()), worker, SLOT(deleteLater()));

    // Terminate worket at application exit.
    connect(this, SIGNAL(terminateClientThreads()),
            worker, SLOT(terminate()));

    if (client != NULL) {
        connect(client, SIGNAL(disconnected()),
                worker, SLOT(terminate()));
        connect(worker, SIGNAL(sendMessage(QLocalSocket*,QByteArray,int)),
                this, SLOT(sendMessage(QLocalSocket*,QByteArray,int)));

        // Add client thread to pool.
        m_clientThreads.start(worker);
    } else {
        // Run internally created command immediatelly (should be fast).
        QThread *workerThread = new QThread(this);
        worker->moveToThread(workerThread);

        // t.started -> w.run -> w.finished -> t.quit -> t.finished -> delete t,w,c
        connect(workerThread, SIGNAL(started()), worker, SLOT(run()));
        connect(worker, SIGNAL(finished()), workerThread, SLOT(quit()));
        connect(workerThread, SIGNAL(finished()), workerThread, SLOT(deleteLater()));

        workerThread->start();
    }
}

Arguments *ClipboardServer::createGlobalShortcut(const QString &shortcut)
{
#ifdef NO_GLOBAL_SHORTCUTS
    Q_UNUSED(shortcut);
    return NULL;
#else
    if ( shortcut.isEmpty() )
        return NULL;

    QKeySequence keyseq(shortcut, QKeySequence::NativeText);
    QxtGlobalShortcut *s = new QxtGlobalShortcut(keyseq, this);
    connect( s, SIGNAL(activated(QxtGlobalShortcut*)),
             this, SLOT(shortcutActivated(QxtGlobalShortcut*)) );

    // Don't process global shortcuts any further.
    // FIXME: This should be set for all modal windows.
    QAction *act = new QAction(this);
    act->setShortcut(keyseq);
    act->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    act->setPriority(QAction::HighPriority);
    m_wnd->addAction(act);
    ConfigurationManager::instance()->addAction(act);
    connect( s, SIGNAL(destroyed()), act, SLOT(deleteLater()) );

    return &m_shortcutActions[s];
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
    }
    return false;
}

void ClipboardServer::loadSettings()
{
#ifndef NO_GLOBAL_SHORTCUTS
    ConfigurationManager *cm = ConfigurationManager::instance();

    // set global shortcuts
    QString key;
    Arguments *args;

    foreach (QxtGlobalShortcut *s, m_shortcutActions.keys())
        delete s;
    m_shortcutActions.clear();

    key = cm->value("toggle_shortcut").toString();
    args = createGlobalShortcut(key);
    if (args)
        args->append("toggle");

    key = cm->value("menu_shortcut").toString();
    args = createGlobalShortcut(key);
    if (args)
        args->append("menu");

    key = cm->value("edit_clipboard_shortcut").toString();
    args = createGlobalShortcut(key);
    if (args) {
        args->append("edit");
        args->append("-1");
    }

    key = cm->value("edit_shortcut").toString();
    args = createGlobalShortcut(key);
    if (args) {
        args->append("edit");
        args->append("0");
    }

    key = cm->value("second_shortcut").toString();
    args = createGlobalShortcut(key);
    if (args) {
        args->append("select");
        args->append("1");
    }

    key = cm->value("show_action_dialog").toString();
    args = createGlobalShortcut(key);
    if (args)
        args->append("action");

    key = cm->value("new_item_shortcut").toString();
    args = createGlobalShortcut(key);
    if (args)
        args->append("edit");

    key = cm->value("next_item_shortcut").toString();
    args = createGlobalShortcut(key);
    if (args)
        args->append("next");

    key = cm->value("previous_item_shortcut").toString();
    args = createGlobalShortcut(key);
    if (args)
        args->append("previous");

    key = cm->value("paste_as_plain_text").toString();
    args = createGlobalShortcut(key);
    if (args) {
        args->append("eval");
        args->append("copy(clipboard()); paste()");
    }

    key = cm->value("disable_monitoring_shortcut").toString();
    args = createGlobalShortcut(key);
    if (args)
        args->append("disable");

    key = cm->value("enable_monitoring_shortcut").toString();
    args = createGlobalShortcut(key);
    if (args)
        args->append("enable");

    key = cm->value("paste_and_copy_next_shortcut").toString();
    args = createGlobalShortcut(key);
    if (args) {
        args->append("eval");
        args->append("paste(); next();");
    }

    key = cm->value("paste_and_copy_previous_shortcut").toString();
    args = createGlobalShortcut(key);
    if (args) {
        args->append("eval");
        args->append("paste(); previous();");
    }
#endif

    // reload clipboard monitor configuration
    if ( isMonitoring() )
        loadMonitorSettings();
}

void ClipboardServer::shortcutActivated(QxtGlobalShortcut *shortcut)
{
    Arguments args = m_shortcutActions[shortcut];
    doCommand(args);
}
