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

#include "remoteprocess.h"

#include "client_server.h"

#include <QCoreApplication>
#include <QByteArray>
#include <QLocalServer>
#include <QLocalSocket>
#include <QString>

RemoteProcess::RemoteProcess(QObject *parent)
    : QObject(parent)
    , m_process()
    , m_server(NULL)
    , m_socket(NULL)
{
}

RemoteProcess::~RemoteProcess()
{
    closeConnection();
}

void RemoteProcess::start(const QString &newServerName, const QStringList &arguments)
{
    Q_ASSERT(m_server == NULL);
    Q_ASSERT(m_socket == NULL);
    Q_ASSERT(!isConnected());
    if ( isConnected() )
        return;

    m_server = newServer(newServerName, &m_process);

    m_process.start( QCoreApplication::applicationFilePath(), arguments );

    if ( m_process.waitForStarted(2000) && m_server->waitForNewConnection(2000) ) {
        m_socket = m_server->nextPendingConnection();
        connect( m_socket, SIGNAL(readyRead()),
                 this, SLOT(readyRead()) );
    }
}

bool RemoteProcess::writeMessage(const QByteArray &msg)
{
    Q_ASSERT(m_server != NULL);
    Q_ASSERT(m_socket != NULL);
    Q_ASSERT(isConnected());
    if ( !isConnected() )
        return false;

    ::writeMessage(m_socket, msg);
    while ( m_socket->bytesToWrite() > 0 ) {
        if ( !m_socket->waitForBytesWritten() )
            return false;
    }

    return true;
}

bool RemoteProcess::isConnected() const
{
    return m_socket != NULL && m_socket->isWritable() && m_process.state() == QProcess::Running;
}

void RemoteProcess::closeConnection()
{
    if (m_server != NULL) {
        if (m_socket != NULL) {
            m_socket->disconnectFromServer();
            m_socket->deleteLater();
            m_socket = NULL;
        }

        m_server->close();
        m_server->deleteLater();
        m_server = NULL;

        if ( m_process.state() != QProcess::NotRunning && !m_process.waitForFinished(1000) ) {
            log( "Remote process: Close connection unsucessful!", LogError );
            m_process.terminate();
            if ( m_process.state() != QProcess::NotRunning && !m_process.waitForFinished(1002) ) {
                log( "Remote process: Cannot terminate process!", LogError );
                m_process.kill();
                if ( m_process.state() != QProcess::NotRunning ) {
                    log( "Remote process: Cannot kill process!!!", LogError );
                }
            }
        }
    }
}

void RemoteProcess::readyRead()
{
    Q_ASSERT(m_server != NULL);
    Q_ASSERT(m_socket != NULL);
    Q_ASSERT(isConnected());

    m_socket->blockSignals(true);

    while ( m_socket->bytesAvailable() > 0 ) {
        QByteArray msg;
        if( !::readMessage(m_socket, &msg) ) {
            log( "Incorrect message from remote process.", LogError );
            emit connectionError();
            return;
        }
        emit newMessage(msg);
    }

    m_socket->blockSignals(false);
}
