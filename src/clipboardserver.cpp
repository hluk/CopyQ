#include "clipboardserver.h"
#include "clipboardmonitor.h"
#include "mainwindow.h"
#include "clipboardbrowser.h"
#include "clipboarditem.h"
#include <QLocalSocket>
#include <QMimeData>
#include "arguments.h"

#ifdef NO_GLOBAL_SHORTCUTS
struct QxtGlobalShortcut {};
#else
#include "../qxt/qxtglobalshortcut.h"
#endif

ClipboardServer::ClipboardServer(int &argc, char **argv) :
        App(argc, argv), m_socket(NULL), m_wnd(NULL), m_monitor(NULL)
{
    // listen
    m_server = newServer( serverName(), this );
    if ( !m_server->isListening() )
        return;

    // don't exit when all windows closed
    QApplication::setQuitOnLastWindowClosed(false);

    // main window
    m_wnd = new MainWindow;

    // notify window if configuration changes
    ConfigurationManager *cm = ConfigurationManager::instance();
    connect( cm, SIGNAL(configurationChanged()),
             this, SLOT(loadSettings()) );

    // listen
    m_monitorserver = newServer( monitorServerName(), this );
    connect( m_server, SIGNAL(newConnection()),
             this, SLOT(newConnection()) );
    connect( m_monitorserver, SIGNAL(newConnection()),
             this, SLOT(newMonitorConnection()) );

    connect( m_wnd, SIGNAL(changeClipboard(const ClipboardItem*)),
             this, SLOT(changeClipboard(const ClipboardItem*)));

    loadSettings();

    // run clipboard monitor
    startMonitoring();
}

