/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

#include "common/arguments.h"
#include "common/client_server.h"
#include "common/common.h"
#include "platform/platformnativeinterface.h"
#include "platform/platformwindow.h"

#include <QCoreApplication>
#include <QFile>

ClipboardClient::ClipboardClient(int &argc, char **argv, int skipArgc, const QString &sessionName)
    : QObject()
    , App(createPlatformNativeInterface()->createClientApplication(argc, argv), sessionName)
    , m_client()
    , m_args(argc, argv, skipArgc + 1)
{
    // client socket
    connect( &m_client, SIGNAL(readyRead()),
             this, SLOT(readyRead()) );
    connect( &m_client, SIGNAL(readChannelFinished()),
             this, SLOT(readFinished()) );
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

    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);
    out << m_args;
    if ( writeMessage(&m_client, msg) ) {
        COPYQ_LOG("Message send to server.");
    } else {
        COPYQ_LOG("Failed to send message to server!");
    }
}

void ClipboardClient::readyRead()
{
    m_client.blockSignals(true);

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
                PlatformWindowPtr window = createPlatformNativeInterface()->getWindow(wid);
                if (window)
                    window->raise();
            } else {
                QFile f;
                f.open((exitCode == CommandSuccess) ? stdout : stderr, QIODevice::WriteOnly);
                f.write( msg.constData() + i, len - i );
            }
        } else if (exitCode == CommandReadInput) {
            COPYQ_LOG("Sending standard input.");
            QFile in;
            in.open(stdin, QIODevice::ReadOnly);
            if ( !writeMessage(&m_client, in.readAll()) ) {
                COPYQ_LOG("Failed to send standard input to server!");
                exit(1);
                break;
            }
        }

        COPYQ_LOG( QString("Message received with exit code %1.").arg(exitCode) );

        if (exitCode == CommandFinished || exitCode == CommandBadSyntax || exitCode == CommandError) {
            exit(exitCode);
            break;
        }
        if (exitCode == CommandExit) {
            exit(0);
            break;
        }
    }

    m_client.blockSignals(false);
}

void ClipboardClient::readFinished()
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
