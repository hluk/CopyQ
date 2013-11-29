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

#include "app/app.h"
#include "app/clipboardclient.h"
#include "app/clipboardmonitor.h"
#include "app/clipboardserver.h"
#include "common/client_server.h"
#include "gui/mainwindow.h"
#include "scriptable/scriptable.h"

#include <cstdio>
#include <QCoreApplication>
#include <QFile>
#include <QScriptEngine>

#ifdef Q_OS_MAC
#  include "platform/mac/macactivity.h"
#endif

#ifdef HAS_TESTS
#  include "tests/tests.h"
#  include <QTest>

namespace tests {

QTEST_MAIN(Tests)

}
#endif // HAS_TESTS

Q_DECLARE_METATYPE(QByteArray*)

namespace {

void evaluate(const QString &functionName, const char *arg)
{
    int argc = 0;
    char *argv[] = {NULL};
    App app(new QCoreApplication(argc, argv));

    Scriptable scriptable(NULL, NULL);
    QScriptEngine engine;
    engine.setGlobalObject( engine.newQObject(&scriptable) );

    QScriptValue fn = engine.globalObject().property(functionName);
    QScriptValueList fnArgs;
    if (arg != NULL)
        fnArgs << QString(arg);

    QScriptValue result = fn.call(QScriptValue(), fnArgs);

    QFile f;
    f.open(stdout, QIODevice::WriteOnly);
    f.write( result.toString().toStdString().data() );
}

int startServer(int argc, char *argv[], const QString &sessionName)
{
#ifdef Q_OS_MAC
    MacActivity activity(MacActivity::Background, "CopyQ Server");
#endif
    ClipboardServer app(argc, argv, sessionName);
    if ( app.isListening() ) {
        return app.exec();
    } else {
        log( QObject::tr("CopyQ server is already running."), LogWarning );
        return 0;
    }
}

int startMonitor(int argc, char *argv[])
{
#ifdef Q_OS_MAC
    MacActivity activity(MacActivity::Background, "CopyQ clipboard monitor");
#endif
    ClipboardMonitor app(argc, argv);
    return app.isConnected() ? app.exec() : 0;
}

int startClient(int argc, char *argv[], int skipArgc, const QString &sessionName)
{
#ifdef Q_OS_MAC
    MacActivity activity(MacActivity::User, "CopyQ Client");
#endif
    ClipboardClient app(argc, argv, skipArgc, sessionName);
    return app.exec();
}

bool needsHelp(const QByteArray &arg)
{
    return arg == "-h" ||
           arg == "--help" ||
           arg == "help";
}

bool needsVersion(const QByteArray &arg)
{
    return arg == "-v" ||
           arg == "--version" ||
           arg == "version";
}

#ifdef HAS_TESTS
bool needsTests(const QByteArray &arg)
{
    return arg == "--tests" ||
           arg == "tests";
}
#endif

QString getSessionName(int *argc, char *argv[])
{
    if (*argc <= 1)
        return QString("");

    QString sessionName;
    const QString arg1(argv[1]);

    if (arg1 == "-s" || arg1 == "--session" || arg1 == "session") {
        if (*argc > 2) {
            *argc -= 2;
            sessionName = argv[2];
        }
    } else if (arg1.startsWith("--session=")) {
        sessionName = arg1.mid( arg1.indexOf('=') + 1 );
        *argc -= 1;
    } else {
        return QString("");
    }

    // check session name
    bool ok = !sessionName.isNull() && sessionName.length() < 16;
    if (ok) {
        foreach (const QChar &c, sessionName) {
            ok = c.isLetterOrNumber() || c == '-' || c == '_';
            if (!ok)
                break;
        }
    }

    if (!ok) {
        log( QObject::tr("Session name must contain at most 16 characters\n"
                         "which can be letters, digits, '-' or '_'!"), LogWarning );
    }

    return ok ? sessionName : QString();
}

} // namespace

int main(int argc, char *argv[])
{
    // print version, help or run tests
    if (argc == 2 || argc == 3) {
        const QByteArray arg(argv[1]);
        if ( argc == 2 && needsVersion(arg) ) {
            evaluate("version", NULL);
            return 0;
        } else if ( needsHelp(arg) ) {
            evaluate("help", argc == 3 ? argv[2] : NULL);
            return 0;
#ifdef HAS_TESTS
        } else if ( needsTests(arg) ) {
            // Skip the "tests" argument and pass the rest to tests.
            return tests::main(argc - 1, argv + 1);
#endif
        }
    }

    // get session name (default empty)
    QString sessionName;
    int newArgc = argc;
    if (argc > 1) {
        sessionName = getSessionName(&newArgc, argv);
        if ( sessionName.isNull() )
            return 2;
    }

    if (newArgc == 1) {
        // if server hasn't been run yet and no argument were specified
        // then run this process as server
        return startServer(argc, argv, sessionName);
    } else if (argc == 3 && strcmp(argv[1], "monitor") == 0) {
        // if first argument is monitor (second is monitor server name/ID)
        // then run this process as monitor
        return startMonitor(argc, argv);
    } else {
        // if argument specified and server is running
        // then run this process as client
        return startClient(argc, argv, argc - newArgc, sessionName);
    }
}
