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
#include "common/common.h"

#include <QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QThread>

namespace {

QLocalServer *newServer(const QString &name)
{
    COPYQ_LOG( QString("Starting server \"%1\".").arg(name) );

    QLocalServer *server = new QLocalServer;

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

Server *Server::create(const QString &name)
{
    QLocalServer *localServer = newServer(name);
    if ( !localServer->isListening() ) {
        delete localServer;
        return NULL;
    }

    return new Server(localServer);
}

void Server::startInThread()
{
    Q_ASSERT(thread() == QThread::currentThread());

    QThread *t = new QThread;
    connect(this, SIGNAL(destroyed()), t, SLOT(quit()));
    connect(t, SIGNAL(finished()), t, SLOT(deleteLater()));
    connect(t, SIGNAL(started()), this, SLOT(start()));

    moveToThread(t);
    t->start();
}

void Server::onNewConnection()
{
    QLocalSocket* socket = m_server->nextPendingConnection();
    if (!socket) {
        log( tr("No pending client connections!"), LogError );
    } else if ( socket->state() != QLocalSocket::ConnectedState ) {
        log( tr("Client is not connected!"), LogError );
        socket->deleteLater();
    } else {
        QScopedPointer<ClientSocket> clientSocket( new ClientSocket(socket, this) );

        const Arguments args = clientSocket->readArguments();
        if ( !args.isEmpty() )
            emit newConnection( args, clientSocket.take() );
    }
}

void Server::start()
{
    while (m_server->hasPendingConnections())
        onNewConnection();

    connect( m_server, SIGNAL(newConnection()),
             this, SLOT(onNewConnection()) );
}

void Server::close()
{
    m_server->close();

    QList<QLocalSocket*> sockets = findChildren<QLocalSocket*>();
    COPYQ_LOG( QString("Sockets open: %1").arg(sockets.size()) );
    foreach (QLocalSocket *socket, sockets)
        socket->waitForDisconnected(4000);

    deleteLater();
}

Server::Server(QLocalServer *server)
    : QObject()
    , m_server(server)
{
    m_server->setParent(this);
    qRegisterMetaType<Arguments>("Arguments");
    connect( qApp, SIGNAL(aboutToQuit()), SLOT(close()) );
}
