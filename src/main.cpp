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

#include "mainwindow.h"

#include "client_server.h"
#include "clipboardserver.h"
#include "clipboardclient.h"
#include "clipboardmonitor.h"
#include "scriptable.h"

#include <QScriptEngine>
#include <QTextStream>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

void evaluate(const QString &functionName, const char *arg)
{
    int argc = 0;
    char *argv[] = {NULL};
    App app(argc, argv);

    Scriptable scriptable(NULL, NULL);
    QScriptEngine engine;
    engine.setGlobalObject( engine.newQObject(&scriptable) );

    QScriptValue fn = engine.globalObject().property(functionName);
    QScriptValueList fnArgs;
    if (arg != NULL)
        fnArgs << QString(arg);

    QTextStream(stdout) << fn.call(QScriptValue(), fnArgs).toString();
}

int startServer(int argc, char *argv[])
{
    ClipboardServer app(argc, argv);
    if ( app.isListening() ) {
#ifdef Q_OS_WIN
    // FIXME: console window is still shown for a moment
    FreeConsole();
#endif
        return app.exec();
    } else {
        log( QObject::tr("CopyQ server is already running."), LogWarning );
        return 0;
    }
}

int startMonitor(int argc, char *argv[])
{
    ClipboardMonitor app(argc, argv);
    return app.isConnected() ? app.exec() : 0;
}

int startClient(int argc, char *argv[])
{
    ClipboardClient app(argc, argv);
    return app.exec();
}

bool needsHelp(const char *arg)
{
    return strcmp("-h",arg) == 0 ||
           strcmp("--help",arg) == 0 ||
           strcmp("help",arg) == 0;
}

bool needsVersion(const char *arg)
{
    return strcmp("-v",arg) == 0 ||
           strcmp("--version",arg) == 0 ||
           strcmp("version",arg) == 0;
}

} // namespace

int main(int argc, char *argv[])
{
    // print version
    if (argc == 2 || argc == 3) {
        const char *arg = argv[1];
        if ( argc == 2 && needsVersion(arg) ) {
            evaluate("version", NULL);
            return 0;
        } else if ( needsHelp(arg) ) {
            evaluate("help", argc == 3 ? argv[2] : NULL);
            return 0;
        }
    }

    if (argc == 1) {
        // if server hasn't been run yet and no argument were specified
        // then run this as server
        return startServer(argc, argv);
    } else if (argc == 2 && strcmp(argv[1], "monitor") == 0) {
        // if argument specified and server is running
        // then run this as client
        return startMonitor(argc, argv);
    } else {
        // if argument specified and server is running
        // then run this as client
        return startClient(argc, argv);
    }
}
