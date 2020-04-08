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

#ifndef CLIENTSOCKET_H
#define CLIENTSOCKET_H

#include <QLocalSocket>
#include <QObject>
#include <QPointer>

using ClientSocketId = qulonglong;

class LocalSocketGuard final
{
public:
    explicit LocalSocketGuard(QLocalSocket *socket);
    ~LocalSocketGuard();

    QLocalSocket *get() const { return m_socket; }
    QLocalSocket *operator->() const { return m_socket; }
    operator QLocalSocket*() { return m_socket; }
    operator bool() { return !m_socket.isNull(); }

    LocalSocketGuard(const LocalSocketGuard &) = delete;
    LocalSocketGuard &operator=(const LocalSocketGuard &) = delete;

private:
    QPointer<QLocalSocket> m_socket;
};

class ClientSocket final : public QObject
{
    Q_OBJECT
public:
    ClientSocket();

    explicit ClientSocket(const QString &serverName, QObject *parent = nullptr);

    explicit ClientSocket(QLocalSocket *socket, QObject *parent = nullptr);

    ~ClientSocket();

    /// Return socket ID unique in process (thread-safe).
    ClientSocketId id() const { return m_socketId; }

    /// Start emitting messageReceived().
    bool start();

    /** Send message to client. */
    void sendMessage(
            const QByteArray &message, //!< Message for client.
            int messageCode //!< Custom message code.
            );

    void close();

signals:
    void messageReceived(const QByteArray &message, int messageCode, ClientSocketId clientId);
    void disconnected(ClientSocketId clientId);
    void connectionFailed(ClientSocketId clientId);

private:
    void onReadyRead();
    void onError(QLocalSocket::LocalSocketError error);
    void onStateChanged(QLocalSocket::LocalSocketState state);

    void error(const QString &errorMessage);

    LocalSocketGuard m_socket;
    ClientSocketId m_socketId;
    bool m_closed;

    bool m_hasMessageLength = false;
    quint32 m_messageLength = 0;
    QByteArray m_message;
};

#endif // CLIENTSOCKET_H
