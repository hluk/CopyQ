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

#ifndef SCRIPTABLEWORKER_H
#define SCRIPTABLEWORKER_H

#include "common/arguments.h"
#include "common/common.h"
#include "gui/mainwindow.h"
#include "scriptable/scriptable.h"
#include "scriptable/scriptableproxy.h"

#include <QRunnable>

class ClientSocket;

class ScriptableWorker : public QRunnable
{
public:
    ScriptableWorker(
            MainWindow *mainWindow, const Arguments &args, ClientSocket *socket,
            const QString &pluginScript);

    void run();

private:
    MainWindow *m_wnd;
    Arguments m_args;
    ClientSocket *m_socket;
    QString m_pluginScript;
    QString m_id;
};

#endif // SCRIPTABLEWORKER_H
