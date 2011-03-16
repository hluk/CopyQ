#include "clipboardserver.h"
#include "clipboardmonitor.h"
#include "mainwindow.h"
#include "clipboardbrowser.h"
#include "clipboarditem.h"
#include <QLocalSocket>
#include <QMimeData>
#include "arguments.h"

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

    // commands send from client to server
    m_commands["toggle"] = Cmd_Toggle;
    m_commands["exit"]   = Cmd_Exit;
    m_commands["menu"]   = Cmd_Menu;
    m_commands["action"] = Cmd_Action;
    m_commands["add"]    = Cmd_Add;
    m_commands["write"]  = Cmd_Write;
    m_commands["_write"] = Cmd_WriteNoUpdate;
    m_commands["edit"]   = Cmd_Edit;
    m_commands["select"] = Cmd_Select;
    m_commands["remove"] = Cmd_Remove;
    m_commands["length"] = Cmd_Length;
    m_commands["size"]   = Cmd_Length;
    m_commands["count"]  = Cmd_Length;
    m_commands["list"]   = Cmd_List;
    m_commands["read"]   = Cmd_Read;

    // listen
    m_monitorserver = newServer( monitorServerName(), this );
    connect( m_server, SIGNAL(newConnection()),
             this, SLOT(newConnection()) );
    connect( m_monitorserver, SIGNAL(newConnection()),
             this, SLOT(newMonitorConnection()) );

    connect( m_wnd->browser(), SIGNAL(changeClipboard(const ClipboardItem*)),
             this, SLOT(changeClipboard(const ClipboardItem*)));

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
        qDebug() << m_monitor->readAllStandardError();
        qDebug( tr("Clipboard Monitor ERROR: Clipboard monitor crashed! (%s)").toLocal8Bit(),
                m_monitor->errorString().toLocal8Bit().constData() );
        m_wnd->showError( tr("Clipboard Monitor ERROR: Clipboard monitor crashed! (%1)").arg(m_monitor->errorString()) );
        // restart clipboard monitor
        stopMonitoring();
        startMonitoring();
    } else if (newState == QProcess::Starting) {
        qDebug( tr("Clipboard Monitor: Starting").toLocal8Bit() );
    } else if (newState == QProcess::Running) {
        qDebug( tr("Clipboard Monitor: Started").toLocal8Bit() );
    }
}

void ClipboardServer::monitorStandardError()
{
    qDebug( tr("Clipboard Monitor: ").toLocal8Bit() +
            m_monitor->readAllStandardError() );
}

void ClipboardServer::stopMonitoring()
{
    if (m_monitor) {
        m_monitor->disconnect( SIGNAL(stateChanged(QProcess::ProcessState)) );

        if ( m_monitor->state() != QProcess::NotRunning ) {
            qDebug( tr("Clipboard Monitor: Terminating").toLocal8Bit() );

            if (m_socket) {
                m_socket->disconnectFromServer();
                m_socket->deleteLater();
                m_socket = NULL;
                m_monitor->waitForFinished(1000);
            }

            if ( m_monitor->state() != QProcess::NotRunning ) {
                qDebug( tr("Clipboard Monitor ERROR: Command 'exit' unsucessful!").toLocal8Bit() );
                m_monitor->terminate();
                m_monitor->waitForFinished(1000);

                if ( m_monitor->state() != QProcess::NotRunning ) {
                    qDebug( tr("Clipboard Monitor ERROR: Cannot terminate process!").toLocal8Bit() );
                    m_monitor->kill();

                    if ( m_monitor->state() != QProcess::NotRunning ) {
                        qDebug( tr("Clipboard Monitor ERROR: Cannot kill process!!!").toLocal8Bit() );
                    }
                }
            }
        }

        if ( m_monitor->state() == QProcess::NotRunning ) {
            qDebug( tr("Clipboard Monitor: Terminated").toLocal8Bit() );
        }

        m_monitor->deleteLater();
        m_monitor = NULL;
    }
    m_wnd->browser()->setAutoUpdate(false);
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
            qDebug( tr("ERROR: Cannot start clipbord monitor!").toLocal8Bit() );
            delete m_monitor;
            exit(10);
            return;
        }
    }
    m_wnd->browser()->setAutoUpdate(true);
}

void ClipboardServer::newConnection()
{
    QLocalSocket* client = m_server->nextPendingConnection();
    connect(client, SIGNAL(disconnected()),
            client, SLOT(deleteLater()));

    QByteArray msg;
    if( !readBytes(client, msg) ) {
        client->deleteLater();
        client->disconnectFromServer();
        return;
    }

    Arguments args(msg);
    QByteArray client_msg;
    // try to handle command
    if ( !doCommand(args, client_msg) ) {
        sendMessage( client, tr("Bad command syntax. Use -h for help.\n"), 2 );
    } else {
        sendMessage(client, client_msg);
    }

    client->deleteLater();
    client->disconnectFromServer();
}

void ClipboardServer::sendMessage(QLocalSocket* client, const QByteArray &message, int exit_code)
{
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    QByteArray zipped = qCompress(message);
    out << (quint32)exit_code << (quint32)zipped.length() << zipped;
    client->write(bytes);
    client->flush();
}

void ClipboardServer::newMonitorConnection()
{
    if (m_socket) {
        m_socket->disconnectFromServer();
        m_socket->deleteLater();
    }
    m_socket = m_monitorserver->nextPendingConnection();
    connect( m_socket, SIGNAL(readyRead()),
             this, SLOT(readyRead()) );
}

