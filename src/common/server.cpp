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

#include "server.h"

#include "common/arguments.h"
#include "common/clientsocket.h"
#include "common/client_server.h"
#include "common/log.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSharedMemory>
#include <QSystemSemaphore>

namespace {

bool serverIsRunning(const QString &serverName)
{
    QLocalSocket socket;
    socket.connectToServer(serverName);
    return socket.waitForConnected(-1);
}

bool tryAttach(QSharedMemory *shmem)
{
    if (shmem->attach()) {
        shmem->detach();
        return true;
    }

    return false;
}

/**
 * Creates and locks system-wide mutex.
 *
 * If mutex cannot be locked, null pointer is returned.
 *
 * Destroying the object unlocks the mutex.
 *
 * Uses existence shared memory resource to check if the mutex is locked.
 * If shared memory (with same name as mutex is created) exists (another
 * process is attached to it), locking fails. Otherwise shared memory is
 * created (and mutex attaches to it).
 *
 * Note: Simple QSystemSemaphore cannot be used because acquiring
 * the semaphore is blocking operation.
 */
QObject *createSystemMutex(const QString &name, QObject *parent)
{
    QSharedMemory *shmem = new QSharedMemory(name, parent);

    QSystemSemaphore createSharedMemoryGuard("shmem_create_" + name, 1);
    if (createSharedMemoryGuard.acquire()) {
        /* Dummy attach and dettach operations can invoke shared memory
         * destruction if the last process attached to shared memory has
         * crashed and memory was not destroyed.
         */
        if (!tryAttach(shmem) || !tryAttach(shmem)) {
            if (!shmem->create(1)) {
                log("Failed to create shared memory: "
                    + shmem->errorString(), LogError);
            }
        }
        createSharedMemoryGuard.release();
    } else {
        log("Failed to acquire shared memory lock: "
            + createSharedMemoryGuard.errorString(), LogError);
    }

    if (shmem->isAttached())
        return shmem;

    delete shmem;
    return NULL;
}

} // namespace

Server::Server(const QString &name, QObject *parent)
    : QObject(parent)
    , m_server(new QLocalServer(this))
    , m_socketCount(0)
{
    if ( createSystemMutex(name, this) && !serverIsRunning(name) ) {
        QLocalServer::removeServer(name);
        m_server->listen(name);
    }

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
