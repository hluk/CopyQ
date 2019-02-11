/*
    Copyright (c) 2019, Lukas Holecek <hluk@email.cz>

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
#include "common/clientsocket.h"
#include "common/commandstatus.h"
#include "common/commandstore.h"
#include "common/common.h"
#include "common/log.h"
#include "common/textdata.h"
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
    case CommandFunctionCallReturnValue:
        return "CommandFunctionCallReturnValue";
    case CommandInputDialogFinished:
        return "CommandInputDialogFinished";
    case CommandStop:
        return "CommandStop";
    default:
        return QString("Unknown(%1)").arg(code);
    }
}

QCoreApplication *createClientApplication(int &argc, char **argv, const QStringList &arguments)
{
    // Clipboard access requires QApplication.
    if ( !arguments.isEmpty() && (
             arguments[0] == "monitorClipboard"
             || arguments[0] == "provideClipboard"
             || arguments[0] == "provideSelection"
#ifdef HAS_MOUSE_SELECTIONS
             || arguments[0] == "synchronizeToSelection"
             || arguments[0] == "synchronizeFromSelection"
#endif
             ) )
    {
        QGuiApplication::setDesktopSettingsAware(false);
        const auto app = createPlatformNativeInterface()
                ->createClipboardProviderApplication(argc, argv);
        setCurrentThreadName(arguments[0]);
        return app;
    }

    const auto app = createPlatformNativeInterface()->createClientApplication(argc, argv);
    setCurrentThreadName("Client");
    return app;
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
    : App(createClientApplication(argc, argv, arguments), sessionName)
    , m_inputReaderThread(nullptr)
{
    restoreSettings();

    const auto serverName = clipboardServerName();
    m_socket = new ClientSocket(serverName, this);

    connect( m_socket, &ClientSocket::messageReceived,
             this, &ClipboardClient::onMessageReceived );
    connect( m_socket, &ClientSocket::disconnected,
             this, &ClipboardClient::onDisconnected );
    connect( m_socket, &ClientSocket::connectionFailed,
             this, &ClipboardClient::onConnectionFailed );

    m_socket->start();

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
    case CommandFunctionCallReturnValue:
        emit functionCallResultReceived(data);
        break;

    case CommandInputDialogFinished: {
        emit inputDialogFinished(data);
        break;
    }

    case CommandStop: {
        emit stopEventLoops();

        // Stop any event loops started later.
        auto timer = new QTimer(this);
        timer->setInterval(250);
        connect(timer, &QTimer::timeout, this, &ClipboardClient::stopEventLoops);
        timer->start();
        break;
    }

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
    abortInputReader();
    App::exit(exitCode);
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
    connect( m_inputReaderThread, &QThread::started, reader, &InputReader::readInput );
    connect( m_inputReaderThread, &QThread::finished, reader, &InputReader::deleteLater );
    connect( reader, &InputReader::inputRead, this, &ClipboardClient::setInput );
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

    connect( &scriptable, &Scriptable::readInput,
             this, &ClipboardClient::startInputReader );
    connect( &scriptableProxy, &ScriptableProxy::sendMessage,
             m_socket, &ClientSocket::sendMessage );

    connect( this, &ClipboardClient::inputReceived,
             &scriptable, &Scriptable::setInput );
    connect( this, &ClipboardClient::functionCallResultReceived,
             &scriptableProxy, &ScriptableProxy::setFunctionCallReturnValue );
    connect( this, &ClipboardClient::inputDialogFinished,
             &scriptableProxy, &ScriptableProxy::setInputDialogResult );

    connect( m_socket, &ClientSocket::disconnected,
             &scriptable, &Scriptable::abort );
    connect( m_socket, &ClientSocket::disconnected,
             &scriptableProxy, &ScriptableProxy::clientDisconnected );

    connect( this, &ClipboardClient::stopEventLoops,
             &scriptable, &Scriptable::stopEventLoops );
    connect( qApp, &QCoreApplication::aboutToQuit,
             &scriptable, &Scriptable::stopEventLoops );

    connect( &scriptable, &Scriptable::finished,
             this, &ClipboardClient::scriptableFinished );
    connect( &scriptable, &Scriptable::finished,
             &scriptableProxy, &ScriptableProxy::clientDisconnected );

    bool hasData;
#if QT_VERSION < QT_VERSION_CHECK(5,5,0)
    auto actionId = qgetenv("COPYQ_ACTION_ID").toInt(&hasData);
#else
    auto actionId = qEnvironmentVariableIntValue("COPYQ_ACTION_ID", &hasData);
#endif
    if (!hasData)
        actionId = -1;
    scriptable.setActionId(actionId);

    const auto actionName = getTextData( qgetenv("COPYQ_ACTION_NAME") );
    scriptable.setActionName(actionName);

    const int exitCode = scriptable.executeArguments(arguments);
    exit(exitCode);
}
