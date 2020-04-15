/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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
    case CommandData:
        return "CommandData";
    default:
        return QString("Unknown(%1)").arg(code);
    }
}

QCoreApplication *createClientApplication(int &argc, char **argv, const QStringList &arguments)
{
    // Clipboard access requires QApplication.
    if ( arguments.size() > 1 && arguments[0] == "--clipboard-access" ) {
        QGuiApplication::setDesktopSettingsAware(false);
        const auto app = platformNativeInterface()
                ->createClipboardProviderApplication(argc, argv);
        setLogLabel(arguments[1].toUtf8());
        return app;
    }

    const auto app = platformNativeInterface()->createClientApplication(argc, argv);
    setLogLabel("Client");
    return app;
}

} // namespace

ClipboardClient::ClipboardClient(int &argc, char **argv, const QStringList &arguments, const QString &sessionName)
    : App(createClientApplication(argc, argv, arguments), sessionName)
{
    restoreSettings();

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
        exit(0);
        break;
    }

    case CommandData:
        emit dataReceived(data);
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

    exit(1);
}

void ClipboardClient::onConnectionFailed()
{
    log( tr("Cannot connect to server! Start CopyQ server first."), LogError );
    exit(1);
}

void ClipboardClient::start(const QStringList &arguments)
{
    QScriptEngine engine;
    ScriptableProxy scriptableProxy(nullptr, nullptr);
    Scriptable scriptable(&engine, &scriptableProxy);

    const auto serverName = clipboardServerName();
    ClientSocket socket(serverName);

    connect( &socket, &ClientSocket::messageReceived,
             this, &ClipboardClient::onMessageReceived );
    connect( &socket, &ClientSocket::disconnected,
             this, &ClipboardClient::onDisconnected );
    connect( &socket, &ClientSocket::connectionFailed,
             this, &ClipboardClient::onConnectionFailed );

    connect( &scriptableProxy, &ScriptableProxy::sendMessage,
             &socket, &ClientSocket::sendMessage );

    connect( this, &ClipboardClient::functionCallResultReceived,
             &scriptableProxy, &ScriptableProxy::setFunctionCallReturnValue );
    connect( this, &ClipboardClient::inputDialogFinished,
             &scriptableProxy, &ScriptableProxy::setInputDialogResult );

    connect( &socket, &ClientSocket::disconnected,
             &scriptable, &Scriptable::abort );
    connect( &socket, &ClientSocket::disconnected,
             &scriptableProxy, &ScriptableProxy::clientDisconnected );

    connect( &scriptable, &Scriptable::finished,
             &scriptableProxy, &ScriptableProxy::clientDisconnected );

    connect( this, &ClipboardClient::dataReceived,
             &scriptable, &Scriptable::dataReceived, Qt::QueuedConnection );
    connect( &scriptable, &Scriptable::receiveData,
             &socket, [&]() {
                socket.sendMessage(QByteArray(), CommandReceiveData);
             });

    bool hasActionId;
#if QT_VERSION < QT_VERSION_CHECK(5,5,0)
    auto actionId = qgetenv("COPYQ_ACTION_ID").toInt(&hasActionId);
#else
    auto actionId = qEnvironmentVariableIntValue("COPYQ_ACTION_ID", &hasActionId);
#endif
    const auto actionName = getTextData( qgetenv("COPYQ_ACTION_NAME") );

    if ( socket.start() ) {
        if (hasActionId)
            scriptable.setActionId(actionId);
        scriptable.setActionName(actionName);

        const int exitCode = scriptable.executeArguments(arguments);
        exit(exitCode);
    }
}
