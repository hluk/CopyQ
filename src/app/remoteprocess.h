/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

class QByteArray;
class QLocalServer;
class QLocalSocket;
class QString;

/**
 * Starts process and handles communication with it.
 *
 * Parent->child hierarchy after start() is called is following:
 *
 *   Parent of RemoteProcess -> QProcess -> QLocalServer -> QLocalSocket
 *
 */
class RemoteProcess : public QObject
{
    Q_OBJECT
public:
    explicit RemoteProcess(QObject *parent = NULL);

    ~RemoteProcess();

    /**
     * Starts server and executes current binary (argv[0]) with given @a arguments.
     */
    void start(const QString &newServerName, const QStringList &arguments);

    /**
     * Send message to remote process.
     */
    bool writeMessage(const QByteArray &msg);

    /**
     * Return true only if both server and process are started.
     */
    bool isConnected() const;

    void closeConnection();

signals:
    /**
     * Remote processed sends @a message.
     */
    void newMessage(const QByteArray &message);

    /**
     * An error occurred with connection.
     */
    void connectionError();

    /**
     * Successfully connected to the remote process.
     */
    void connected();

private slots:
    void readyRead();
    void ping();
    void pongTimeout();
    void onNewConnection();

private:
    QLocalServer *m_server;
    QLocalSocket *m_socket;
    QTimer m_timerPing;
    QTimer m_timerPongTimeout;
};

#endif // REMOTEPROCESS_H
