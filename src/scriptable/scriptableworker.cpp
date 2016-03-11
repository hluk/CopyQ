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

#include "scriptableworker.h"

#include "common/action.h"
#include "common/client_server.h"
#include "common/clientsocket.h"
#include "common/commandstatus.h"
#include "common/log.h"
#include "../qt/bytearrayclass.h"

#include <QApplication>
#include <QObject>
#include <QScriptEngine>

Q_DECLARE_METATYPE(QByteArray*)

namespace {

#define SCRIPT_LOG(text) \
    COPYQ_LOG( QString("Script %1: %2").arg(m_id).arg(text) )

QByteArray serializeScriptValue(const QScriptValue &value)
{
    QByteArray data;

    QByteArray *bytes = qscriptvalue_cast<QByteArray*>(value.data());

    if (bytes != NULL) {
        data = *bytes;
    } else if ( value.isArray() ) {
        const quint32 len = value.property("length").toUInt32();
        for (quint32 i = 0; i < len; ++i)
            data += serializeScriptValue(value.property(i));
    } else if ( !value.isUndefined() ) {
        data = value.toString().toUtf8() + '\n';
    }

    return data;
}

} // namespace

ScriptableWorker::ScriptableWorker(
        MainWindow *mainWindow, const Arguments &args, ClientSocket *socket,
        const QString &pluginScript)
    : QRunnable()
    , m_wnd(mainWindow)
    , m_args(args)
    , m_socket(socket)
    , m_pluginScript(pluginScript)
{
    if ( hasLogLevel(LogDebug) )
        m_id = m_socket->property("id").toString();
}

void ScriptableWorker::run()
{
    if ( hasLogLevel(LogDebug) ) {
        bool isEval = m_args.length() == Arguments::Rest + 2
                && m_args.at(Arguments::Rest) == "eval";

        for (int i = Arguments::Rest + (isEval ? 1 : 0); i < m_args.length(); ++i) {
            QString indent = isEval ? QString("EVAL:")
                                    : (QString::number(i - Arguments::Rest + 1) + " ");
            foreach (const QByteArray &line, m_args.at(i).split('\n')) {
                SCRIPT_LOG( indent + getTextData(line) );
                indent = "  ";
            }
        }
    }

    bool hasData;
    const quintptr id = m_args.at(Arguments::ActionId).toULongLong(&hasData);
    QVariantMap data;
    if (hasData)
        data = Action::data(id);

    const QString currentPath = getTextData(m_args.at(Arguments::CurrentPath));

    QScriptEngine engine;
    ScriptableProxy proxy(m_wnd, data);
    Scriptable scriptable(&proxy);
    scriptable.initEngine(&engine, currentPath, data);

    if (m_socket) {
        QObject::connect( proxy.signaler(), SIGNAL(sendMessage(QByteArray,int)),
                          m_socket, SLOT(sendMessage(QByteArray,int)) );

        QObject::connect( &scriptable, SIGNAL(sendMessage(QByteArray,int)),
                          m_socket, SLOT(sendMessage(QByteArray,int)) );
        QObject::connect( m_socket, SIGNAL(messageReceived(QByteArray,int)),
                          &scriptable, SLOT(setInput(QByteArray)) );

        QObject::connect( m_socket, SIGNAL(disconnected()),
                          &scriptable, SLOT(abort()) );
        QObject::connect( &scriptable, SIGNAL(destroyed()),
                          m_socket, SLOT(deleteAfterDisconnected()) );

        if ( m_socket->isClosed() ) {
            SCRIPT_LOG("TERMINATED");
            return;
        }

        m_socket->start();
    }

    QObject::connect( &scriptable, SIGNAL(requestApplicationQuit()),
                      qApp, SLOT(quit()) );

    QByteArray response;
    int exitCode;

    if ( m_args.length() <= Arguments::Rest ) {
        SCRIPT_LOG("Error: bad command syntax");
        exitCode = CommandBadSyntax;
    } else {
        const QString cmd = getTextData( m_args.at(Arguments::Rest) );

#ifdef HAS_TESTS
        if ( cmd == "flush" && m_args.length() == Arguments::Rest + 2 ) {
            log( "flush ID: " + getTextData(m_args.at(Arguments::Rest + 1)), LogAlways );
            scriptable.sendMessageToClient(QByteArray(), CommandFinished);
            return;
        }
#endif

        QScriptValue fn = engine.globalObject().property(cmd);
        if ( !fn.isFunction() ) {
            SCRIPT_LOG("Error: unknown command");
            const QString msg =
                    Scriptable::tr("Name \"%1\" doesn't refer to a function.").arg(cmd);
            response = createLogMessage(msg, LogError).toUtf8();
            exitCode = CommandError;
        } else {
            /* Special arguments:
             * "-"  read this argument from stdin
             * "--" read all following arguments without control sequences
             */
            QScriptValueList fnArgs;
            bool readRaw = false;
            for ( int i = Arguments::Rest + 1; i < m_args.length(); ++i ) {
                const QByteArray &arg = m_args.at(i);
                if (!readRaw && arg == "--") {
                    readRaw = true;
                } else {
                    const QScriptValue value = readRaw || arg != "-"
                            ? scriptable.newByteArray(arg)
                            : scriptable.input();
                    fnArgs.append(value);
                }
            }

            engine.evaluate(m_pluginScript);
            QScriptValue result = fn.call(QScriptValue(), fnArgs);

            if ( engine.hasUncaughtException() ) {
                const QString exceptionText =
                        QString("%1\n--- backtrace ---\n%2\n--- end backtrace ---")
                        .arg( engine.uncaughtException().toString(),
                              engine.uncaughtExceptionBacktrace().join("\n") );

                SCRIPT_LOG( QString("Error: Exception in command \"%1\": %2")
                             .arg(cmd, exceptionText) );

                response = createLogMessage(exceptionText, LogError).toUtf8();
                exitCode = CommandError;
            } else {
                response = serializeScriptValue(result);
                exitCode = CommandFinished;
            }
        }
    }

    if (exitCode == CommandFinished && hasData)
        Action::setData(id, scriptable.data());

    scriptable.sendMessageToClient(response, exitCode);

    SCRIPT_LOG("DONE");
}
