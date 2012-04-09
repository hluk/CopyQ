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

#include "clipboardclient.h"
#include "clipboardserver.h"
#include "client_server.h"
#include "arguments.h"
#include <QFile>
#include <iostream>

ClipboardClient::ClipboardClient(int &argc, char **argv)
    : App(argc, argv)
    , m_client( new QLocalSocket(this) )
    , m_args(argc, argv)
{
    // client socket
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
    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);
    out << m_args;
    writeMessage(m_client, msg);
}

void ClipboardClient::readyRead()
{
    int exit_code, i, len;
    QByteArray msg;

    if( !readMessage(m_client, &msg) )
        exit(1);

    QDataStream in(&msg, QIODevice::ReadOnly);
    in >> exit_code;
    i = sizeof(exit_code);

    len = msg.length();
    if (len > i) {
        if ( m_args.length() > 0 &&
             (m_args.at(0) == "toggle" || m_args.at(0) == "show") ) {
            WId wid = (WId)(QString(msg.constData()+i).toLongLong());
            raiseWindow(wid);
        } else {
            std::ostream &out = (exit_code == 0) ? std::cout : std::cerr;
            out.write( msg.constData()+i, len-i );
        }
    }

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
