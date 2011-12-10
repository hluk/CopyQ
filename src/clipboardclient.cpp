#include "clipboardclient.h"
#include "clipboardserver.h"
#include "client_server.h"
#include "arguments.h"
#include <QFile>
#include <iostream>

ClipboardClient::ClipboardClient(int &argc, char **argv) :
    App(argc, argv), m_args(argc, argv)
{
    // client socket
    m_client = new QLocalSocket(this);
    connect( m_client, SIGNAL(readyRead()),
             this, SLOT(readyRead()) );
    connect( m_client, SIGNAL(readChannelFinished()),
             this, SLOT(readFinnished()) );
    connect( m_client, SIGNAL(error(QLocalSocket::LocalSocketError)),
             this, SLOT(error(QLocalSocket::LocalSocketError)) );
    connect( m_client, SIGNAL(connected()),
             this, SLOT(sendMessage()));

    // connect to server
    m_client->connectToServer( ClipboardServer::serverName() );
}

#include <QDebug>
void ClipboardClient::sendMessage()
{
    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);
    out << m_args;
    writeMessage(m_client, msg);
}

void ClipboardClient::readyRead()
{
    int exit_code, i;
    QByteArray msg;

    if( !readMessage(m_client, &msg) )
        exit(1);

    QDataStream in(&msg, QIODevice::ReadOnly);
    in >> exit_code;
    i = sizeof(exit_code);

    std::cout.write( msg.constData()+i, msg.length()-i );

    exit(exit_code);
}

void ClipboardClient::readFinnished()
{
    exit();
}

void ClipboardClient::error(QLocalSocket::LocalSocketError socketError)
{
    switch (socketError) {
    case QLocalSocket::ServerNotFoundError:
        log( tr("Cannot connect to server! Start CopyQ server first."),
             LogError );
        break;
    case QLocalSocket::ConnectionRefusedError:
        log( tr("Connection refused by server!"), LogError );
        break;
    case QLocalSocket::PeerClosedError:
        log( tr("Connection lost!"), LogError );
        break;
    default:
        log( m_client->errorString(), LogError );
    }
    exit(1);
}
