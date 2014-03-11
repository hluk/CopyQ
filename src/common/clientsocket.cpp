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

#define SOCKET_LOG(text) \
    COPYQ_LOG( QString("%1: %2").arg(property("id").toInt()).arg(text) )

ClientSocket::ClientSocket()
    : QObject()
    , m_socket()
    , m_deleteAfterDisconnected(false)
    , m_closed(true)
{
}

ClientSocket::ClientSocket(QLocalSocket *socket, QObject *parent)
    : QObject(parent)
    , m_socket(socket)
    , m_deleteAfterDisconnected(false)
    , m_closed(false)
{
    m_socket->setParent(this);
    connect( m_socket.data(), SIGNAL(stateChanged(QLocalSocket::LocalSocketState)),
             this, SLOT(onStateChanged(QLocalSocket::LocalSocketState)) );
    connect( m_socket.data(), SIGNAL(error(QLocalSocket::LocalSocketError)),
             this, SLOT(onError(QLocalSocket::LocalSocketError)) );

    onStateChanged(m_socket->state());

#ifdef COPYQ_LOG_DEBUG
    setProperty("id", m_socket->socketDescriptor());
    SOCKET_LOG("New connection");
#endif
}

void ClientSocket::start()
{
    connect( m_socket, SIGNAL(readyRead()),
             this, SLOT(onReadyRead()), Qt::UniqueConnection );
    QMetaObject::invokeMethod(this, "onReadyRead", Qt::QueuedConnection);
}

void ClientSocket::sendMessage(const QByteArray &message, int exitCode)
{
    SOCKET_LOG( QString("Sending message to client (exit code: %1).").arg(exitCode) );

    if ( m_socket.isNull() ) {
        SOCKET_LOG("Cannot send message to client. Socket is already deleted.");
    } else if ( m_socket->state() == QLocalSocket::ConnectedState ) {
        QByteArray msg;
        QDataStream out(&msg, QIODevice::WriteOnly);
        out << exitCode;
        out.writeRawData( message.constData(), message.length() );
        if ( writeMessage(m_socket, msg) )
            SOCKET_LOG("Message send to client.");
        else
            SOCKET_LOG("Failed to send message to client!");
    } else {
        SOCKET_LOG("Client disconnected!");
    }
}

void ClientSocket::deleteAfterDisconnected()
{
    if ( m_socket.isNull() ) {
        SOCKET_LOG("Socket is already deleted.");
    } else if ( m_socket->state() == QLocalSocket::UnconnectedState ) {
        SOCKET_LOG("Delete after disconnected.");
        deleteLater();
    } else {
        SOCKET_LOG("Will delete after disconnected.");
        m_deleteAfterDisconnected = true;
    }
}

void ClientSocket::close()
{
    if ( !m_socket.isNull() )
        m_socket->disconnectFromServer();
}

bool ClientSocket::isClosed() const
{
    return m_closed;
}

void ClientSocket::onReadyRead()
{
    if ( m_socket.isNull() ) {
        SOCKET_LOG("Cannot read message from client. Socket is already deleted.");
        return;
    }

    while (m_socket->bytesAvailable() > 0) {
        QByteArray msg;

        if ( !readMessage(m_socket, &msg) )
            log( tr("Failed to read message from client!"), LogError );
        else
            emit messageReceived(msg);
    }
}

void ClientSocket::onError(QLocalSocket::LocalSocketError error)
{
    if (error == QLocalSocket::PeerClosedError
        || (error == QLocalSocket::ConnectionError
            && m_socket->errorString().startsWith("QLocalSocketPrivate::completeAsyncRead")) )
    {
        SOCKET_LOG("Disconnected from client.");
    } else {
        log( m_socket->errorString(), LogError );
    }

    onStateChanged(QLocalSocket::UnconnectedState);
}

void ClientSocket::onStateChanged(QLocalSocket::LocalSocketState state)
{
    if (!m_closed) {
        m_closed = state != QLocalSocket::ConnectedState;
        if (m_closed) {
            emit disconnected();
            if (m_deleteAfterDisconnected)
                deleteLater();
        }
    }
}

Arguments ClientSocket::readArguments()
{
    Arguments args;
    QByteArray msg;

    if ( readMessage(m_socket, &msg) ) {
        SOCKET_LOG("Message received from client.");
        QDataStream input(msg);
        input >> args;
    } else {
        log( tr("Failed to read message from client!"), LogError );
    }

    return args;
}
