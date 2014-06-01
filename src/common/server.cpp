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

#include "server.h"

#include "common/arguments.h"
#include "common/clientsocket.h"
#include "common/client_server.h"
#include "common/log.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QLocalServer>
#include <QLocalSocket>

namespace {

QLocalServer *newServer(const QString &name, QObject *parent)
{
    COPYQ_LOG( QString("Starting server \"%1\".").arg(name) );

    QLocalServer *server = new QLocalServer(parent);

    // check if other server is running
    QLocalSocket socket;
    socket.connectToServer(name);
    if ( socket.waitForConnected(2000) ) {
        // server is running
        QDataStream out(&socket);
        out << (quint32)0;
        COPYQ_LOG( QString("Server \"%1\" already running!").arg(name) );
    } else {
        QLocalServer::removeServer(name);
        server->listen(name);
        COPYQ_LOG( QString("Server \"%1\" started.").arg(name) );
    }

    return server;
}

} // namespace

Server::Server(const QString &name, QObject *parent)
    : QObject(parent)
    , m_server(newServer(name, this))
    , m_socketCount(0)
{
    qRegisterMetaType<Arguments>("Arguments");
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
        QScopedPointer<ClientSocket> clientSocket( new ClientSocket(socket) );

        const Arguments args = clientSocket->readArguments();
        if ( !args.isEmpty() ) {
            ++m_socketCount;
            connect( clientSocket.data(), SIGNAL(destroyed()),
                     this, SLOT(onSocketClosed()) );
            connect( this, SIGNAL(destroyed()),
                     clientSocket.data(), SLOT(close()) );
            connect( this, SIGNAL(destroyed()),
                     clientSocket.data(), SLOT(deleteAfterDisconnected()) );
            emit newConnection( args, clientSocket.take() );
        }
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
