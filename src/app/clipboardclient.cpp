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

#include "clipboardclient.h"

#include "common/client_server.h"
#include "common/commandstatus.h"
#include "common/commandstore.h"
#include "common/log.h"
#include "item/itemfactory.h"
#include "platform/platformnativeinterface.h"
#include "scriptable/scriptable.h"
#include "scriptable/scriptableproxy.h"

#include <QCoreApplication>
#include <QFile>
#include <QScriptEngine>
#include <QSettings>
#include <QThread>

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
    case CommandSetScripts:
        return "CommandSetScripts";
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

} // namespace

void InputReader::readInput()
{
    QFile in;
    in.open(stdin, QIODevice::ReadOnly);

    QByteArray input = in.readAll();
    emit inputRead(input);
}

ClipboardClient::ClipboardClient(int &argc, char **argv, int skipArgc, const QString &sessionName)
    : Client()
    , App("Client", createPlatformNativeInterface()->createClientApplication(argc, argv), sessionName)
    , m_inputReaderThread(nullptr)
    , m_arguments( createPlatformNativeInterface()->getCommandLineArguments(argc, argv).mid(skipArgc) )
{
    restoreSettings();

    auto engine = new QScriptEngine(this);
    m_scriptableProxy = new ScriptableProxy(nullptr, this);
    m_scriptable = new Scriptable(engine, m_scriptableProxy);

    QObject::connect( m_scriptable, SIGNAL(sendMessage(QByteArray,int)),
                      this, SLOT(onMessageReceived(QByteArray,int)) );
    QObject::connect( m_scriptable, SIGNAL(readInput()),
                      this, SLOT(startInputReader()) );
    QObject::connect( m_scriptableProxy, SIGNAL(sendFunctionCall(QByteArray)),
                      this, SLOT(sendFunctionCall(QByteArray)) );

    startClientSocket(clipboardServerName());
    sendMessage(QByteArray(), CommandGetScripts);
}

void ClipboardClient::onMessageReceived(const QByteArray &data, int messageCode)
{
    COPYQ_LOG( "Message received: " + messageCodeToString(messageCode) );

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

    case CommandSetScripts:
        start(data);
        break;

    case CommandFunctionCallReturnValue:
        m_scriptableProxy->setReturnValue(data);
        emit functionCallResultReceived();
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
        m_scriptable->setInput(m_input);
}

void ClipboardClient::exit(int exitCode)
{
    m_scriptableProxy->setReturnValue(QByteArray());
    abortInputReader();
    App::exit(exitCode);
}

void ClipboardClient::sendFunctionCall(const QByteArray &bytes)
{
    sendMessage(bytes, CommandFunctionCall);

    QEventLoop loop;
    connect(this, SIGNAL(functionCallResultReceived()), &loop, SLOT(quit()));
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

void ClipboardClient::start(const QByteArray &scriptsData)
{
    const auto commands = importCommandsFromText( QString::fromUtf8(scriptsData) );

    ItemFactory factory;
    factory.loadPlugins();

    QSettings settings;
    factory.loadItemFactorySettings(&settings);

    const auto scriptableObjects = factory.scriptableObjects();

    const auto engine = m_scriptable->engine();
    auto plugins = engine->newObject();
    engine->globalObject().setProperty("plugins", plugins);

    for (auto obj : scriptableObjects) {
        const auto value = engine->newQObject(obj, QScriptEngine::ScriptOwnership);
        const auto name = obj->objectName();
        plugins.setProperty(name, value);
        obj->setScriptable(m_scriptable);
        obj->start();
    }

    if ( !m_scriptable->sourceScriptCommands(commands) )
        return;

    m_scriptable->executeArguments(m_arguments);
}
