// SPDX-License-Identifier: GPL-3.0-or-later

#include "server.h"

#include "common/common.h"
#include "common/config.h"
#include "common/clientsocket.h"
#include "common/client_server.h"
#include "common/log.h"

#include <QDir>
#include <QEventLoop>
#include <QLocalServer>
#include <QLocalSocket>

namespace {

bool serverIsRunning(const QString &serverName)
{
    QLocalSocket socket;
    socket.connectToServer(serverName);
    return socket.waitForConnected(-1);
}

QString lockFilePath()
{
    const QString lockFilePath = getConfigurationFilePath(".lock");

    // Ensure parent dir exists.
    const QString path = QDir::cleanPath( lockFilePath + QLatin1String("/..") );
    QDir dir(path);
    dir.mkpath(".");

    return lockFilePath;
}

} // namespace

Server::Server(const QString &name, QObject *parent)
    : QObject(parent)
    , m_server(new QLocalServer)
    , m_lockFile(lockFilePath())
    , m_socketCount(0)
{
    if ( m_lockFile.tryLock() && !serverIsRunning(name) ) {
        QLocalServer::removeServer(name);
        if ( !m_server->listen(name) ) {
            log( QString("Failed to create server \"%1\": %2")
                 .arg(m_server->fullServerName(), m_server->errorString()),
                 LogError);
        }
    } else if (m_lockFile.error() == QLockFile::LockFailedError) {
        COPYQ_LOG( QStringLiteral("Another process holds lock file: %1")
                   .arg(lockFilePath()) );
    } else if (m_lockFile.error() == QLockFile::PermissionError) {
        log( QStringLiteral("Insufficient permissions to create lock file: %1")
             .arg(lockFilePath()), LogError );
    } else if (m_lockFile.error() == QLockFile::UnknownError) {
        log( QStringLiteral("Failed to lock file: %1")
             .arg(lockFilePath()), LogError );
    }
}

Server::~Server()
{
    // Postpone destroying server, otherwise it crashes when re-creating server.
    m_server->deleteLater();
}

void Server::start()
{
    connect( m_server, &QLocalServer::newConnection,
             this, &Server::onNewConnection );

    while (m_server->hasPendingConnections())
        onNewConnection();
}

bool Server::isListening() const
{
    return m_server->isListening();
}

void Server::close()
{
    m_server->close();

    if (m_socketCount > 0) {
        COPYQ_LOG( QString("Waiting for %1 sockets to disconnect").arg(m_socketCount) );

        QEventLoop loop;
        m_loop = &loop;
        loop.exec(QEventLoop::ExcludeUserInputEvents);
        m_loop = nullptr;
    }

    m_lockFile.unlock();
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
        ++m_socketCount;
        connect( socket, &QLocalSocket::disconnected,
                 this, &Server::onSocketDestroyed );

        auto clientSocket = std::make_shared<ClientSocket>(socket);
        emit newConnection(clientSocket);
    }
}

void Server::onSocketDestroyed()
{
    Q_ASSERT(m_socketCount > 0);
    --m_socketCount;
    if (m_loop)
        m_loop->quit();
}
