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

#ifndef SCRIPTABLEWORKER_H
#define SCRIPTABLEWORKER_H

#include "common/arguments.h"
#include "common/common.h"

#include <QObject>
#include <QRunnable>
#include <QSharedPointer>

class MainWindow;
class QLocalSocket;

class ScriptableWorker : public QObject, public QRunnable
{
    Q_OBJECT
public:
    ScriptableWorker(const QSharedPointer<MainWindow> &mainWindow, const Arguments &args,
                     QLocalSocket *client, QObject *parent = NULL);

public slots:
    void run();
    void terminate();

signals:
    void sendMessage(QLocalSocket *client, const QByteArray &message, int exitCode);
    void finished();
    void terminateScriptable();
    void messageReceived(const QByteArray &bytes);

private slots:
    void onSendMessage(const QByteArray &message, int exitCode);
    void onReadyRead();

private:
    CommandStatus executeScript(QByteArray *response = NULL);

    QSharedPointer<MainWindow> m_wnd;
    Arguments m_args;
    QLocalSocket *m_client;
    bool m_terminated;
};

#endif // SCRIPTABLEWORKER_H
