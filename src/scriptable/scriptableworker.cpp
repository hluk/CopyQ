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

#include "scriptableworker.h"

#include "common/action.h"
#include "common/client_server.h"
#include "common/clientsocket.h"
#include "common/commandstatus.h"
#include "common/log.h"
#include "item/itemwidget.h"
#include "../qt/bytearrayclass.h"

#include <QApplication>
#include <QObject>
#include <QScriptEngine>

Q_DECLARE_METATYPE(QByteArray*)

namespace {

void createPluginObjects(
        const QList<ItemScriptableFactoryPtr> &criptableFactories,
        Scriptable *scriptable)
{
    const auto engine = scriptable->engine();
    auto plugins = engine->newObject();
    engine->globalObject().setProperty("plugins", plugins);

    for (auto scriptableFactory : criptableFactories) {
        const auto obj = scriptableFactory->create();
        const auto value = engine->newQObject(obj, QScriptEngine::ScriptOwnership);
        const auto name = scriptableFactory->name();
        plugins.setProperty(name, value);
        obj->setScriptable(scriptable);
        obj->start();
    }
}

class FakeSocket : public QObject {
    Q_OBJECT

public slots:
    void sendMessage(const QByteArray &message, int messageCode)
    {
        switch (messageCode) {
        case CommandFinished:
            log("finished", LogDebug);
            break;

        case CommandError:
        case CommandBadSyntax:
        case CommandException:
            log(message, LogWarning);
            break;

        case CommandPrint:
            log("print: " + message, LogNote);
            break;

        case CommandReadInput:
            break;

        default:
            break;
        }
    }

private:
    void log(const QString &message, LogLevel logLevel)
    {
        ::log("scripts: " + message, logLevel);
    }
};

} // namespace

ScriptableWorkerSocketGuard::ScriptableWorkerSocketGuard(const ClientSocketPtr &socket)
    : m_socket(socket)
{
}

ScriptableWorkerSocketGuard::~ScriptableWorkerSocketGuard()
{
    m_socket = nullptr;
}

ClientSocket *ScriptableWorkerSocketGuard::socket() const
{
    return m_socket.get();
}

ScriptableWorker::ScriptableWorker(MainWindow *mainWindow,
        const ClientSocketPtr &socket,
        const QList<ItemScriptableFactoryPtr> &scriptableFactories,
        const QVector<Command> &scriptCommands)
    : QRunnable()
    , m_wnd(mainWindow)
    , m_socketGuard(new ScriptableWorkerSocketGuard(socket))
    , m_scriptableFactories(scriptableFactories)
    , m_scriptCommands(scriptCommands)
{
}

void ScriptableWorker::run()
{
    auto socket = m_socketGuard->socket();

    setCurrentThreadName("Script-" + QString::number(socket->id()));

    QScriptEngine engine;
    ScriptableProxy proxy(m_wnd);
    Scriptable scriptable(&engine, &proxy);

    QObject::connect( &scriptable, SIGNAL(sendMessage(QByteArray,int)),
                      socket, SLOT(sendMessage(QByteArray,int)) );
    QObject::connect( socket, SIGNAL(messageReceived(QByteArray,int)),
                      &scriptable, SLOT(onMessageReceived(QByteArray,int)) );

    QObject::connect( socket, SIGNAL(disconnected()),
                      &scriptable, SLOT(onDisconnected()) );
    QObject::connect( socket, SIGNAL(connectionFailed()),
                      &scriptable, SLOT(onDisconnected()) );

    QMetaObject::invokeMethod(socket, "start", Qt::QueuedConnection);

    engine.globalObject().setProperty("registerDisplayFunction", QScriptValue::UndefinedValue);

    createPluginObjects(m_scriptableFactories, &scriptable);

    if ( scriptable.sourceScriptCommands(m_scriptCommands) ) {
        while ( scriptable.isConnected() )
            QCoreApplication::processEvents();
    }

    QMetaObject::invokeMethod(m_socketGuard, "deleteLater", Qt::QueuedConnection);
}

MainScriptableWorker::MainScriptableWorker(
        MainWindow *mainWindow,
        const QList<ItemScriptableFactoryPtr> &scriptableFactories,
        const QVector<Command> &scriptCommands,
        QObject *parent)
    : QThread(parent)
    , m_wnd(mainWindow)
    , m_scriptableFactories(scriptableFactories)
    , m_scriptCommands(scriptCommands)
{
}

void MainScriptableWorker::close()
{
    connect( this, SIGNAL(scriptStarted()), SLOT(close())  );
    emit closed();
}

void MainScriptableWorker::run()
{
    setCurrentThreadName("MainScript");
    FakeSocket socket;

    QScriptEngine engine;
    ScriptableProxy proxy(m_wnd);
    Scriptable scriptable(&engine, &proxy);

    QObject::connect( &scriptable, SIGNAL(sendMessage(QByteArray,int)),
                      &socket, SLOT(sendMessage(QByteArray,int)) );

    QObject::connect( this, SIGNAL(closed()),
                      &scriptable, SLOT(onDisconnected()) );

    emit scriptStarted();

    createPluginObjects(m_scriptableFactories, &scriptable);

    scriptable.runScripts(m_scriptCommands);
}

#include "scriptableworker.moc"