ClipboardServer::~ClipboardServer()
{
    if( isMonitoring() ) {
        stopMonitoring();
    }

    if(m_wnd) {
        delete m_wnd;
    }

    if (m_socket) {
        m_socket->disconnectFromServer();
        m_socket->deleteLater();
    }
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
    if (m_monitor) {
        m_monitor->disconnect( SIGNAL(stateChanged(QProcess::ProcessState)) );

        if ( m_monitor->state() != QProcess::NotRunning ) {
            log( tr("Clipboard Monitor: Terminating") );

            if (m_socket) {
                m_socket->disconnectFromServer();
                m_socket->deleteLater();
                m_socket = NULL;
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

        m_monitor->deleteLater();
        m_monitor = NULL;
    }
    m_wnd->browser(0)->setAutoUpdate(false);
}

void ClipboardServer::startMonitoring()
{
    if ( !m_monitor ) {
        m_monitor = new QProcess;
        connect( m_monitor, SIGNAL(stateChanged(QProcess::ProcessState)),
                 this, SLOT(monitorStateChanged(QProcess::ProcessState)) );
        connect( m_monitor, SIGNAL(readyReadStandardError()),
                 this, SLOT(monitorStandardError()) );
        m_monitor->start( QApplication::arguments().at(0),
                          QStringList() << "monitor",
                          QProcess::ReadOnly );
        if ( !m_monitor->waitForStarted(2000) ) {
            log( tr("Cannot start clipboard monitor!"), LogError );
            delete m_monitor;
            exit(10);
            return;
        }
    }
    m_wnd->browser(0)->setAutoUpdate(true);
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
    if ( !doCommand(args, &client_msg) ) {
        sendMessage( client, tr("Bad command syntax. Use -h for help.\n"), 2 );
    } else {
        QApplication::processEvents();
        sendMessage(client, client_msg);
    }

    client->disconnectFromServer();
}

void ClipboardServer::sendMessage(QLocalSocket* client, const QByteArray &message, int exit_code)
{
    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);
    out << exit_code;
    out.writeRawData( message.constData(), message.length() );
    writeMessage(client, msg);
}

void ClipboardServer::newMonitorConnection()
{
    if (m_socket)
        m_socket->disconnectFromServer();
    m_socket = m_monitorserver->nextPendingConnection();
    connect(m_socket, SIGNAL(disconnected()),
            m_socket, SLOT(deleteLater()));
    connect( m_socket, SIGNAL(readyRead()),
             this, SLOT(readyRead()) );
}

void ClipboardServer::readyRead()
{
    while ( m_socket->bytesAvailable() ) {
        ClipboardItem item;
        QByteArray msg;
        if( !readMessage(m_socket, &msg) ) {
            // something wrong with connection
            // -> restart monitor
            stopMonitoring();
            startMonitoring();
            return;
        }
        QDataStream in(&msg, QIODevice::ReadOnly);
        in >> item;

        QMimeData *data = item.data();
        ClipboardBrowser *c = m_wnd->browser(0);

        c->setAutoUpdate(false);
        c->add( cloneData(*data) );
        c->setAutoUpdate(true);
    }
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

bool ClipboardServer::doCommand(Arguments &args, QByteArray *response)
{
    QString cmd;
    args >> cmd;
    if ( args.error() )
        return false;

    ClipboardBrowser *c = m_wnd->browser(0);
    bool noupdate = false;
    QString mime;
    QMimeData *data;
    int row;

    // show/hide main window
    if (cmd == "show") {
        if ( !args.atEnd() )
            return false;
        m_wnd->showWindow();
        response->append(QString::number((qlonglong)m_wnd->winId()));
    }
    else if (cmd == "hide") {
        if ( !args.atEnd() )
            return false;
        m_wnd->hideWindow();
    }
    else if (cmd == "toggle") {
        if ( !args.atEnd() )
            return false;
        if ( m_wnd->isVisible() ) {
            m_wnd->hideWindow();
        } else {
            m_wnd->showWindow();
            response->append(QString::number((qlonglong)m_wnd->winId()));
        }
    }

    // exit server
    else if (cmd == "exit") {
        if ( !args.atEnd() )
            return false;
        // close client and exit
        *response = tr("Terminating server.\n").toLocal8Bit();
        exit();
    }

    // show menu
    else if (cmd == "menu") {
        if ( !args.atEnd() )
            return false;
        m_wnd->showMenu();
    }

    // show action dialog or run action on item
    // action
    // action [[row] ... ["cmd" "[sep]"]]
    else if (cmd == "action") {
        args >> 0 >> row;
        c->setCurrent(row);
        while ( !args.finished() ) {
            args >> row;
            if (args.error())
                break;
            c->setCurrent(row, false, true);
        }
        if ( !args.error() ) {
            m_wnd->openActionDialog(-1);
        } else {
            QString cmd, sep;

            args.back();
            args >> cmd >> QString('\n') >> sep;

            if ( !args.finished() )
                return false;

            Command command;
            command.cmd = cmd;
            command.output = true;
            command.input = true;
            command.sep = sep;
            command.wait = false;
            m_wnd->action(c->selectedText(), &command);
        }
    }

    // add new items
    else if (cmd == "add") {
        if ( args.atEnd() )
            return false;

        if ( isMonitoring() ) c->setAutoUpdate(false);
        c->setUpdatesEnabled(false);

        while( !args.atEnd() ) {
            c->add( args.toString() );
        }

        c->setUpdatesEnabled(true);
        if ( isMonitoring() ) c->setAutoUpdate(true);

        c->updateClipboard();
    }

    // add new items
    else if (cmd == "write" || cmd == "_write") {
        noupdate = cmd.startsWith('_');
        data = new QMimeData;
        do {
            QByteArray bytes;
            args >> mime >> bytes;

            if ( args.error() ) {
                delete data;
                data = NULL;
                return false;
            }

            data->setData( mime, bytes );
        } while ( !args.atEnd() );

        if ( noupdate && isMonitoring() )
            c->setAutoUpdate(false);

        c->add(data);

        if ( noupdate && isMonitoring() )
            c->setAutoUpdate(true);
    }

    // edit clipboard item
    // edit [row=0] ...
    else if (cmd == "edit") {
        args >> 0 >> row;
        c->setCurrent(row);
        while ( !args.finished() ) {
            args >> row;
            if ( args.error() )
                return false;
            c->setCurrent(row, false, true);
        }
        c->openEditor();
    }

    // set current item
    // select [row=0]
    else if (cmd == "select") {
        args >> 0 >> row;
        if ( !args.finished() )
            return false;
        c->moveToClipboard(row);
    }

    // remove item from clipboard
    // remove [row=0] ...
    else if (cmd == "remove") {
        args >> 0 >> row;
        c->setCurrent(row);
        while ( !args.finished() ) {
            args >> row;
            if ( args.error() )
                return false;
            c->setCurrent(row, false, true);
        }
        c->remove();
    }

    else if (cmd == "length" || cmd == "size" || cmd == "count") {
        if ( args.finished() ) {
            *response = QString("%1\n").arg(c->length()).toLocal8Bit();
        } else {
            return false;
        }
    }

    // read [mime="text/plain"|row] ...
    else if (cmd == "read") {
        mime = QString("text/plain");

        if ( args.atEnd() ) {
            const QMimeData *data = QApplication::clipboard()->mimeData();
            if (data)
                response->append( data->data(mime) );
        } else {
            do {
                args >> row;
                if ( args.error() ) {
                    args.back();
                    args >> mime;
                    args >> -1 >> row;
                }

                const QMimeData *data = (row >= 0) ?
                            c->itemData(row) :
                            QApplication::clipboard()->mimeData();

                if (data) {
                    if (mime == "?")
                        response->append( data->formats().join(" ")+'\n' );
                    else
                        response->append( data->data(mime) );
                }
            } while( !args.atEnd() );
        }
    }

    // clipboard [mime="text/plain"]
    else if (cmd == "clipboard") {
        args >> QString("text/plain") >> mime;
        QClipboard *clipboard = QApplication::clipboard();
        response->append( clipboard->mimeData()->data(mime) );
    }

#ifdef Q_WS_X11
    // selection [mime="text/plain"]
    else if (cmd == "selection") {
        args >> QString("text/plain") >> mime;
        QClipboard *clipboard = QApplication::clipboard();
        response->append( clipboard->mimeData(QClipboard::Selection)->data(mime) );
    }
#endif

    // unknown command
    else {
        return false;
    }

    return true;
}

Arguments *ClipboardServer::createGlobalShortcut(const QString &shortcut)
{
#ifdef NO_GLOBAL_SHORTCUTS
    return NULL;
#else
    if (shortcut == tr("(No Shortcut)") || shortcut.isEmpty())
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
    ConfigurationManager *cm = ConfigurationManager::instance(m_wnd);

#ifndef NO_GLOBAL_SHORTCUTS
    // set global shortcuts
    QString key;
    Arguments *args;

    foreach (QxtGlobalShortcut *s, m_shortcutActions.keys())
        s->deleteLater();
    m_shortcutActions.clear();

    key = cm->value("toggle_shortcut").toString();
    args = createGlobalShortcut(key);
    if (args)
        args->append("toggle");

    key = cm->value("menu_shortcut").toString();
    args = createGlobalShortcut(key);
    if (args)
        args->append("menu");
#endif

    // restart clipboard monitor to reload its configuration
    if ( isMonitoring() ) {
        stopMonitoring();
        startMonitoring();
    }
}

void ClipboardServer::shortcutActivated(QxtGlobalShortcut *shortcut)
{
    Arguments args = m_shortcutActions[shortcut];
    QByteArray response;
    doCommand(args, &response);
}
