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

#include "client.h"

#include "common/arguments.h"
#include "common/clientsocket.h"
#include "platform/platformnativeinterface.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QThread>

Client::Client(QObject *parent)
    : QObject(parent)
{
}

void Client::sendMessage(const QByteArray &message, int messageCode)
{
    emit sendMessageRequest(message, messageCode);
}

void Client::startClientSocket(const QString &serverName, int argc, char **argv, int skipArgc, int messageCode)
{
    Arguments arguments(
                createPlatformNativeInterface()->getCommandLineArguments(argc, argv)
                .mid(skipArgc) );

    ClientSocket *socket = new ClientSocket(serverName);

    connect( socket, SIGNAL(messageReceived(QByteArray,int)),
             this, SLOT(onMessageReceived(QByteArray,int)) );
    connect( socket, SIGNAL(disconnected()),
             this, SLOT(onDisconnected()) );
    connect( socket, SIGNAL(connectionFailed()),
             this, SLOT(onConnectionFailed()) );

    connect( socket, SIGNAL(disconnected()),
             socket, SLOT(deleteAfterDisconnected()) );
    connect( socket, SIGNAL(connectionFailed()),
             socket, SLOT(deleteAfterDisconnected()) );
    connect( qApp, SIGNAL(aboutToQuit()),
             socket, SLOT(deleteAfterDisconnected()) );

    connect( this, SIGNAL(sendMessageRequest(QByteArray,int)),
             socket, SLOT(sendMessage(QByteArray,int)) );

    socket->start();

    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);
    out << arguments;
    sendMessage(msg, messageCode);
}
