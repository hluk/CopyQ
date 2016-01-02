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

#ifndef CLIENTSOCKET_H
#define CLIENTSOCKET_H

#include <QLocalSocket>
#include <QObject>
#include <QPointer>

class Arguments;

class ClientSocket : public QObject
{
    Q_OBJECT
    friend class Server;
public:
    ClientSocket();

    explicit ClientSocket(QLocalSocket *socket, QObject *parent = NULL);

    ~ClientSocket();

    /// Start emiting messageReceived(). This method is thread-safe.
    void start();

public slots:
    /** Send message to client. */
    void sendMessage(
            const QByteArray &message, //!< Message for client.
            int messageCode //!< Custom message code.
            );
    void deleteAfterDisconnected();

    void close();

    bool isClosed() const;

signals:
    void messageReceived(const QByteArray &message, int messageCode);
    void disconnected();

private slots:
    void onReadyRead();
    void onError(QLocalSocket::LocalSocketError error);
    void onStateChanged(QLocalSocket::LocalSocketState state);

private:
    /// Receive arguments from client.
    Arguments readArguments();

    QPointer<QLocalSocket> m_socket;
    bool m_deleteAfterDisconnected;
    bool m_closed;
};

#endif // CLIENTSOCKET_H