void ClipboardServer::readyRead()
{
    QByteArray msg;
    if( !readBytes(m_socket, msg) ) {
        // something wrong sith connection
        // -> restart monitor
        stopMonitoring();
        startMonitoring();
        return;
    }

    QByteArray bytes = qUncompress(msg);
    QDataStream in2(&bytes, QIODevice::ReadOnly);

    ClipboardItem item;
    in2 >> item;

    QMimeData *data = item.data();
    ClipboardBrowser *c = m_wnd->browser();

    // don't allow clipboard to be empty
    if ( data->formats().isEmpty() || (data->hasText() && data->text().isEmpty()) ) {
        // reset clipboard content
        if ( c->length() > 0 ) {
            qDebug( tr("NOTE: Clipboard or selection is empty -> resetting previous content.").toLocal8Bit() );
            c->moveToClipboard(0);
        }
    } else {
        c->add( cloneData(*data) );
    }
}

void ClipboardServer::changeClipboard(const ClipboardItem *item)
{
    if ( !m_socket || !m_socket->isWritable() )
        return;

    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out << *item;

    QDataStream(m_socket) << (quint32)bytes.length() << bytes;
    m_socket->flush();
}

bool ClipboardServer::doCommand(Arguments &args, QByteArray &response)
{
    QString cmd;
    args >> cmd;
    if ( args.error() )
        return false;

    ClipboardBrowser *c = m_wnd->browser();
    bool noupdate = false;
    QString mime;
    QMimeData *data;
    int row;

    switch( m_commands.value(cmd, Cmd_Unknown) ) {

    // show/hide main window
    case Cmd_Toggle:
        if ( !args.atEnd() )
            return false;
        m_wnd->toggleVisible();
        break;

    // exit server
    case Cmd_Exit:
        if ( !args.atEnd() )
            return false;
        // close client and exit
        response = tr("Terminating server.\n").toLocal8Bit();
        exit();
        break;

    // show menu
    case Cmd_Menu:
        if ( !args.atEnd() )
            return false;
        m_wnd->showMenu();
        break;

    // show action dialog or run action on item
    // action
    // action [row] "cmd" "[sep]"
    case Cmd_Action:
        if ( args.atEnd() ) {
            m_wnd->openActionDialog(0);
        } else {
            QString cmd, sep;

            args >> 0 >> row >> cmd >> QString('\n') >> sep;

            if ( !args.finished() )
                return false;

            ConfigurationManager::Command command;
            command.cmd = cmd;
            command.output = true;
            command.input = true;
            command.sep = sep;
            command.wait = false;
            m_wnd->action(row, &command);
        }
        break;

    // add new items
    case Cmd_Add:
        if ( args.atEnd() )
            return false;

        if ( isMonitoring() ) c->setAutoUpdate(false);
        while( !args.atEnd() ) {
            c->add( args.toString() );
        }
        if ( isMonitoring() ) c->setAutoUpdate(true);

        c->updateClipboard();
        break;

    // add new items
    case Cmd_WriteNoUpdate:
        noupdate = true;
    case Cmd_Write:
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
        break;

    // edit clipboard item
    case Cmd_Edit:
        args >> 0 >> row;
        if ( args.finished() ) {
            c->setCurrent(row);
            c->openEditor();
        } else {
            return false;
        }
        break;

    // set current item
    // select [row=1]
    case Cmd_Select:
        args >> 0 >> row;
        if ( !args.finished() )
            return false;
        c->moveToClipboard(row);
        break;

    // remove item from clipboard
    // remove [row=0] ...
    case Cmd_Remove:
        if ( args.finished() ) {
            c->setCurrent(0);
            c->remove();
        }
        else {
            int row;
            do {
                args >> 0 >> row;
                c->setCurrent(row);
                c->remove();
            } while ( !args.finished() );
        }
        break;

    case Cmd_Length:
        if ( args.finished() ) {
            response = QString("%1\n").arg(c->length()).toLocal8Bit();
        } else {
            return false;
        }
        break;

    // print items in given rows, format can have two arguments %1:item %2:row
    // list [format="%1\n"|row=0] ...
    case Cmd_List:
        if ( args.finished() ) {
            response = c->itemText(0).toLocal8Bit()+'\n';
        } else {
            QString fmt("%1\n");
            do {
                args >> 0 >> row;
                if ( args.error() ) {
                    args.back();
                    args >> fmt;
                    fmt.replace(QString("\\n"),QString('\n'));
                } else {
                    response.append( fmt.arg( c->itemText(row) ).arg(row) );
                }
            } while( !args.atEnd() );
        }
        break;

    // print items in given rows, format can have two arguments %1:item %2:row
    // read [mime="text/plain"|row=0] ...
    case Cmd_Read:
        mime = QString("text/plain");

        if ( args.atEnd() ) {
            response = c->itemData(0)->data(mime);
        } else {
            QString mime("text/plain");
            do {
                args >> 0 >> row;
                if ( args.error() ) {
                    args.back();
                    args >> mime;
                } else {
                    response.append( c->itemData(row)->data(mime) );
                }
            } while( !args.atEnd() );
        }
        break;

    default:
        return false;
    }

    return true;
}

void ClipboardServer::loadSettings()
{
    // restart clipboard monitor to reload its configuration
    if ( isMonitoring() ) {
        stopMonitoring();
        startMonitoring();
    }
}
