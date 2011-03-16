#include "clipboardclient.h"
#include "clipboardserver.h"
#include "client_server.h"
#include "arguments.h"
#include <QFile>
#include <iostream>

ClipboardClient::ClipboardClient(int &argc, char **argv) :
        App(argc, argv)
{
    // parse arguments
    Arguments args(argc, argv);
    m_msg = args.message();

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

void ClipboardClient::sendMessage()
{
    QDataStream out(m_client);
    out << (quint32)m_msg.length() << m_msg;
}

void ClipboardClient::readyRead()
{
    QDataStream in(m_client);

    quint32 exit_code;
    while( m_client->bytesAvailable() < (qint64)sizeof(quint32) )
        m_client->waitForReadyRead();
    in >> exit_code;

    quint32 sz;
    while( m_client->bytesAvailable() < (qint64)sizeof(quint32) )
        m_client->waitForReadyRead();
    in >> sz;

    QByteArray msg;
    while( m_client->bytesAvailable() < sz )
        m_client->waitForReadyRead();
    in >> msg;

    QByteArray bytes = qUncompress(msg);
    std::cout.write( bytes.constData(), bytes.length() );

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
