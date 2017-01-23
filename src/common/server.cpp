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

#include "server.h"

#include "common/clientsocket.h"
#include "common/client_server.h"
#include "common/log.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QLocalServer>
#include <QLocalSocket>

namespace {

bool serverIsRunning(const QString &serverName)
{
    QLocalSocket socket;
    socket.connectToServer(serverName);
    return socket.waitForConnected(-1);
}

} // namespace

Server::Server(const QString &name, QObject *parent)
    : QObject(parent)
    , m_server(new QLocalServer(this))
    , m_socketCount(0)
    , m_mutex(getPlatformSystemMutex(name))
{
    if ( m_mutex->tryLock() && !serverIsRunning(name) ) {
        QLocalServer::removeServer(name);
        if ( !m_server->listen(name) )
            log("Failed to create server: " + m_server->errorString(), LogError);
    }

    const QString mutexError = m_mutex->error();
    if ( !mutexError.isEmpty() )
        log("Failed to lock system mutex: " + mutexError, LogError);

    connect( qApp, SIGNAL(aboutToQuit()), SLOT(close()) );
}

void Server::start()
{
    while (m_server->hasPendingConnections())
        onNewConnection();

    connect( m_server, SIGNAL(newConnection()),
             this, SLOT(onNewConnection()) );
}

bool Server::isListening() const
{
    return m_server->isListening();
}

void Server::onNewConnection()
{
    QLocalSocket* socket = m_server->nextPendingConnection();
    if (!socket) {
        log("No pending client connections!", LogError);
    } else if ( socket->state() != QLocalSocket::ConnectedState ) {
        log("Client is not connected!", LogError);
        socket->deleteLater();
    } else {
        ClientSocket *clientSocket = new ClientSocket(socket);

        ++m_socketCount;
        connect( clientSocket, SIGNAL(destroyed()),
                 this, SLOT(onSocketClosed()) );
        connect( this, SIGNAL(destroyed()),
                 clientSocket, SLOT(close()) );
        connect( this, SIGNAL(destroyed()),
                 clientSocket, SLOT(deleteAfterDisconnected()) );
        connect( clientSocket, SIGNAL(disconnected()),
                 clientSocket, SLOT(deleteAfterDisconnected()) );

        emit newConnection(clientSocket);
    }
}

void Server::onSocketClosed()
{
    Q_ASSERT(m_socketCount > 0);
    --m_socketCount;
}

void Server::close()
{
    m_server->close();

    COPYQ_LOG( QString("Sockets open: %1").arg(m_socketCount) );
    while (m_socketCount > 0)
        QCoreApplication::processEvents();

    deleteLater();
}
