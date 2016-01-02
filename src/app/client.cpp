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

#include "client.h"

#include "common/arguments.h"
#include "common/clientsocket.h"
#include "platform/platformnativeinterface.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QLocalSocket>
#include <QThread>

Client::Client(QObject *parent)
    : QObject(parent)
{
}

void Client::sendMessage(const QByteArray &message, int messageCode)
{
    emit sendMessageRequest(message, messageCode);
}

bool Client::startClientSocket(const QString &serverName, int argc, char **argv, int skipArgc)
{
    QLocalSocket *localSocket = new QLocalSocket(this);
    localSocket->connectToServer(serverName);
    if ( !localSocket->waitForConnected(4000) )
        return false;

    Arguments arguments(
                createPlatformNativeInterface()->getCommandLineArguments(argc, argv)
                .mid(skipArgc) );

    ClientSocket *socket = new ClientSocket(localSocket);

    QThread *t = new QThread;
    connect(socket, SIGNAL(destroyed()), t, SLOT(quit()));
    connect(t, SIGNAL(finished()), t, SLOT(deleteLater()));
    socket->moveToThread(t);
    t->start();

    connect( socket, SIGNAL(messageReceived(QByteArray,int)),
             this, SLOT(onMessageReceived(QByteArray,int)) );
    connect( socket, SIGNAL(disconnected()),
             this, SLOT(onDisconnected()) );
    connect( qApp, SIGNAL(aboutToQuit()),
             socket, SLOT(deleteAfterDisconnected()) );
    connect( this, SIGNAL(sendMessageRequest(QByteArray,int)),
             socket, SLOT(sendMessage(QByteArray,int)) );

    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);
    out << arguments;
    sendMessage(msg, 0);

    socket->start();

    return true;
}
