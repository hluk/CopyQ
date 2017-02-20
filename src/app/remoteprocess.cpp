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

#include "remoteprocess.h"

#include "common/common.h"
#include "common/client_server.h"
#include "common/clientsocket.h"
#include "common/monitormessagecode.h"
#include "common/log.h"
#include "common/server.h"

#include <QCoreApplication>
#include <QByteArray>
#include <QProcess>
#include <QString>

namespace {

const int pingMaxRetries = 4;

} // namespace

RemoteProcess::RemoteProcess(QObject *parent)
    : QObject(parent)
    , m_process(nullptr)
    , m_pongRetryCount(0)
    , m_state(Unconnected)
{
    initSingleShotTimer( &m_timerPing, 8000, this, SLOT(ping()) );
    initSingleShotTimer( &m_timerPongTimeout, 4000, this, SLOT(pongTimeout()) );
}

RemoteProcess::~RemoteProcess()
{
    terminate();
    m_timerPing.stop();
    m_timerPongTimeout.stop();
}

void RemoteProcess::start(const QString &newServerName, const QStringList &arguments)
{
    Q_ASSERT(!isConnected());
    if ( isConnected() )
        return;

    m_state = Connecting;

    Server *server = new Server(newServerName, this);
    if ( !server->isListening() ) {
        delete server;
        onConnectionError("Failed to start local server \"" + newServerName + "\"!");
        return;
    }

    connect(server, SIGNAL(newConnection(ClientSocket*)),
            this, SLOT(onNewConnection(ClientSocket*)));
    connect(this, SIGNAL(connectionError(QString)),
            server, SLOT(deleteLater()));

    server->start();

    COPYQ_LOG( QString("Remote process: Starting new remote process \"%1 %2\".")
               .arg(QCoreApplication::applicationFilePath())
               .arg(arguments.join(" ")) );

    terminate();
    m_process = new QProcess(this);
    m_process->start(QCoreApplication::applicationFilePath(), arguments, QIODevice::NotOpen);
    m_process->closeReadChannel(QProcess::StandardOutput);
    m_process->closeReadChannel(QProcess::StandardError);
    m_process->closeWriteChannel();

    QTimer::singleShot(16000, this, SLOT(checkConnection()));
}

bool RemoteProcess::checkConnection() {
    if (!isConnected()) {
        onConnectionError("Failed to start");
        return false;
    }

    return true;
}

void RemoteProcess::onNewConnection(ClientSocket *socket)
{
    COPYQ_LOG("Remote process: Started.");

    connect( this, SIGNAL(destroyed()),
             socket, SLOT(deleteAfterDisconnected()) );
    connect( this, SIGNAL(sendMessage(QByteArray,int)),
             socket, SLOT(sendMessage(QByteArray,int)) );
    connect( socket, SIGNAL(messageReceived(QByteArray,int)),
             this, SLOT(onMessageReceived(QByteArray,int)) );
    connect( socket, SIGNAL(disconnected()),
             this, SLOT(onDisconnected()) );

    if ( socket->isClosed() ) {
        onConnectionError("Connection lost");
        socket->deleteLater();
    } else {
        m_state = Connected;

        socket->start();

        ping();

        emit connected();
    }
}

void RemoteProcess::onMessageReceived(const QByteArray &message, int messageCode)
{
    // restart waiting on ping response
    if (m_timerPongTimeout.isActive())
        m_timerPongTimeout.start();

    if (messageCode == MonitorPong) {
        if (m_pongRetryCount > 0)
            COPYQ_LOG( QString("Remote process: Pong received on try %1/%2")
                   .arg(QString::number(m_pongRetryCount))
                   .arg(QString::number(pingMaxRetries)) );
        m_pongRetryCount = 0;
        m_timerPongTimeout.stop();
        m_timerPing.start();
    } else if (messageCode == MonitorLog) {
        log( getTextData(message).trimmed(), LogNote );
    } else if (messageCode == MonitorClipboardChanged) {
        emit newMessage(message);
    } else if (messageCode == 0) {
        // Ignore message with Arguments.
    } else {
        log( QString("Unknown message code %1 from remote process!").arg(messageCode), LogError );
    }
}

void RemoteProcess::onConnectionError(const QString &error)
{
    m_timerPing.stop();
    m_timerPongTimeout.stop();
    m_state = Unconnected;
    emit connectionError(error);
}

void RemoteProcess::onDisconnected()
{
    onConnectionError("Disconnected");
}

void RemoteProcess::terminate()
{
    if (!m_process)
        return;

    if (m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(100))
            m_process->kill();
    }

    m_process->deleteLater();
    m_process = nullptr;
}

void RemoteProcess::writeMessage(const QByteArray &msg, int messageCode)
{
    emit sendMessage(msg, messageCode);
}

bool RemoteProcess::isConnected() const
{
    return m_state == Connected;
}

void RemoteProcess::ping()
{
    if ( checkConnection() ) {
        writeMessage( QByteArray(), MonitorPing );
        m_timerPing.stop();
        m_timerPongTimeout.start();
    }
}

void RemoteProcess::pongTimeout()
{
    if (m_pongRetryCount < pingMaxRetries) {
        ++m_pongRetryCount;
        COPYQ_LOG( QString("Remote process: Resending ping %1/%2")
                   .arg(QString::number(m_pongRetryCount))
                   .arg(QString::number(pingMaxRetries)) );
        ping();
    } else {
        onConnectionError("Connection timeout");
    }
}
