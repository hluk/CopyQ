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
        connect(m_socket, SIGNAL(disconnected()), m_socket, SLOT(deleteLater()));
    }
}

void ClientSocket::onReadyRead()
{
    QByteArray msg;
    if ( m_socket.isNull() ) {
        SOCKET_LOG("Cannot read message from client. Socket is already deleted.");
    } else if ( !readMessage(m_socket, &msg) ) {
        log( tr("Failed to read message from client!"), LogError );
        return;
    }

    emit messageReceived(msg);
}

void ClientSocket::onError(QLocalSocket::LocalSocketError error)
{
    if (error == QLocalSocket::PeerClosedError)
        SOCKET_LOG("Disconnected from client.");
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
    SOCKET_LOG("New client connection");
#endif
}

Arguments ClientSocket::readArguments()
{
    Arguments args;
    QByteArray msg;

    if ( readMessage(m_socket, &msg) ) {
        SOCKET_LOG("Message received from client.");
        QDataStream input(msg);
        input >> args;
        connect( m_socket, SIGNAL(readyRead()),
                 this, SLOT(onReadyRead()) );
    } else {
        log( tr("Failed to read message from client!"), LogError );
    }

    return args;
}
