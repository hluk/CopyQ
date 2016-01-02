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

#include "clipboardclient.h"

#include "common/client_server.h"
#include "common/commandstatus.h"
#include "common/log.h"
#include "platform/platformnativeinterface.h"
#include "platform/platformwindow.h"

#include <QCoreApplication>
#include <QFile>
#include <QThread>

void InputReader::readInput()
{
    QFile in;
    in.open(stdin, QIODevice::ReadOnly);

    QByteArray input = in.readAll();
    emit inputRead(input);
}

ClipboardClient::ClipboardClient(int &argc, char **argv, int skipArgc, const QString &sessionName)
    : Client()
    , App(createPlatformNativeInterface()->createClientApplication(argc, argv), sessionName)
    , m_inputReaderThread(NULL)
{
    restoreSettings();

    if ( !startClientSocket(clipboardServerName(), argc, argv, skipArgc) ) {
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
        startInputReader();
    } else {
        QFile f;
        f.open((messageCode == CommandSuccess || messageCode == CommandFinished) ? stdout : stderr, QIODevice::WriteOnly);
        f.write(data);
    }

    COPYQ_LOG( QString("Message received with exit code %1.").arg(messageCode) );

    if (messageCode == CommandFinished || messageCode == CommandBadSyntax || messageCode == CommandError) {
        abortInputReader();
        exit(messageCode);
    }
}

void ClipboardClient::onDisconnected()
{
    if ( wasClosed() )
        return;

    log( tr("Connection lost!"), LogError );

    abortInputReader();

    exit(1);
}

void ClipboardClient::setInput(const QByteArray &input)
{
    m_input = input;
    sendInput();
    abortInputReader();
}

void ClipboardClient::sendInput()
{
    if ( !wasClosed() )
        sendMessage(m_input, 0);
}

void ClipboardClient::startInputReader()
{
    if ( wasClosed() || m_inputReaderThread )
        return;

    if ( isInputReaderFinished() ) {
        sendInput();
        return;
    }

    InputReader *reader = new InputReader;
    m_inputReaderThread = new QThread(this);
    reader->moveToThread(m_inputReaderThread);
    connect( m_inputReaderThread, SIGNAL(started()), reader, SLOT(readInput()) );
    connect( m_inputReaderThread, SIGNAL(finished()), reader, SLOT(deleteLater()) );
    connect( reader, SIGNAL(inputRead(QByteArray)), this, SLOT(setInput(QByteArray)) );
    m_inputReaderThread->start();
}

void ClipboardClient::abortInputReader()
{
    if (m_inputReaderThread) {
        m_inputReaderThread->exit();
        if (!m_inputReaderThread->wait(2000)) {
            m_inputReaderThread->terminate();
            m_inputReaderThread->wait(2000);
        }
    }
}

bool ClipboardClient::isInputReaderFinished() const
{
    return m_inputReaderThread && m_inputReaderThread->isFinished();
}
