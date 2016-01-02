/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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
#include "common/log.h"

#include <QDataStream>

#define SOCKET_LOG(text) \
    COPYQ_LOG_VERBOSE( QString("Socket %1: %2").arg(property("id").toInt()).arg(text) )

namespace {

bool readBytes(QLocalSocket *socket, qint64 size, QByteArray *bytes)
{
    qint64 avail, read = 0;
    bytes->clear();
    while (read < size) {
        if ( socket->bytesAvailable() == 0 && !socket->waitForReadyRead(4000) )
            return false;
        avail = qMin( socket->bytesAvailable(), size-read );
        bytes->append( socket->read(avail) );
        read += avail;
    }

    return true;
}

bool readMessage(QLocalSocket *socket, QByteArray *msg)
{
    QByteArray bytes;
    quint32 len;

    COPYQ_LOG_VERBOSE("Reading message.");

    if ( readBytes(socket, sizeof(len), &bytes) ) {
        QDataStream(bytes) >> len;

        if ( readBytes(socket, len, msg) ) {
            COPYQ_LOG_VERBOSE( QString("Message read (%1 bytes).").arg(msg->size()) );
            return true;
        }
    }

    COPYQ_LOG("ERROR: Incorrect message!");

    return false;
}

bool writeMessage(QLocalSocket *socket, const QByteArray &msg)
{
    COPYQ_LOG_VERBOSE( QString("Write message (%1 bytes).").arg(msg.size()) );

    QDataStream out(socket);
    // length is serialized as a quint32, followed by msg
    out.writeBytes( msg.constData(), msg.length() );

    if (out.status() != QDataStream::Ok) {
        COPYQ_LOG("Cannot write message!");
        return false;
    }

    COPYQ_LOG_VERBOSE("Message written.");
    return true;
}

} //namespace

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

    if ( hasLogLevel(LogDebug) ) {
        setProperty("id", m_socket->socketDescriptor());
        SOCKET_LOG("Creating socket.");
    }
}

ClientSocket::~ClientSocket()
{
    SOCKET_LOG("Destroying socket.");
}

void ClientSocket::start()
{
    QMetaObject::invokeMethod(this, "onReadyRead", Qt::QueuedConnection);
}

void ClientSocket::sendMessage(const QByteArray &message, int messageCode)
{
    SOCKET_LOG( QString("Sending message to client (exit code: %1).").arg(messageCode) );

    if ( m_socket.isNull() ) {
        SOCKET_LOG("Cannot send message to client. Socket is already deleted.");
    } else if (m_closed) {
        SOCKET_LOG("Client disconnected!");
    } else {
        QByteArray msg;
        QDataStream out(&msg, QIODevice::WriteOnly);
        out << static_cast<qint32>(messageCode);
        out.writeRawData( message.constData(), message.length() );
        if ( writeMessage(m_socket, msg) )
            SOCKET_LOG("Message sent to client.");
        else
            SOCKET_LOG("Failed to send message to client!");
    }
}

void ClientSocket::deleteAfterDisconnected()
{
    if ( m_socket.isNull() ) {
        SOCKET_LOG("Socket is already deleted.");
    } else if (m_closed) {
        SOCKET_LOG("Delete after disconnected.");
        deleteLater();
    } else {
        SOCKET_LOG("Will delete after disconnected.");
        m_deleteAfterDisconnected = true;
    }
}

void ClientSocket::close()
{
    if ( !m_socket.isNull() ) {
        SOCKET_LOG("Disconnecting socket.");
        m_socket->disconnectFromServer();
    }
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

    /* QLocalSocket::waitForReadyRead() seems to re-emit readyRead()
     * though QIODevice::waitForReadyRead() doesn't do that (very confusing).
     */
    disconnect( m_socket, SIGNAL(readyRead()),
                this, SLOT(onReadyRead()) );

    while (m_socket->bytesAvailable() > 0) {
        QByteArray msg;

        if ( !readMessage(m_socket, &msg) ) {
            log( tr("Failed to read message from client!"), LogError );
            m_socket->abort();
            onStateChanged(QLocalSocket::UnconnectedState);
            return;
        }

        QDataStream stream(msg);
        qint32 messageCode;
        stream >> messageCode;
        const int i = sizeof(messageCode);
        const QByteArray data( msg.constData() + i, msg.length() - i );
        emit messageReceived(data, messageCode);
    }

    connect( m_socket, SIGNAL(readyRead()),
             this, SLOT(onReadyRead()) );
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
    QByteArray msg;

    if ( readMessage(m_socket, &msg) ) {
        SOCKET_LOG("Message received from client.");
        QDataStream input(msg);
        qint32 exitCode;
        Arguments args;
        input >> exitCode >> args;
        if ( input.status() == QDataStream::Ok && exitCode == 0 && !args.isEmpty() )
            return args;
    }

    log( tr("Failed to read message from client!"), LogError );

    return Arguments();
}
