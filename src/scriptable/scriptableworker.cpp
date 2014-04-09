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

#include "scriptableworker.h"

#include "common/client_server.h"
#include "common/clientsocket.h"
#include "common/commandstatus.h"
#include "../qt/bytearrayclass.h"

#include <QApplication>
#include <QObject>
#include <QScriptEngine>

Q_DECLARE_METATYPE(QByteArray*)

#define MONITOR_LOG(msg) \
    COPYQ_LOG( QString("Scripting engine: %1").arg(msg) );

ScriptableWorker::ScriptableWorker(const MainWindowPtr &mainWindow,
                                   const Arguments &args, ClientSocket *socket)
    : QRunnable()
    , m_wnd(mainWindow)
    , m_args(args)
    , m_socket(socket)
{
}

void ScriptableWorker::run()
{
    MONITOR_LOG("starting");

    QScriptEngine engine;
    ScriptableProxy proxy(m_wnd);
    Scriptable scriptable(&proxy);
    scriptable.initEngine( &engine, QString::fromUtf8(m_args.at(Arguments::CurrentPath)),
                           m_args.at(Arguments::ActionId) );

    if (m_socket) {
        QObject::connect( &scriptable, SIGNAL(sendMessage(QByteArray,int)),
                          m_socket, SLOT(sendMessage(QByteArray,int)) );
        QObject::connect( m_socket, SIGNAL(messageReceived(QByteArray,int)),
                          &scriptable, SLOT(setInput(QByteArray)) );

        QObject::connect( m_socket, SIGNAL(disconnected()),
                          &scriptable, SLOT(abort()) );
        QObject::connect( &scriptable, SIGNAL(destroyed()),
                          m_socket, SLOT(deleteAfterDisconnected()) );

        if ( m_socket->isClosed() ) {
            MONITOR_LOG("terminated");
            return;
        }

        m_socket->start();
    }

    QObject::connect( &scriptable, SIGNAL(requestApplicationQuit()),
                      qApp, SLOT(quit()) );

    QByteArray response;
    int exitCode;

    if ( m_args.length() <= Arguments::Rest ) {
        MONITOR_LOG("Error: bad command syntax");
        exitCode = CommandBadSyntax;
    } else {
        const QString cmd = QString::fromUtf8( m_args.at(Arguments::Rest) );

        if ( hasLogLevel(LogDebug) ) {
            MONITOR_LOG("Client arguments:");
            for (int i = Arguments::Rest; i < m_args.length(); ++i)
                MONITOR_LOG( "    " + QString::fromUtf8(m_args.at(i)) );
        }

#ifdef HAS_TESTS
        if ( cmd == "flush" && m_args.length() == Arguments::Rest + 2 ) {
            MONITOR_LOG( "flush ID: " + QString::fromUtf8(m_args.at(Arguments::Rest + 1)) );
            scriptable.sendMessageToClient(QByteArray(), CommandFinished);
            return;
        }
#endif

        QScriptValue fn = engine.globalObject().property(cmd);
        if ( !fn.isFunction() ) {
            MONITOR_LOG("Error: unknown command");
            response = createLogMessage("CopyQ client",
                                        Scriptable::tr("Name \"%1\" doesn't refer to a function.")
                                        .arg(cmd),
                                        LogError).toUtf8();
            exitCode = CommandError;
        } else {
            QScriptValueList fnArgs;
            for ( int i = Arguments::Rest + 1; i < m_args.length(); ++i )
                fnArgs.append( scriptable.newByteArray(m_args.at(i)) );

            QScriptValue result = fn.call(QScriptValue(), fnArgs);

            if ( engine.hasUncaughtException() ) {
                const QString exceptionText = engine.uncaughtException().toString();
                MONITOR_LOG( QString("Error: exception in command \"%1\": %2")
                             .arg(cmd).arg(exceptionText) );
                response = createLogMessage("CopyQ client", exceptionText, LogError).toUtf8();
                exitCode = CommandError;
            } else {
                QByteArray *bytes = qscriptvalue_cast<QByteArray*>(result.data());
                if (bytes != NULL)
                    response = *bytes;
                else if (!result.isUndefined())
                    response = result.toString().toUtf8() + '\n';
                exitCode = CommandFinished;
            }
        }
    }

    scriptable.sendMessageToClient(response, exitCode);

    MONITOR_LOG("finished");
}
