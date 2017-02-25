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

#ifndef REMOTEPROCESS_H
#define REMOTEPROCESS_H

#include <QObject>
#include <QTimer>

#include <memory>

class ClientSocket;
class Server;
class QByteArray;
class QProcess;
class QString;

using ClientSocketPtr = std::shared_ptr<ClientSocket>;
using ServerPtr = std::unique_ptr<Server>;

/**
 * Starts process and handles communication with it.
 */
class RemoteProcess : public QObject
{
    Q_OBJECT
public:
    explicit RemoteProcess(QObject *parent = nullptr);

    ~RemoteProcess();

    /**
     * Starts server and executes current binary (argv[0]) with given @a arguments.
     */
    void start(const QString &newServerName, const QStringList &arguments);

    /**
     * Send message to remote process.
     */
    void writeMessage(const QByteArray &msg, int messageCode);

    /**
     * Return true only if both server and process are started.
     */
    bool isConnected() const;

signals:
    /**
     * Remote processed sends @a message.
     */
    void newMessage(const QByteArray &message);

    /**
     * Sends message to monitor.
     */
    void sendMessage(const QByteArray &message, int messageCode);

    /**
     * An error occurred with connection.
     */
    void connectionError(const QString &error);

    /**
     * Successfully connected to the remote process.
     */
    void connected();

private slots:
    void ping();
    void pongTimeout();
    void onNewConnection(const ClientSocketPtr &socket);
    void onMessageReceived(const QByteArray &message, int messageCode);
    bool checkConnection();
    void onConnectionError(const QString &error);
    void onDisconnected();

private:
    void terminate();

    ServerPtr m_server;
    QProcess *m_process;
    ClientSocketPtr m_socket;
    QTimer m_timerPing;
    QTimer m_timerPongTimeout;
    int m_pongRetryCount;
    enum State {
        Unconnected,
        Connecting,
        Connected
    } m_state;
};

#endif // REMOTEPROCESS_H
