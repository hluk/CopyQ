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

#include "arguments.h"
#include "clipboardbrowser.h"
#include "clipboarditem.h"
#include "clipboardmonitor.h"
#include "configurationmanager.h"
#include "itemfactory.h"
#include "mainwindow.h"
#include "remoteprocess.h"
#include "scriptable.h"
#include "../qt/bytearrayclass.h"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMenu>
#include <QScriptEngine>

#ifdef NO_GLOBAL_SHORTCUTS
struct QxtGlobalShortcut {};
#else
#include "../qxt/qxtglobalshortcut.h"
#endif

Q_DECLARE_METATYPE(QByteArray*)

ClipboardServer::ClipboardServer(int &argc, char **argv)
    : QObject()
    , App(new QApplication(argc, argv))
    , m_server(NULL)
    , m_wnd(NULL)
    , m_monitor(NULL)
    , m_lastHash(0)
    , m_shortcutActions()
{
    // listen
    m_server = newServer( clipboardServerName(), this );
    if ( !m_server->isListening() )
        return;

    // don't exit when all windows closed
    QApplication::setQuitOnLastWindowClosed(false);

    // main window
    m_wnd = new MainWindow;

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

    COPYQ_LOG("Monitor stderr:");

    const QByteArray stderrOutput = m_monitor->process().readAllStandardError().trimmed();
    foreach( const QByteArray &line, stderrOutput.split('\n') )
        log( tr("Clipboard Monitor: ") + line, LogNote );

    COPYQ_LOG("Monitor stderr end.");
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
            log( tr("Cannot start clipboard monitor!"), LogError );
            delete m_monitor;
            m_monitor = NULL;
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
    settings["_last_hash"] = cm->value("_last_hash");
    settings["formats"] = ItemFactory::instance()->formatsToSave();
    settings["check_clipboard"] = cm->value("check_clipboard");
#ifdef COPYQ_WS_X11
    settings["copy_clipboard"] = cm->value("copy_clipboard");
    settings["copy_selection"] = cm->value("copy_selection");
    settings["check_selection"] = cm->value("check_selection");
#endif

    QByteArray settings_data;
    QDataStream settings_out(&settings_data, QIODevice::WriteOnly);
    settings_out << settings;

    ClipboardItem item;
    item.setData("application/x-copyq-settings", settings_data);

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
    readMessage(client, &msg);
    QDataStream in(msg);
    in >> args;

    COPYQ_LOG( QString("%1: Message received from client.").arg(id) );

    QByteArray client_msg;
    // try to handle command
    int exitCode = doCommand(args, &client_msg, client);
    if ( exitCode == CommandBadSyntax ) {
        client_msg = tr("Bad command syntax. Use -h for help.\n").toLocal8Bit();
    }

    if ( client->state() == QLocalSocket::ConnectedState ) {
        connect(client, SIGNAL(disconnected()),
                client, SLOT(deleteLater()));
        sendMessage(client, client_msg, exitCode);
        sendMessage(client, QByteArray(), CommandFinished);
        COPYQ_LOG( QString("%1: Disconnected from client.").arg(id) );
    } else {
        COPYQ_LOG( QString("%1: Client disconnected!").arg(id) );
        client->deleteLater();
    }
}

void ClipboardServer::sendMessage(QLocalSocket* client, const QByteArray &message, int exitCode)
{
    COPYQ_LOG( QString("%1: Sending message to client (exit code: %2).")
               .arg(client->socketDescriptor())
               .arg(exitCode) );

    {
        QByteArray msg;
        QDataStream out(&msg, QIODevice::WriteOnly);
        out << exitCode;
        out.writeRawData( message.constData(), message.length() );
        writeMessage(client, msg);
    }

    COPYQ_LOG( QString("%1: Message send to client.").arg(client->socketDescriptor()) );
}

void ClipboardServer::newMonitorMessage(const QByteArray &message)
{
    COPYQ_LOG("Receiving message from monitor.");

    ClipboardItem item;
    QDataStream in(message);
    in >> item;

    m_wnd->setTrayToolTip(&item);

    if ( m_lastHash != item.dataHash() ) {
        m_lastHash = item.dataHash();
        m_wnd->addToTab( item.data() );
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

CommandStatus ClipboardServer::doCommand(
        Arguments &args, QByteArray *response, QLocalSocket *client)
{
    COPYQ_LOG("Starting scripting engine.");

    if ( args.length() <= Arguments::Rest ) {
        COPYQ_LOG("Scripting engine: bad command syntax");
        return CommandBadSyntax;
    }
    const QString cmd = QString::fromUtf8( args.at(Arguments::Rest) );

    QScriptEngine engine;
    Scriptable scriptable(m_wnd, client);
    scriptable.initEngine( &engine, QString::fromUtf8(args.at(Arguments::CurrentPath)) );
    if (client != NULL) {
        connect( client, SIGNAL(disconnected()),
                 &scriptable, SLOT(abort()) );
    }

    QScriptValue result;
    QScriptValueList fnArgs;

    QScriptValue fn = engine.globalObject().property(cmd);
    if ( !fn.isFunction() ) {
        COPYQ_LOG("Scripting engine: unknown command");
        return CommandBadSyntax;
    }

    for ( int i = Arguments::Rest + 1; i < args.length(); ++i )
        fnArgs.append( scriptable.newByteArray(args.at(i)) );

    result = fn.call(QScriptValue(), fnArgs);

    if ( engine.hasUncaughtException() ) {
        COPYQ_LOG( QString("Scripting engine: command error (\"%1\")").arg(cmd) );
        response->append(engine.uncaughtException().toString() + '\n');
        engine.clearExceptions();
        return CommandError;
    }

    QByteArray *bytes = qscriptvalue_cast<QByteArray*>(result.data());
    if (bytes != NULL)
        response->append(*bytes);
    else if (!result.isUndefined())
        response->append(result.toString() + '\n');

    COPYQ_LOG("Scripting engine: finished");

    return CommandSuccess;
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

    key = cm->value("second_shortcut").toString();

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
#endif

    // reload clipboard monitor configuration
    if ( isMonitoring() )
        loadMonitorSettings();
}

void ClipboardServer::shortcutActivated(QxtGlobalShortcut *shortcut)
{
    Arguments args = m_shortcutActions[shortcut];
    QByteArray response;
    doCommand(args, &response);
}
