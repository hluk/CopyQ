/*
    Copyright (c) 2012, Lukas Holecek <hluk@email.cz>

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
#include "clipboardmonitor.h"
#include "mainwindow.h"
#include "clipboardbrowser.h"
#include "clipboarditem.h"
#include "arguments.h"
#include "scriptable.h"
#include "../qt/bytearrayclass.h"

#include <QLocalSocket>
#include <QMimeData>
#include <QTimer>
#include <QScriptEngine>

#ifdef NO_GLOBAL_SHORTCUTS
struct QxtGlobalShortcut {};
#else
#include "../qxt/qxtglobalshortcut.h"
#endif

Q_DECLARE_METATYPE(QByteArray*)

ClipboardServer::ClipboardServer(int &argc, char **argv)
    : App(argc, argv)
    , m_server(NULL)
    , m_monitorserver(NULL)
    , m_socket(NULL)
    , m_wnd(NULL)
    , m_monitor(NULL)
    , m_lastHash(0)
    , m_shortcutActions()
{
    // listen
    m_server = newServer( serverName(), this );
    if ( !m_server->isListening() )
        return;

    // don't exit when all windows closed
    QApplication::setQuitOnLastWindowClosed(false);

    // main window
    m_wnd = new MainWindow;

    // listen
    m_monitorserver = newServer( monitorServerName(), this );
    connect( m_server, SIGNAL(newConnection()),
             this, SLOT(newConnection()) );
    connect( m_monitorserver, SIGNAL(newConnection()),
             this, SLOT(newMonitorConnection()) );

    connect( m_wnd, SIGNAL(destroyed()),
             this, SLOT(quit()) );
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
}

ClipboardServer::~ClipboardServer()
{
    if( isMonitoring() )
        stopMonitoring();

    if (m_socket)
        m_socket->disconnectFromServer();

    delete m_wnd;

    // delete shortcuts manually
    foreach (QxtGlobalShortcut *s, m_shortcutActions.keys())
        delete s;
    m_shortcutActions.clear();
}

void ClipboardServer::monitorStateChanged(QProcess::ProcessState newState)
{
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
    log( tr("Clipboard Monitor: ") +
            m_monitor->readAllStandardError(), LogError );
}

void ClipboardServer::stopMonitoring()
{
    ConfigurationManager *cm = ConfigurationManager::instance();
    cm->setValue("_last_hash", m_lastHash);

    if (m_monitor) {
        m_monitor->disconnect( SIGNAL(stateChanged(QProcess::ProcessState)) );

        if ( m_monitor->state() != QProcess::NotRunning ) {
            log( tr("Clipboard Monitor: Terminating") );

            if (m_socket) {
                m_socket->disconnectFromServer();
                m_monitor->waitForFinished(1000);
            }

            if ( m_monitor->state() != QProcess::NotRunning ) {
                log( tr("Clipboard Monitor: Command 'exit' unsucessful!"),
                     LogError );
                m_monitor->terminate();
                m_monitor->waitForFinished(1000);

                if ( m_monitor->state() != QProcess::NotRunning ) {
                    log( tr("Clipboard Monitor: Cannot terminate process!"),
                         LogError );
                    m_monitor->kill();

                    if ( m_monitor->state() != QProcess::NotRunning ) {
                        log( tr("Clipboard Monitor: Cannot kill process!!!"),
                             LogError );
                    }
                }
            }
        }

        if ( m_monitor->state() == QProcess::NotRunning ) {
            log( tr("Clipboard Monitor: Terminated") );
        }

        m_socket->deleteLater();
        m_socket = NULL;
        m_monitor->deleteLater();
        m_monitor = NULL;
    }
}

void ClipboardServer::startMonitoring()
{
    if ( !m_monitor ) {
        m_monitor = new QProcess(this);
        connect( m_monitor, SIGNAL(stateChanged(QProcess::ProcessState)),
                 this, SLOT(monitorStateChanged(QProcess::ProcessState)) );
        connect( m_monitor, SIGNAL(readyReadStandardError()),
                 this, SLOT(monitorStandardError()) );
        m_monitor->start( QApplication::arguments().at(0),
                          QStringList() << "monitor",
                          QProcess::ReadOnly );
        if ( !m_monitor->waitForStarted(2000) ) {
            log( tr("Cannot start clipboard monitor!"), LogError );
            m_monitor->deleteLater();
            exit(10);
            return;
        }
    }
    m_wnd->browser(0)->setAutoUpdate(true);
}

void ClipboardServer::loadMonitorSettings()
{
    if ( !m_socket || !m_socket->isWritable() )
        return;

    ConfigurationManager *cm = ConfigurationManager::instance();

    QVariantMap settings;
    settings["_last_hash"] = cm->value("_last_hash");
    settings["formats"] = cm->value("formats");
    settings["check_clipboard"] = cm->value("check_clipboard");
#ifdef Q_WS_X11
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
    writeMessage(m_socket, msg);
}

void ClipboardServer::newConnection()
{
    QLocalSocket* client = m_server->nextPendingConnection();
    connect(client, SIGNAL(disconnected()),
            client, SLOT(deleteLater()));

    Arguments args;
    QByteArray msg;
    readMessage(client, &msg);
    QDataStream in(msg);
    in >> args;

    QByteArray client_msg;
    // try to handle command
    int exitCode = doCommand(args, &client_msg, client);
    if ( exitCode == CommandBadSyntax ) {
        client_msg = tr("Bad command syntax. Use -h for help.\n").toLocal8Bit();
    }
    sendMessage(client, client_msg, exitCode);

    client->disconnectFromServer();
}

void ClipboardServer::sendMessage(QLocalSocket* client, const QByteArray &message, int exitCode)
{
    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);
    out << exitCode;
    out.writeRawData( message.constData(), message.length() );
    writeMessage(client, msg);
}

void ClipboardServer::newMonitorConnection()
{
    if (m_socket) {
        m_socket->disconnectFromServer();
        m_socket->deleteLater();
        m_socket = NULL;
    }
    m_socket = m_monitorserver->nextPendingConnection();
    connect( m_socket, SIGNAL(readyRead()),
             this, SLOT(readyRead()) );

    loadMonitorSettings();
}

void ClipboardServer::readyRead()
{
    m_socket->blockSignals(true);
    while ( m_socket && m_socket->bytesAvailable() ) {
        ClipboardItem item;
        QByteArray msg;
        if( !readMessage(m_socket, &msg) ) {
            // something is wrong with the connection -> restart monitor
            log( tr("Incorrect message from Clipboard Monitor."), LogError );
            stopMonitoring();
            startMonitoring();
            return;
        }
        QDataStream in(&msg, QIODevice::ReadOnly);
        in >> item;
        if ( m_lastHash == item.dataHash() )
            continue;
        m_lastHash = item.dataHash();

        m_wnd->addToTab( item.data() );
    }
    if (m_socket)
        m_socket->blockSignals(false);
}

void ClipboardServer::changeClipboard(const ClipboardItem *item)
{
    if ( !m_socket || !m_socket->isWritable() )
        return;

    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);
    out << *item;
    writeMessage(m_socket, msg);
}

CommandStatus ClipboardServer::doCommand(
        Arguments &args, QByteArray *response, QLocalSocket *client)
{
    if ( args.length() <= Arguments::Rest )
        return CommandBadSyntax;
    QString cmd = args.at(Arguments::Rest);

    QScriptEngine *engine = new QScriptEngine(m_wnd);
    Scriptable scriptable(m_wnd, client);
    scriptable.initEngine(engine, args.at(Arguments::CurrentPath));

    QScriptValue result;
    QScriptValueList fnArgs;

    QScriptValue fn = engine->globalObject().property(cmd);
    if ( !fn.isFunction() )
        return CommandBadSyntax;

    for ( int i = Arguments::Rest + 1; i < args.length(); ++i )
        fnArgs.append( scriptable.newByteArray(args.at(i)) );

    result = fn.call(QScriptValue(), fnArgs);

    if ( engine->hasUncaughtException() ) {
        response->append(engine->uncaughtException().toString() + '\n');
        engine->clearExceptions();
        return CommandError;
    }

    QByteArray *bytes = qscriptvalue_cast<QByteArray*>(result.data());
    if (bytes != NULL)
        response->append(*bytes);
    else if (!result.isUndefined())
        response->append(result.toString() + '\n');

    return CommandSuccess;
}

Arguments *ClipboardServer::createGlobalShortcut(const QString &shortcut)
{
#ifdef NO_GLOBAL_SHORTCUTS
    return NULL;
#else
    if ( shortcut.isEmpty() )
        return NULL;

    QKeySequence keyseq(shortcut, QKeySequence::NativeText);
    QxtGlobalShortcut *s = new QxtGlobalShortcut(keyseq, this);
    connect( s, SIGNAL(activated(QxtGlobalShortcut*)),
             this, SLOT(shortcutActivated(QxtGlobalShortcut*)) );

    return &m_shortcutActions[s];
#endif
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

    key = cm->value("edit_shortcut").toString();
    args = createGlobalShortcut(key);
    if (args)
        args->append("edit");

    key = cm->value("second_shortcut").toString();
    args = createGlobalShortcut(key);
    if (args) {
        args->append("select");
        args->append("1");
    }
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
