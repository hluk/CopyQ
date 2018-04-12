/*
    Copyright (c) 2018, Lukas Holecek <hluk@email.cz>

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
#include "common/commandstore.h"
#include "common/log.h"
#include "platform/platformnativeinterface.h"
#include "scriptable/scriptable.h"
#include "scriptable/scriptableproxy.h"

#include <QApplication>
#include <QFile>
#include <QScriptEngine>
#include <QSettings>
#include <QThread>
#include <QTimer>

namespace {

QString messageCodeToString(int code)
{
    switch (code) {
    case CommandFinished:
        return "CommandFinished";
    case CommandError:
        return "CommandError";
    case CommandBadSyntax:
        return "CommandBadSyntax";
    case CommandException:
        return "CommandException";
    case CommandPrint:
        return "CommandPrint";
    case CommandFunctionCallReturnValue:
        return "CommandFunctionCallReturnValue";
    default:
        return QString("Unknown(%1)").arg(code);
    }
}

void printClientStdout(const QByteArray &output)
{
    QFile f;
    f.open(stdout, QIODevice::WriteOnly);
    f.write(output);
}

void printClientStderr(const QByteArray &output)
{
    QFile f;
    f.open(stderr, QIODevice::WriteOnly);
    f.write(output);
    if ( !output.endsWith('\n') )
        f.write("\n");
}

QCoreApplication *createClientApplication(int &argc, char **argv, const QStringList &arguments)
{
    // Clipboard access requires QApplication.
    if ( !arguments.isEmpty() && (
             arguments[0] == "monitorClipboard"
             || arguments[0] == "provideClipboard"
             || arguments[0] == "provideSelection") )
    {
        return createPlatformNativeInterface()
                ->createClipboardProviderApplication(argc, argv);
    }

    return createPlatformNativeInterface()->createClientApplication(argc, argv);
}

} // namespace

void InputReader::readInput()
{
    QFile in;
    in.open(stdin, QIODevice::ReadOnly);

    QByteArray input = in.readAll();
    emit inputRead(input);
}

ClipboardClient::ClipboardClient(int &argc, char **argv, const QStringList &arguments, const QString &sessionName)
    : Client()
    , App("Client", createClientApplication(argc, argv, arguments), sessionName)
    , m_inputReaderThread(nullptr)
{
    restoreSettings();

    startClientSocket(clipboardServerName());

    bool hasActionData;
    const int id = qgetenv("COPYQ_ACTION_ID").toInt(&hasActionData);
    QByteArray message;
    if (hasActionData) {
        QDataStream out(&message, QIODevice::WriteOnly);
        out << id;
    }

    // Start script after QCoreApplication::exec().
    auto timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [&]() { start(arguments); });
    connect(timer, &QTimer::timeout, timer, &QObject::deleteLater);
    timer->start(0);
}

void ClipboardClient::onMessageReceived(const QByteArray &data, int messageCode)
{
    COPYQ_LOG_VERBOSE( "Message received: " + messageCodeToString(messageCode) );

    switch (messageCode) {
    case CommandFinished:
        printClientStdout(data);
        exit(0);
        break;

    case CommandError:
        exit(1);
        break;

    case CommandBadSyntax:
    case CommandException:
        printClientStderr(data);
        exit(messageCode);
        break;

    case CommandPrint:
        printClientStdout(data);
        break;

    case CommandFunctionCallReturnValue:
        emit functionCallResultReceived(data);
        break;

    default:
        log( "Unhandled message: " + messageCodeToString(messageCode), LogError );
        break;
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

void ClipboardClient::onConnectionFailed()
{
    log( tr("Cannot connect to server! Start CopyQ server first."), LogError );
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
        emit inputReceived(m_input);
}

void ClipboardClient::exit(int exitCode)
{
    emit functionCallResultReceived(QByteArray());
    abortInputReader();
    App::exit(exitCode);
}

void ClipboardClient::sendFunctionCall(const QByteArray &bytes)
{
    sendMessage(bytes, CommandFunctionCall);

    QEventLoop loop;
    connect(this, SIGNAL(functionCallResultReceived(QByteArray)), &loop, SLOT(quit()));
    connect(qApp, SIGNAL(aboutToQuit()), &loop, SLOT(quit()));
    loop.exec();
}

void ClipboardClient::startInputReader()
{
    if ( wasClosed() || m_inputReaderThread )
        return;

    if ( isInputReaderFinished() ) {
        sendInput();
        return;
    }

    auto reader = new InputReader;
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

void ClipboardClient::start(const QStringList &arguments)
{
    QScriptEngine engine;
    ScriptableProxy scriptableProxy(nullptr, nullptr);
    Scriptable scriptable(&engine, &scriptableProxy);

    connect( &scriptable, SIGNAL(sendMessage(QByteArray,int)),
             this, SLOT(onMessageReceived(QByteArray,int)) );
    connect( &scriptable, SIGNAL(readInput()),
             this, SLOT(startInputReader()) );
    connect( &scriptableProxy, SIGNAL(sendFunctionCall(QByteArray)),
             this, SLOT(sendFunctionCall(QByteArray)) );

    connect( this, SIGNAL(inputReceived(QByteArray)),
             &scriptable, SLOT(setInput(QByteArray)) );
    connect( this, SIGNAL(functionCallResultReceived(QByteArray)),
             &scriptableProxy, SLOT(setReturnValue(QByteArray)) );

    scriptable.executeArguments(arguments);
}
