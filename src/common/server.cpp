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
#include <QLockFile>

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
    dir.mkpath(QStringLiteral("."));

    return lockFilePath;
}

} // namespace

struct Server::PrivateData {
    QLocalServer server;
    QLockFile lockFile = lockFilePath();
    int socketCount = 0;
    QEventLoop *loop = nullptr;
};

Server::Server(const QString &name, QObject *parent)
    : QObject(parent)
    , m_data(new PrivateData)
{
    if ( m_data->lockFile.tryLock() && !serverIsRunning(name) ) {
        QLocalServer::removeServer(name);
        if ( !m_data->server.listen(name) ) {
            log( QStringLiteral("Failed to create server \"%1\": %2")
                 .arg(m_data->server.fullServerName(), m_data->server.errorString()),
                 LogError);
        }
    } else if (m_data->lockFile.error() == QLockFile::LockFailedError) {
        COPYQ_LOG( QStringLiteral("Another process holds lock file: %1")
                   .arg(lockFilePath()) );
    } else if (m_data->lockFile.error() == QLockFile::PermissionError) {
        log( QStringLiteral("Insufficient permissions to create lock file: %1")
             .arg(lockFilePath()), LogError );
    } else if (m_data->lockFile.error() == QLockFile::UnknownError) {
        log( QStringLiteral("Failed to lock file: %1")
             .arg(lockFilePath()), LogError );
    }
}

Server::~Server()
{
    // Postpone destroying server, otherwise it crashes when re-creating server.
    m_data->server.deleteLater();
}

void Server::start()
{
    connect( &m_data->server, &QLocalServer::newConnection,
             this, &Server::onNewConnection );

    while (m_data->server.hasPendingConnections())
        onNewConnection();
}

bool Server::isListening() const
{
    return m_data->server.isListening();
}

void Server::close()
{
    m_data->server.close();

    if (m_data->socketCount > 0) {
        COPYQ_LOG( QStringLiteral("Waiting for %1 sockets to disconnect").arg(m_data->socketCount) );

        QEventLoop loop;
        m_data->loop = &loop;
        loop.exec(QEventLoop::ExcludeUserInputEvents);
        m_data->loop = nullptr;
    }

    m_data->lockFile.unlock();
}

void Server::onNewConnection()
{
    QLocalSocket* socket = m_data->server.nextPendingConnection();
    if (!socket) {
        log("No pending client connections!", LogError);
    } else if ( socket->state() != QLocalSocket::ConnectedState ) {
        log("Client is not connected!", LogError);
        socket->deleteLater();
    } else {
        ++m_data->socketCount;
        connect( socket, &QLocalSocket::disconnected,
                 this, &Server::onSocketDestroyed );

        auto clientSocket = std::make_shared<ClientSocket>(socket);
        emit newConnection(clientSocket);
    }
}

void Server::onSocketDestroyed()
{
    Q_ASSERT(m_data->socketCount > 0);
    --m_data->socketCount;
    if (m_data->loop)
        m_data->loop->quit();
}
