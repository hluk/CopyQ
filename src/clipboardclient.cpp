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

#include "clipboardclient.h"

#include "arguments.h"
#include "client_server.h"
#include "clipboardserver.h"

#include <cstdio>
#include <QFile>

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
    int exitCode, i, len;
    QByteArray msg;
    while ( m_client && m_client->bytesAvailable() ) {
        if( !readMessage(m_client, &msg) )
            exit(1);

        QDataStream in(&msg, QIODevice::ReadOnly);
        in >> exitCode;
        i = sizeof(exitCode);

        len = msg.length();
        if (len > i) {
            if (exitCode == CommandActivateWindow) {
                WId wid = (WId)(QByteArray(msg.constData()+i).toLongLong());
                raiseWindow(wid);
            } else {
                QFile f;
                f.open((exitCode == 0) ? stdout : stderr, QIODevice::WriteOnly);
                QDataStream out(&f);
                out.writeBytes( msg.constData() + i, len - i );
            }
        }

        if (exitCode != CommandActivateWindow)
            exit(exitCode);
    }
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
