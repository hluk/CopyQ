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
#include "scriptable.h"
#include "../qt/bytearrayclass.h"

#include <QApplication>
#include <QLocalSocket>
#include <QScriptEngine>

Q_DECLARE_METATYPE(QByteArray*)

ScriptableWorker::ScriptableWorker(const QSharedPointer<MainWindow> &mainWindow,
                                   const Arguments &args, QObject *socket, QObject *parent)
    : QObject(parent)
    , QRunnable()
    , m_wnd(mainWindow)
    , m_args(args)
    , m_socket(socket)
    , m_terminated(false)
{
    if (m_socket) {
        connect(m_socket, SIGNAL(disconnected()), SLOT(terminate()));
        connect(this, SIGNAL(terminated()), m_socket, SLOT(deleteAfterDisconnected()));
    }
}

void ScriptableWorker::run()
{
    if (m_terminated)
        return;

#ifdef COPYQ_LOG_DEBUG
    QString msg = QString("Scripting engine: %1");
#endif

    COPYQ_LOG( msg.arg("starting") );

    QScriptEngine engine;
    ScriptableProxy proxy(m_wnd);
    Scriptable scriptable(&proxy);
    scriptable.abort();
    scriptable.initEngine( &engine, QString::fromUtf8(m_args.at(Arguments::CurrentPath)) );

    if (m_socket) {
        connect( &scriptable, SIGNAL(sendMessage(QByteArray,int)),
                 m_socket, SLOT(sendMessage(QByteArray,int)) );
        connect( m_socket, SIGNAL(messageReceived(QByteArray)),
                 &scriptable, SLOT(setInput(QByteArray)) );
    }

    connect( &scriptable, SIGNAL(requestApplicationQuit()),
             qApp, SLOT(quit()) );
    connect( this, SIGNAL(terminated()),
             &scriptable, SLOT(abort()) );

    QByteArray response;
    int exitCode;

    if ( m_args.length() <= Arguments::Rest ) {
        COPYQ_LOG( msg.arg("bad command syntax") );
        exitCode = CommandBadSyntax;
    } else {
        const QString cmd = QString::fromUtf8( m_args.at(Arguments::Rest) );

        QScriptValue fn = engine.globalObject().property(cmd);
        if ( !fn.isFunction() ) {
            COPYQ_LOG( msg.arg("unknown command") );
            response = Scriptable::tr("Name \"%1\" doesn't refer to a function.")
                    .arg(cmd).toLocal8Bit() + '\n';
            exitCode = CommandBadSyntax;
        } else {
            QScriptValueList fnArgs;
            for ( int i = Arguments::Rest + 1; i < m_args.length(); ++i )
                fnArgs.append( scriptable.newByteArray(m_args.at(i)) );

            QScriptValue result = fn.call(QScriptValue(), fnArgs);

            if ( engine.hasUncaughtException() ) {
                COPYQ_LOG( msg.arg("command error (\"%1\")").arg(cmd) );
                response = engine.uncaughtException().toString().toLocal8Bit() + '\n';
                exitCode = CommandError;
                engine.clearExceptions();
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

    emit terminated();
}

void ScriptableWorker::terminate()
{
    if (m_terminated)
        return;

#ifdef COPYQ_LOG_DEBUG
    COPYQ_LOG( QString("Scripting engine: %1").arg("terminating") );
#endif

    m_terminated = true;
    emit terminated();
}
