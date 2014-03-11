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
#include "../qt/bytearrayclass.h"

#include <QApplication>
#include <QLocalSocket>
#include <QObject>
#include <QScriptEngine>

Q_DECLARE_METATYPE(QByteArray*)

ScriptableWorker::ScriptableWorker(const QSharedPointer<MainWindow> &mainWindow,
                                   const Arguments &args, ClientSocket *socket)
    : QRunnable()
    , m_wnd(mainWindow)
    , m_args(args)
    , m_socket(socket)
{
}

void ScriptableWorker::run()
{
#ifdef COPYQ_LOG_DEBUG
    QString msg = QString("Scripting engine: %1");
#endif

    COPYQ_LOG( msg.arg("starting") );

    QScriptEngine engine;
    ScriptableProxy proxy(m_wnd);
    Scriptable scriptable(&proxy);
    scriptable.initEngine( &engine, QString::fromUtf8(m_args.at(Arguments::CurrentPath)) );

    if (m_socket) {
        QObject::connect( &scriptable, SIGNAL(sendMessage(QByteArray,int)),
                          m_socket, SLOT(sendMessage(QByteArray,int)) );
        QObject::connect( m_socket, SIGNAL(messageReceived(QByteArray)),
                          &scriptable, SLOT(setInput(QByteArray)) );

        QObject::connect( m_socket, SIGNAL(disconnected()),
                          &scriptable, SLOT(abort()) );
        QObject::connect( &scriptable, SIGNAL(destroyed()),
                          m_socket, SLOT(deleteAfterDisconnected()) );

        if ( m_socket->isClosed() ) {
            COPYQ_LOG( msg.arg("terminated") );
            return;
        }

        m_socket->start();
    }

    QObject::connect( &scriptable, SIGNAL(requestApplicationQuit()),
                      qApp, SLOT(quit()) );

    QByteArray response;
    int exitCode;

    if ( m_args.length() <= Arguments::Rest ) {
        COPYQ_LOG( msg.arg("Error: bad command syntax") );
        exitCode = CommandBadSyntax;
    } else {
        const QString cmd = QString::fromUtf8( m_args.at(Arguments::Rest) );
#ifdef COPYQ_LOG_DEBUG
        COPYQ_LOG( msg.arg(QString("Client arguments:")) );
        for (int i = Arguments::Rest; i < m_args.length(); ++i)
            COPYQ_LOG( msg.arg("    " + QString::fromLocal8Bit(m_args.at(i))) );
#endif

#ifdef HAS_TESTS
        if ( cmd == "flush" && m_args.length() == Arguments::Rest + 2 ) {
            COPYQ_LOG( msg.arg(QString("flush ID:") + QString::fromLocal8Bit(m_args.at(Arguments::Rest + 1))) );
            scriptable.sendMessageToClient(QByteArray(), CommandFinished);
            return;
        }
#endif

        QScriptValue fn = engine.globalObject().property(cmd);
        if ( !fn.isFunction() ) {
            COPYQ_LOG( msg.arg("Error: unknown command") );
            response = createLogMessage("CopyQ client",
                                        Scriptable::tr("Name \"%1\" doesn't refer to a function.")
                                        .arg(cmd),
                                        LogError).toLocal8Bit();
            exitCode = CommandError;
        } else {
            QScriptValueList fnArgs;
            for ( int i = Arguments::Rest + 1; i < m_args.length(); ++i )
                fnArgs.append( scriptable.newByteArray(m_args.at(i)) );

            QScriptValue result = fn.call(QScriptValue(), fnArgs);

            if ( engine.hasUncaughtException() ) {
                const QString exceptionText = engine.uncaughtException().toString();
                COPYQ_LOG( msg.arg(QString("Error: exception in command \"%1\": %2")
                                   .arg(cmd).arg(exceptionText)) );
                response = createLogMessage("CopyQ client", exceptionText, LogError).toLocal8Bit();
                exitCode = CommandError;
            } else {
                QByteArray *bytes = qscriptvalue_cast<QByteArray*>(result.data());
                if (bytes != NULL)
                    response = *bytes;
                else if (!result.isUndefined())
                    response = result.toString().toLocal8Bit() + '\n';
                exitCode = CommandFinished;
            }
        }
    }

    scriptable.sendMessageToClient(response, exitCode);

    COPYQ_LOG( msg.arg("finished") );
}
