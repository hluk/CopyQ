/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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

#include "common/client_server.h"
#include "common/log.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QDataStream>
#include <QPointer>

#define SOCKET_LOG(text) \
    COPYQ_LOG_VERBOSE( QString("Socket %1: %2").arg(property("id").toInt()).arg(text) )

namespace {

template <typename T>
int doStreamDataSize(T value)
{
    QByteArray bytes;
    {
        QDataStream dataStream(&bytes, QIODevice::WriteOnly);
        dataStream << value;
    }
    return bytes.length();
}

template <typename T>
int streamDataSize(T value)
{
    static int size = doStreamDataSize(value);
    return size;
}

template <typename T>
bool readValue(T *value, QByteArray *message)
{
    QDataStream stream(*message);
    stream >> *value;
    message->remove(0, streamDataSize(*value));
    return stream.status() == QDataStream::Ok;
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
    , m_socket(nullptr)
    , m_deleteAfterDisconnected(false)
    , m_closed(true)
    , m_hasMessageLength(false)
{
}

ClientSocket::ClientSocket(const QString &serverName, QObject *parent)
    : QObject(parent)
    , m_socket(new QLocalSocket(this))
    , m_deleteAfterDisconnected(false)
    , m_closed(false)
    , m_hasMessageLength(false)
{
    m_socket->connectToServer(serverName);
}

ClientSocket::ClientSocket(QLocalSocket *socket, QObject *parent)
    : QObject(parent)
    , m_socket(socket)
    , m_deleteAfterDisconnected(false)
    , m_closed(false)
    , m_hasMessageLength(false)
{
    socket->setParent(this);
}

ClientSocket::~ClientSocket()
{
    SOCKET_LOG("Destroying socket.");
    close();
}

void ClientSocket::start()
{
    if ( !m_socket || !m_socket->waitForConnected(4000) )
    {
        emit connectionFailed();
        return;
    }

    if ( hasLogLevel(LogDebug) ) {
        setProperty("id", m_socket->socketDescriptor());
        SOCKET_LOG("Creating socket.");
    }

    connect( m_socket, SIGNAL(stateChanged(QLocalSocket::LocalSocketState)),
             this, SLOT(onStateChanged(QLocalSocket::LocalSocketState)), Qt::UniqueConnection );
    connect( m_socket, SIGNAL(error(QLocalSocket::LocalSocketError)),
             this, SLOT(onError(QLocalSocket::LocalSocketError)), Qt::UniqueConnection );
    connect( m_socket, SIGNAL(readyRead()),
             this, SLOT(onReadyRead()), Qt::UniqueConnection );

    onStateChanged(m_socket->state());

    onReadyRead();
}

void ClientSocket::sendMessage(const QByteArray &message, int messageCode)
{
    SOCKET_LOG( QString("Sending message to client (exit code: %1).").arg(messageCode) );

    if (!m_socket) {
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
    if (!m_socket) {
        SOCKET_LOG("Socket is already deleted.");
        deleteLater();
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
    if (m_socket) {
        SOCKET_LOG("Disconnecting socket.");
        m_socket->disconnectFromServer();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
}

bool ClientSocket::isClosed() const
{
    return m_closed;
}

void ClientSocket::onReadyRead()
{
    if (!m_socket) {
        SOCKET_LOG("Cannot read message from client. Socket is already deleted.");
        return;
    }

    const qint64 available = m_socket->bytesAvailable();
    m_message.append( m_socket->read(available) );

    while ( !m_message.isEmpty() ) {
        if (!m_hasMessageLength) {
            if ( m_message.length() < streamDataSize(m_messageLength) )
                break;

            if ( !readValue(&m_messageLength, &m_message) ) {
                error("Failed to read message length from client!");
                return;
            }
            m_hasMessageLength = true;
        }

        if ( static_cast<quint32>(m_message.length()) < m_messageLength )
            break;

        QByteArray msg = m_message.mid(0, m_messageLength);
        qint32 messageCode;
        if ( !readValue(&messageCode, &msg) ) {
            error("Failed to read message code from client!");
            return;
        }

        m_hasMessageLength = false;
        m_message = m_message.mid(m_messageLength);

        emit messageReceived(msg, messageCode);
    }
}

void ClientSocket::onError(QLocalSocket::LocalSocketError error)
{
    if (error == QLocalSocket::SocketTimeoutError)
        return;

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
            if (m_hasMessageLength)
                log("ERROR: Socket disconnected before receiving message", LogError);

            emit disconnected();
            if (m_deleteAfterDisconnected)
                deleteLater();
        }
    }
}

void ClientSocket::error(const QString &errorMessage)
{
    log(errorMessage, LogError);
    if (m_socket)
        m_socket->abort();
    onStateChanged(QLocalSocket::UnconnectedState);
}
