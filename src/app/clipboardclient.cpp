// SPDX-License-Identifier: GPL-3.0-or-later

#include "clipboardclient.h"

#include "common/client_server.h"
#include "common/clientsocket.h"
#include "common/commandstatus.h"
#include "common/commandstore.h"
#include "common/common.h"
#include "common/log.h"
#include "common/textdata.h"
#include "item/itemfactory.h"
#include "platform/platformnativeinterface.h"
#include "scriptable/scriptable.h"
#include "scriptable/scriptableproxy.h"

#include <QApplication>
#include <QFile>
#include <QJSEngine>
#include <QThread>
#include <QTimer>

namespace {

QString messageCodeToString(int code)
{
    switch (code) {
    case CommandFunctionCallReturnValue:
        return QStringLiteral("CommandFunctionCallReturnValue");
    case CommandInputDialogFinished:
        return QStringLiteral("CommandInputDialogFinished");
    case CommandStop:
        return QStringLiteral("CommandStop");
    case CommandData:
        return QStringLiteral("CommandData");
    default:
        return QStringLiteral("Unknown(%1)").arg(code);
    }
}

QCoreApplication *createClientApplication(int &argc, char **argv, const QStringList &arguments)
{
    // Clipboard access requires QApplication.
    if ( arguments.size() > 1 && arguments[0] == QLatin1String("--clipboard-access") ) {
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
    App::installTranslator();

    // Start script after QCoreApplication::exec().
    QTimer::singleShot(0, this, [&]() { start(arguments); });
}

void ClipboardClient::onMessageReceived(const QByteArray &data, int messageCode)
{
    COPYQ_LOG_VERBOSE( QLatin1String("Message received: ") + messageCodeToString(messageCode) );

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
        log( QLatin1String("Unhandled message: ") + messageCodeToString(messageCode), LogError );
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
    ItemFactory itemFactory;
    itemFactory.loadPlugins();
    QSettings settings;
    itemFactory.loadItemFactorySettings(&settings);

    QJSEngine engine;
    ScriptableProxy scriptableProxy(nullptr, nullptr);
    Scriptable scriptable(&engine, &scriptableProxy, &itemFactory);

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

    connect( this, &ClipboardClient::dataReceived,
             &scriptable, &Scriptable::dataReceived, Qt::QueuedConnection );
    connect( &scriptable, &Scriptable::receiveData,
             &socket, [&]() {
                socket.sendMessage(QByteArray(), CommandReceiveData);
             });

    bool hasActionId;
    auto actionId = qEnvironmentVariableIntValue("COPYQ_ACTION_ID", &hasActionId);
    const auto actionName = getTextData( qgetenv("COPYQ_ACTION_NAME") );

    if ( socket.start() ) {
        if (hasActionId)
            scriptable.setActionId(actionId);
        scriptable.setActionName(actionName);

        const int exitCode = scriptable.executeArguments(arguments);
        socket.disconnect(&scriptable);
        exit(exitCode);
    }
}
