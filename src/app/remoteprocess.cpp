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

#include "remoteprocess.h"

#include "common/arguments.h"
#include "common/client_server.h"
#include "common/clientsocket.h"
#include "common/common.h"
#include "common/server.h"

#include <QCoreApplication>
#include <QByteArray>
#include <QProcess>
#include <QString>

RemoteProcess::RemoteProcess(QObject *parent)
    : QObject(parent)
    , m_timerPing()
    , m_timerPongTimeout()
    , m_state(Unconnected)
{
    m_timerPing.setInterval(8000);
    m_timerPing.setSingleShot(true);
    connect( &m_timerPing, SIGNAL(timeout()),
             this, SLOT(ping()) );

    m_timerPongTimeout.setInterval(4000);
    m_timerPongTimeout.setSingleShot(true);
    connect( &m_timerPongTimeout, SIGNAL(timeout()),
             this, SLOT(pongTimeout()) );
}

RemoteProcess::~RemoteProcess()
{
    m_timerPing.stop();
    m_timerPongTimeout.stop();
}

void RemoteProcess::start(const QString &newServerName, const QStringList &arguments)
{
    Q_ASSERT(!isConnected());
    if ( isConnected() )
        return;

    m_state = Connecting;

    Server *server = Server::create(newServerName);
    if (!server) {
        onConnectionError();
        return;
    }

    connect(this, SIGNAL(destroyed()),
            server, SLOT(deleteLater()) );
    connect(server, SIGNAL(newConnection(Arguments,ClientSocket*)),
            this, SLOT(onNewConnection(Arguments,ClientSocket*)));

    server->startInThread();

    COPYQ_LOG( QString("Remote process: Starting new remote process \"%1 %2\".")
               .arg(QCoreApplication::applicationFilePath())
               .arg(arguments.join(" ")) );

    qint64 monitorPid;
    if ( !QProcess::startDetached(QCoreApplication::applicationFilePath(), arguments,
                                  QString(), &monitorPid) )
    {
        log( "Remote process: Failed to start new remote process!", LogError );
        onConnectionError();
        return;
    }

    COPYQ_LOG( QString("Remote process: Started with pid %1.").arg(monitorPid) );

    QTimer::singleShot(16000, this, SLOT(checkConnection()));
}

void RemoteProcess::checkConnection() {
    if (!isConnected()) {
        emit connectionError();
    }
}

void RemoteProcess::onNewConnection(const Arguments &, ClientSocket *socket)
{
    COPYQ_LOG("Remote process: Started.");
    m_state = Connected;

    connect( this, SIGNAL(destroyed()),
             socket, SLOT(deleteAfterDisconnected()) );
    connect( this, SIGNAL(sendMessage(QByteArray)),
             socket, SLOT(sendMessage(QByteArray)) );
    connect( socket, SIGNAL(messageReceived(QByteArray)),
             this, SIGNAL(newMessage(QByteArray)) );
    connect( socket, SIGNAL(disconnected()),
             this, SLOT(onConnectionError()) );

    socket->start();

    ping();

    emit connected();
}

void RemoteProcess::onConnectionError()
{
    m_state = Unconnected;
    emit connectionError();
}

void RemoteProcess::writeMessage(const QByteArray &msg)
{
    emit sendMessage(msg);
}

bool RemoteProcess::isConnected() const
{
    return m_state == Connected;
}

void RemoteProcess::ping()
{
    if ( isConnected() ) {
        writeMessage("ping");
        m_timerPing.stop();
        m_timerPongTimeout.start();
    }
}

void RemoteProcess::pongTimeout()
{
    log( "Remote process: Connection timeout!", LogError );
    onConnectionError();
}
