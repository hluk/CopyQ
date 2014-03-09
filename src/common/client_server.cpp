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

#include "common/client_server.h"
#include "common/common.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QIODevice>
#include <QLocalServer>
#include <QLocalSocket>
#include <QString>

bool readBytes(QIODevice *socket, qint64 size, QByteArray *bytes)
{
    qint64 avail, read = 0;
    bytes->clear();
    while (read < size) {
        if ( socket->bytesAvailable() == 0 && !socket->waitForReadyRead(1000) )
            return false;
        avail = qMin( socket->bytesAvailable(), size-read );
        bytes->append( socket->read(avail) );
        read += avail;
    }

    return true;
}

bool readMessage(QIODevice *socket, QByteArray *msg)
{
    QByteArray bytes;
    quint32 len;

    COPYQ_LOG_VERBOSE("Reading message.");

    if ( readBytes(socket, sizeof(len), &bytes) ) {
        QDataStream(bytes) >> len;

        if ( readBytes(socket, len, msg) ) {
            COPYQ_LOG_VERBOSE( QString("Message read (%1 bytes).").arg(msg->size()) );
            return true;
        }
    }

    COPYQ_LOG("ERROR: Incorrect message!");

    return false;
}

bool writeMessage(QIODevice *socket, const QByteArray &msg)
{
    COPYQ_LOG_VERBOSE( QString("Write message (%1 bytes).").arg(msg.size()) );

    QDataStream out(socket);
    // length is serialized as a quint32, followed by msg
    out.writeBytes( msg.constData(), msg.length() );

    if (out.status() != QDataStream::Ok) {
        COPYQ_LOG("Cannot write message!");
        return false;
    }

    COPYQ_LOG_VERBOSE("Message written.");
    return true;
}

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

QString serverName(const QString &name)
{
    // applicationName changes case depending on whether this is a GUI app
    // or a console app on OS X.
    return QCoreApplication::applicationName().toLower() + "_"
#ifdef Q_OS_WIN
            + qgetenv("USERNAME")
#else
            + qgetenv("USER")
#endif
            + "_" + name;
}

QString clipboardServerName()
{
    return serverName("s");
}
