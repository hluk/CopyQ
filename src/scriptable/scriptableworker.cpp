/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#include "scriptable.h"
#include "../qt/bytearrayclass.h"

#include <QApplication>
#include <QLocalSocket>
#include <QScriptEngine>
#include <QThread>

Q_DECLARE_METATYPE(QByteArray*)

ScriptableWorker::ScriptableWorker(MainWindow *mainWindow, const Arguments &args,
                                   QLocalSocket *client, QObject *parent)
    : QObject(parent)
    , QRunnable()
    , m_wnd(mainWindow)
    , m_args(args)
    , m_client(client)
    , m_terminated(false)
{
    setAutoDelete(false);
}

void ScriptableWorker::run()
{
    if (!m_terminated) {
        if (m_client == NULL) {
            executeScript();
        } else {
            QByteArray response;
            int exitCode = executeScript(&response);

            if ( exitCode == CommandBadSyntax )
                response = tr("Bad command syntax. Use -h for help.\n").toLocal8Bit();
            emit sendMessage(m_client, response, exitCode);

            emit sendMessage(m_client, QByteArray(), CommandFinished);
        }
    }

    emit finished();
}

void ScriptableWorker::terminate()
{
    m_terminated = true;
    emit terminateScriptable();
}

void ScriptableWorker::onSendMessage(const QByteArray &message, int exitCode)
{
    if (m_client != NULL)
        emit sendMessage(m_client, message, exitCode);
    else
        QApplication::exit(0);
}

CommandStatus ScriptableWorker::executeScript(QByteArray *response)
{
#ifdef COPYQ_LOG_DEBUG
        const QString msg = QString("Starting scripting engine: %2");
#endif

    COPYQ_LOG( msg.arg("starting") );

    if ( m_args.length() <= Arguments::Rest ) {
        COPYQ_LOG( msg.arg("bad command syntax") );
        return CommandBadSyntax;
    }
    const QString cmd = QString::fromUtf8( m_args.at(Arguments::Rest) );

    QScriptEngine engine;
    ScriptableProxy proxy(m_wnd);
    Scriptable scriptable(&proxy);
    scriptable.abort();
    scriptable.initEngine( &engine, QString::fromUtf8(m_args.at(Arguments::CurrentPath)) );
    if (m_client != NULL) {
        connect(&scriptable, SIGNAL(sendMessage(QByteArray,int)),
                SLOT(onSendMessage(QByteArray,int)));
    }

    connect( this, SIGNAL(terminateScriptable()),
             &scriptable, SLOT(abort()) );

    QScriptValue result;
    QScriptValueList fnArgs;

    QScriptValue fn = engine.globalObject().property(cmd);
    if ( !fn.isFunction() ) {
        COPYQ_LOG( msg.arg("unknown command") );
        return CommandBadSyntax;
    }

    for ( int i = Arguments::Rest + 1; i < m_args.length(); ++i )
        fnArgs.append( scriptable.newByteArray(m_args.at(i)) );

    result = fn.call(QScriptValue(), fnArgs);

    if ( engine.hasUncaughtException() ) {
        COPYQ_LOG( msg.arg("command error (\"%1\")").arg(cmd) );
        if (response != NULL)
            response->append(engine.uncaughtException().toString() + '\n');
        engine.clearExceptions();
        return CommandError;
    }

    QByteArray *bytes = qscriptvalue_cast<QByteArray*>(result.data());
    if (response != NULL) {
        if (bytes != NULL)
            response->append(*bytes);
        else if (!result.isUndefined())
            response->append(result.toString() + '\n');
    }

    COPYQ_LOG( msg.arg("finished") );

    return CommandSuccess;
}
