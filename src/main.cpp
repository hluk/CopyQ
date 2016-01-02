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

#include "app/app.h"
#include "app/clipboardclient.h"
#include "app/clipboardmonitor.h"
#include "app/clipboardserver.h"
#include "common/common.h"
#include "common/log.h"
#include "platform/platformnativeinterface.h"
#include "scriptable/scriptable.h"

#include <QCoreApplication>
#include <QFile>
#include <QScriptEngine>

#ifdef HAS_TESTS
#  include "tests/tests.h"
#endif // HAS_TESTS

Q_DECLARE_METATYPE(QByteArray*)

namespace {

int evaluate(const QString &functionName, const QStringList &arguments, int argc, char **argv)
{
    App app( new QCoreApplication(argc, argv) );

    Scriptable scriptable(NULL, NULL);
    QScriptEngine engine;
    engine.setGlobalObject( engine.newQObject(&scriptable) );

    QScriptValue function = engine.globalObject().property(functionName);
    QScriptValueList functionArguments;

    foreach (const QString &argument, arguments)
        functionArguments.append(argument);

    const QScriptValue result = function.call( QScriptValue(), functionArguments );

    QFile f;
    f.open(stdout, QIODevice::WriteOnly);
    f.write( scriptable.fromString(result.toString()) );

    return engine.hasUncaughtException() ? 1 : 0;
}

int startServer(int argc, char *argv[], const QString &sessionName)
{
    ClipboardServer app(argc, argv, sessionName);
    return app.exec();
}

int startMonitor(int argc, char *argv[])
{
    ClipboardMonitor app(argc, argv);
    return app.exec();
}

int startClient(int argc, char *argv[], int skipArguments, const QString &sessionName)
{
    ClipboardClient app(argc, argv, skipArguments, sessionName);
    return app.exec();
}

bool needsHelp(const QString &arg)
{
    return arg == "-h" ||
           arg == "--help" ||
           arg == "help";
}

bool needsVersion(const QString &arg)
{
    return arg == "-v" ||
           arg == "--version" ||
           arg == "version";
}

#ifdef HAS_TESTS
bool needsTests(const QString &arg)
{
    return arg == "--tests" ||
           arg == "tests";
}
#endif

bool containsOnlyValidCharacters(const QString &sessionName)
{
    foreach (const QChar &c, sessionName) {
        if ( !c.isLetterOrNumber() && c != '-' && c != '_' )
            return false;
    }

    return true;
}

bool isValidSessionName(const QString &sessionName)
{
    return !sessionName.isNull() &&
           sessionName.length() < 16 &&
           containsOnlyValidCharacters(sessionName);
}

QString getSessionName(const QStringList &arguments, int *skipArguments)
{
    const QString firstArgument = arguments.value(0);
    *skipArguments = 0;

    if (firstArgument == "-s" || firstArgument == "--session" || firstArgument == "session") {
        *skipArguments = 2;
        return arguments.value(1);
    }

    if ( firstArgument.startsWith("--session=") ) {
        *skipArguments = 1;
        return firstArgument.mid( firstArgument.indexOf('=') + 1 );
    }

    return getTextData( qgetenv("COPYQ_SESSION_NAME") );
}

} // namespace

int main(int argc, char **argv)
{
    const QStringList arguments =
            createPlatformNativeInterface()->getCommandLineArguments(argc, argv);

    // Print version, help or run tests.
    if ( !arguments.isEmpty() ) {
        if ( needsVersion(arguments.first()) )
            return evaluate( "version", QStringList(), argc, argv );

        if ( needsHelp(arguments.first()) )
            return evaluate( "help", arguments.mid(1), argc, argv );

#ifdef HAS_TESTS
        if ( needsTests(arguments.first()) ) {
            // Skip the "tests" argument and pass the rest to tests.
            return runTests(argc - 1, argv + 1);
        }
#endif
    }

    // Get session name (default is empty).
    int skipArguments;
    const QString sessionName = getSessionName(arguments, &skipArguments);

    if ( !isValidSessionName(sessionName) ) {
        log( QObject::tr("Session name must contain at most 16 characters\n"
                         "which can be letters, digits, '-' or '_'!"), LogError );
        return 2;
    }

    // If server hasn't been run yet and no argument were specified
    // then run this process as server.
    if ( arguments.size() - skipArguments == 0 )
        return startServer(argc, argv, sessionName);

    // If first argument is "monitor" (second is monitor server name/ID)
    // then run this process as clipboard monitor.
    if ( arguments.size() == 2 && arguments[0] == "monitor" )
        return startMonitor(argc, argv);

    // If argument was specified and server is running
    // then run this process as client.
    return startClient(argc, argv, skipArguments, sessionName);
}
