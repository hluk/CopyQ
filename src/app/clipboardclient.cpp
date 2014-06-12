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

#include "clipboardclient.h"

#include "common/arguments.h"
#include "common/client_server.h"
#include "common/commandstatus.h"
#include "common/log.h"
#include "platform/platformnativeinterface.h"
#include "platform/platformwindow.h"

#include <QCoreApplication>
#include <QFile>

ClipboardClient::ClipboardClient(int &argc, char **argv, int skipArgc, const QString &sessionName)
    : Client()
    , App(createPlatformNativeInterface()->createClientApplication(argc, argv), sessionName)
{
    Arguments arguments(argc, argv, skipArgc + 1);
    if ( !startClientSocket(clipboardServerName(), arguments) ) {
        log( tr("Cannot connect to server! Start CopyQ server first."), LogError );
        exit(1);
    }
}

void ClipboardClient::onMessageReceived(const QByteArray &data, int messageCode)
{
    if (messageCode == CommandActivateWindow) {
        COPYQ_LOG("Activating window.");
        PlatformWindowPtr window = createPlatformNativeInterface()->deserialize(data);
        if (window)
            window->raise();
    } else if (messageCode == CommandReadInput) {
        COPYQ_LOG("Sending standard input.");
        QFile in;
        in.open(stdin, QIODevice::ReadOnly);
        sendMessage( in.readAll(), 0 );
    } else {
        QFile f;
        f.open((messageCode == CommandSuccess || messageCode == CommandFinished) ? stdout : stderr, QIODevice::WriteOnly);
        f.write(data);
    }

    COPYQ_LOG( QString("Message received with exit code %1.").arg(messageCode) );

    if (messageCode == CommandFinished || messageCode == CommandBadSyntax || messageCode == CommandError)
        exit(messageCode);
}

void ClipboardClient::onDisconnected()
{
    if ( wasClosed() )
        return;

    log( tr("Connection lost!"), LogError );
    exit(1);
}
