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
#include "../qt/bytearrayclass.h"

#include <QApplication>
#include <QObject>
#include <QScriptEngine>

Q_DECLARE_METATYPE(QByteArray*)

ScriptableWorker::ScriptableWorker(
        MainWindow *mainWindow, ClientSocket *socket, const QString &pluginScript)
    : QRunnable()
    , m_wnd(mainWindow)
    , m_socket(socket)
    , m_pluginScript(pluginScript)
{
}

void ScriptableWorker::run()
{
    setCurrentThreadName("Script-" + QString::number(m_socket->id()));

    QScriptEngine engine;
    ScriptableProxy proxy(m_wnd);
    Scriptable scriptable(&proxy, m_pluginScript);
    scriptable.initEngine(&engine);

    if (m_socket) {
        QObject::connect( &proxy, SIGNAL(sendMessage(QByteArray,int)),
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
            COPYQ_LOG("TERMINATED");
            return;
        }

        QMetaObject::invokeMethod(m_socket, "start", Qt::QueuedConnection);
    }

    QObject::connect( &scriptable, SIGNAL(requestApplicationQuit()),
                      qApp, SLOT(quit()) );

    while (!scriptable.isAborted())
        QCoreApplication::processEvents();
}
