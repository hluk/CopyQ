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

#include "clientsocket.h"

#include "common/arguments.h"
#include "common/client_server.h"
#include "common/common.h"

#include <QLocalServer>

void ClientSocket::sendMessage(const QByteArray &message, int exitCode)
{
#ifdef COPYQ_LOG_DEBUG
    quintptr id = property("id").toInt();
#endif

    COPYQ_LOG( QString("%1: Sending message to client (exit code: %2).")
               .arg(id)
               .arg(exitCode) );

    if ( m_socket->state() == QLocalSocket::ConnectedState ) {
        QByteArray msg;
        QDataStream out(&msg, QIODevice::WriteOnly);
        out << exitCode;
        out.writeRawData( message.constData(), message.length() );
        if ( writeMessage(m_socket, msg) )
            COPYQ_LOG( QString("%1: Message send to client.").arg(id) );
        else
            COPYQ_LOG( QString("%1: Failed to send message to client!").arg(id) );
    } else {
        COPYQ_LOG( QString("%1: Client disconnected!").arg(id) );
    }
}

void ClientSocket::close()
{
    m_socket->close();
}

void ClientSocket::onReadyRead()
{
    QByteArray msg;
    if ( !readMessage(m_socket, &msg) ) {
        log( tr("Failed to read message from client!"), LogError );
        return;
    }

    emit messageReceived(msg);
}

void ClientSocket::onError(QLocalSocket::LocalSocketError error)
{
    if (error == QLocalSocket::PeerClosedError)
        COPYQ_LOG( QString("%1: Disconnected from client.").arg(property("id").toInt()) );
    else
        log( m_socket->errorString(), LogError );
}

void ClientSocket::onDisconnected()
{
    emit disconnected();
}

ClientSocket::ClientSocket(QLocalSocket *socket, QObject *parent)
    : QObject(parent)
    , m_socket(socket)
{
    m_socket->setParent(this);
    connect( m_socket, SIGNAL(disconnected()),
             this, SLOT(onDisconnected()) );
    connect( m_socket, SIGNAL(error(QLocalSocket::LocalSocketError)),
             this, SLOT(onError(QLocalSocket::LocalSocketError)) );

#ifdef COPYQ_LOG_DEBUG
    setProperty("id", m_socket->socketDescriptor());
    COPYQ_LOG( QString("%1: New client connection %1").arg(property("id").toInt()) );
#endif
}

Arguments ClientSocket::readArguments()
{
    Arguments args;
    QByteArray msg;

    if ( readMessage(m_socket, &msg) ) {
        COPYQ_LOG( QString("%1: Message received from client.").arg(property("id").toInt()) );
        QDataStream input(msg);
        input >> args;
        connect( m_socket, SIGNAL(readyRead()),
                 this, SLOT(onReadyRead()) );
    } else {
        log( tr("Failed to read message from client!"), LogError );
    }

    return args;
}
