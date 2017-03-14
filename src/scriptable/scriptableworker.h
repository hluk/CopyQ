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

#ifndef SCRIPTABLEWORKER_H
#define SCRIPTABLEWORKER_H

#include "common/common.h"
#include "gui/mainwindow.h"
#include "scriptable/scriptable.h"
#include "scriptable/scriptableproxy.h"

#include <QObject>
#include <QRunnable>

class ClientSocket;
using ClientSocketPtr = std::shared_ptr<ClientSocket>;

class ItemScriptable;

/**
 * Handles socket destruction.
 *
 * ScriptableWorker destructor is called in different thread than
 * the socket's thread.
 *
 * When ScriptableWorker is finished, it invokes deleteLater()
 * of the guard so the socket is deleted in correct thread.
 */
class ScriptableWorkerSocketGuard : public QObject {
public:
    explicit ScriptableWorkerSocketGuard(const ClientSocketPtr &socket);

    ~ScriptableWorkerSocketGuard();

    ClientSocket *socket() const;

private:
    ClientSocketPtr m_socket;
};

class ScriptableWorker : public QRunnable
{
public:
    ScriptableWorker(
            MainWindow *mainWindow,
            const ClientSocketPtr &socket,
            const QList<ItemScriptable*> &scriptables);

    ~ScriptableWorker();

    void run() override;

private:
    MainWindow *m_wnd;
    QPointer<ScriptableWorkerSocketGuard> m_socketGuard;
    QList<ItemScriptable*> m_scriptables;
};

#endif // SCRIPTABLEWORKER_H
