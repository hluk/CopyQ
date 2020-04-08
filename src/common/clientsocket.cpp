/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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
#include "common/sleeptimer.h"

#include <QDataStream>

#define SOCKET_LOG(text) \
    COPYQ_LOG_VERBOSE( QString("Socket %1: %2").arg(m_socketId).arg(text) )

namespace {

const int bigMessageThreshold = 5 * 1024 * 1024;
ClientSocketId lastSocketId = 0;

const quint32 protocolMagicNumber = 0x0C090701;
const quint32 protocolVersion = 1;

template <typename T>
int doStreamDataSize(T value)
{
    QByteArray bytes;
    {
        QDataStream dataStream(&bytes, QIODevice::WriteOnly);
        dataStream.setVersion(QDataStream::Qt_5_0);
        dataStream << value;
    }
    return bytes.length();
}

template <typename T>
int streamDataSize(T value)
{
    static int size = doStreamDataSize<T>(value);
    return size;
}

int headerDataSize()
{
    QByteArray bytes;
    {
        QDataStream dataStream(&bytes, QIODevice::WriteOnly);
        dataStream.setVersion(QDataStream::Qt_5_0);
        dataStream << protocolMagicNumber << protocolVersion;
    }
    return bytes.length();
}

template <typename T>
bool readValue(T *value, QByteArray *message)
{
    QDataStream stream(*message);
    stream.setVersion(QDataStream::Qt_5_0);
    stream >> *value;
    message->remove(0, streamDataSize(*value));
    return stream.status() == QDataStream::Ok;
}

bool writeMessage(QLocalSocket *socket, const QByteArray &msg)
{
    COPYQ_LOG_VERBOSE( QString("Write message (%1 bytes).").arg(msg.size()) );

    if (msg.size() > bigMessageThreshold)
        COPYQ_LOG( QString("Sending big message: %1 MiB").arg(msg.size() / 1024 / 1024) );

    QDataStream out(socket);
    out.setVersion(QDataStream::Qt_5_0);
    // length is serialized as a quint32, followed by msg
    const auto length = static_cast<quint32>(msg.length());
    out << protocolMagicNumber << protocolVersion;
    out.writeBytes( msg.constData(), length );

    if (out.status() != QDataStream::Ok) {
        COPYQ_LOG("Cannot write message!");
        return false;
    }

    COPYQ_LOG_VERBOSE("Message written.");
    return true;
}

} //namespace

LocalSocketGuard::LocalSocketGuard(QLocalSocket *socket)
    : m_socket(socket)
{
}

LocalSocketGuard::~LocalSocketGuard()
{
    if (m_socket) {
        if (m_socket->state() == QLocalSocket::UnconnectedState)
            m_socket->deleteLater();
        else
            m_socket->disconnectFromServer();
    }
}

ClientSocket::ClientSocket()
    : QObject()
    , m_socket(nullptr)
    , m_socketId(++lastSocketId)
    , m_closed(true)
{
}

ClientSocket::ClientSocket(const QString &serverName, QObject *parent)
    : QObject(parent)
    , m_socket(new QLocalSocket)
    , m_socketId(++lastSocketId)
    , m_closed(false)
{
    m_socket->connectToServer(serverName);

    // Try to connect again in case the server just started.
    if ( m_socket->state() == QLocalSocket::UnconnectedState ) {
        COPYQ_LOG("Waiting for server to start");
        SleepTimer t(1000);
        do {
            m_socket->connectToServer(serverName);
        } while ( m_socket->state() == QLocalSocket::UnconnectedState && t.sleep() );
    }
}

ClientSocket::ClientSocket(QLocalSocket *socket, QObject *parent)
    : QObject(parent)
    , m_socket(socket)
    , m_socketId(++lastSocketId)
    , m_closed(false)
{
}

ClientSocket::~ClientSocket()
{
    SOCKET_LOG("Destroying socket.");
    close();
}

bool ClientSocket::start()
{
    if ( !m_socket || !m_socket->waitForConnected(4000) )
    {
        emit connectionFailed(id());
        return false;
    }

    SOCKET_LOG("Creating socket.");

    connect( m_socket.get(), &QLocalSocket::stateChanged,
             this, &ClientSocket::onStateChanged );
    connect( m_socket.get(), static_cast<void (QLocalSocket::*)(QLocalSocket::LocalSocketError)>(&QLocalSocket::error),
             this, &ClientSocket::onError );
    connect( m_socket.get(), &QLocalSocket::readyRead,
             this, &ClientSocket::onReadyRead );

    onStateChanged(m_socket->state());

    onReadyRead();

    return true;
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
        out.setVersion(QDataStream::Qt_5_0);
        out << static_cast<qint32>(messageCode);
        out.writeRawData( message.constData(), message.length() );
        if ( writeMessage(m_socket, msg) )
            SOCKET_LOG("Message sent to client.");
        else
            SOCKET_LOG("Failed to send message to client!");
    }
}

void ClientSocket::close()
{
    if (m_socket) {
        SOCKET_LOG("Disconnecting socket.");
        m_socket->disconnectFromServer();
    }
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
            const int preambleSize = headerDataSize() + streamDataSize(m_messageLength);
            if ( m_message.length() < preambleSize )
                break;

            {
                QDataStream stream(m_message);
                stream.setVersion(QDataStream::Qt_5_0);
                quint32 magicNumber;
                quint32 version;
                stream >> magicNumber >> version >> m_messageLength;
                if ( stream.status() != QDataStream::Ok ) {
                    error("Failed to read message length from client!");
                    return;
                }

                if (magicNumber != protocolMagicNumber) {
                    error("Unexpected message magic number from client!");
                    return;
                }

                if (version != protocolVersion) {
                    error("Unexpected message version from client!");
                    return;
                }
            }

            m_message.remove(0, preambleSize);
            m_hasMessageLength = true;

            if (m_messageLength > bigMessageThreshold)
                COPYQ_LOG( QString("Receiving big message: %1 MiB").arg(m_messageLength / 1024 / 1024) );
        }

        const auto length = static_cast<int>(m_messageLength);
        if ( m_message.length() < length )
            break;

        QByteArray msg = m_message.mid(0, length);
        qint32 messageCode;

        if ( !readValue(&messageCode, &msg) ) {
            error("Failed to read message code from client!");
            return;
        }

        m_hasMessageLength = false;
        m_message = m_message.mid(length);

        emit messageReceived(msg, messageCode, id());
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
        m_closed = state == QLocalSocket::UnconnectedState;
        if (m_closed) {
            if (m_hasMessageLength)
                log("ERROR: Socket disconnected before receiving message", LogError);

            emit disconnected(id());
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
