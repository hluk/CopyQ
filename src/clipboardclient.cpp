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
#include "platform/platformnativeinterface.h"

#include <cstdio>
#include <QCoreApplication>
#include <QFile>

ClipboardClient::ClipboardClient(int &argc, char **argv)
    : QObject()
    , App(new QCoreApplication(argc, argv))
    , m_client()
    , m_args(argc, argv)
{
    // client socket
    connect( &m_client, SIGNAL(readyRead()),
             this, SLOT(readyRead()) );
    connect( &m_client, SIGNAL(readChannelFinished()),
             this, SLOT(readFinnished()) );
    connect( &m_client, SIGNAL(error(QLocalSocket::LocalSocketError)),
             this, SLOT(error(QLocalSocket::LocalSocketError)) );
    connect( &m_client, SIGNAL(connected()),
             this, SLOT(sendMessage()));

    // connect to server
    m_client.connectToServer( clipboardServerName() );
}

void ClipboardClient::sendMessage()
{
    COPYQ_LOG("Sending message to server.");

    {
        QByteArray msg;
        QDataStream out(&msg, QIODevice::WriteOnly);
        out << m_args;
        writeMessage(&m_client, msg);
    }

    COPYQ_LOG("Message send to server.");
}

void ClipboardClient::readyRead()
{
    COPYQ_LOG("Receiving message from server.");

    int exitCode, i, len;
    QByteArray msg;
    while ( m_client.bytesAvailable() ) {
        if( !readMessage(&m_client, &msg) )
            exit(1);

        QDataStream in(&msg, QIODevice::ReadOnly);
        in >> exitCode;
        i = sizeof(exitCode);

        len = msg.length();
        if (len > i) {
            if (exitCode == CommandActivateWindow) {
                COPYQ_LOG("Activating window.");
                WId wid = (WId)(QByteArray(msg.constData()+i).toLongLong());
                createPlatformNativeInterface()->raiseWindow(wid);
            } else {
                QFile f;
                f.open((exitCode == CommandSuccess) ? stdout : stderr, QIODevice::WriteOnly);
                f.write( msg.constData() + i, len - i );
            }
        }

        COPYQ_LOG( QString("Message received with exit code %1.").arg(exitCode) );

        if (exitCode == CommandFinished || exitCode == CommandBadSyntax || exitCode == CommandError) {
            exit(exitCode);
            break;
        }
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
        log( m_client.errorString(), LogError );
    }
    exit(1);
}
