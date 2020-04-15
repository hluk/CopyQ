/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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
#include "app/applicationexceptionhandler.h"
#include "app/clipboardclient.h"
#include "app/clipboardserver.h"
#include "common/commandstatus.h"
#include "common/log.h"
#include "common/messagehandlerforqt.h"
#include "common/textdata.h"
#include "platform/platformnativeinterface.h"
#ifdef Q_OS_UNIX
#   include "platform/unix/unixsignalhandler.h"
#endif
#include "scriptable/scriptable.h"

#include <QApplication>
#include <QFile>
#include <QScriptEngine>
#include <QSettings>

#ifdef HAS_TESTS
#  include "tests/tests.h"
#endif // HAS_TESTS

#include <exception>

Q_DECLARE_METATYPE(QByteArray*)

namespace {

int evaluate(
        const QString &functionName,
        const QStringList &arguments, int argc, char **argv,
        const QString &sessionName)
{
    App app( platformNativeInterface()->createConsoleApplication(argc, argv), sessionName );
    setLogLabel("Prompt");

    QScriptEngine engine;
    Scriptable scriptable(&engine, nullptr);

    QScriptValue function = engine.globalObject().property(functionName);
    QScriptValueList functionArguments;

    functionArguments.reserve( arguments.size() );
    for (const auto &argument : arguments)
        functionArguments.append(argument);

    const QScriptValue result = function.call( QScriptValue(), functionArguments );

    const auto output = scriptable.fromString(result.toString());
    if ( !output.isEmpty() ) {
        QFile f;
        if ( engine.hasUncaughtException() )
            f.open(stderr, QIODevice::WriteOnly);
        else
            f.open(stdout, QIODevice::WriteOnly);

        f.write(output);
        if ( !output.endsWith("\n") )
            f.write("\n");
        f.close();
    }

    const int exitCode = engine.hasUncaughtException() ? CommandException : 0;
    app.exit(exitCode);
    return exitCode;
}

bool containsOnlyValidCharacters(const QString &sessionName)
{
    for (const auto &c : sessionName) {
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

QString restoreSessionName(const QString &sessionId)
{
    const QSettings settings(QSettings::IniFormat, QSettings::UserScope, "copyq", "copyq_no_session");
    const auto sessionNameKey = "session_" + sessionId;
    const auto sessionName = settings.value(sessionNameKey).toString();
    return sessionName;
}

int startServer(int argc, char *argv[], QString sessionName)
{
    // By default, enable automatic screen scaling in Qt for high-DPI displays
    // (this works better at least in Windows).
    if ( qEnvironmentVariableIsEmpty("QT_AUTO_SCREEN_SCALE_FACTOR") )
        qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "1");

    auto qapp = platformNativeInterface()->createServerApplication(argc, argv);
    if ( qapp->isSessionRestored() ) {
        const auto sessionId = qapp->sessionId();
        sessionName = restoreSessionName(sessionId);
        COPYQ_LOG( QString("Restoring session ID \"%1\", session name \"%2\"")
                   .arg(sessionId, sessionName) );
        if ( !sessionName.isEmpty() && !isValidSessionName(sessionName) ) {
            log("Failed to restore session name", LogError);
            return 2;
        }
    }

    ClipboardServer app(qapp, sessionName);
    return app.exec();
}

int startClient(int argc, char *argv[], const QStringList &arguments, const QString &sessionName)
{
    ClipboardClient app(argc, argv, arguments, sessionName);
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

bool needsInfo(const QString &arg)
{
    return arg == "--info" ||
           arg == "info";
}

bool needsLogs(const QString &arg)
{
    return arg == "--logs" ||
           arg == "logs";
}

#ifdef HAS_TESTS
bool needsTests(const QString &arg)
{
    return arg == "--tests" ||
           arg == "tests";
}
#endif

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

    // Skip session arguments passed from session manager.
    if (arguments.size() == 2 && firstArgument == "-session")
        *skipArguments = 2;

    return getTextData( qgetenv("COPYQ_SESSION_NAME") );
}

int startApplication(int argc, char **argv)
{
    installMessageHandlerForQt();

#ifdef Q_OS_UNIX
    if ( !initUnixSignalHandler() )
        log( QString("Failed to create handler for Unix signals!"), LogError );
#endif

    const QStringList arguments =
            platformNativeInterface()->getCommandLineArguments(argc, argv);

    // Get session name (default is empty).
    int skipArguments;
    const QString sessionName = getSessionName(arguments, &skipArguments);

    if ( !isValidSessionName(sessionName) ) {
        log( QObject::tr("Session name must contain at most 16 characters\n"
                         "which can be letters, digits, '-' or '_'!"), LogError );
        return 2;
    }

    // Print version, help or run tests.
    if ( arguments.size() > skipArguments ) {
        const auto arg = arguments[skipArguments];

        if ( needsVersion(arg) )
            return evaluate( "version", QStringList(), argc, argv, sessionName );

        if ( needsHelp(arg) )
            return evaluate( "help", arguments.mid(skipArguments + 1), argc, argv, sessionName );

        if ( needsInfo(arg) )
            return evaluate( "info", arguments.mid(skipArguments + 1), argc, argv, sessionName );

        if ( needsLogs(arg) )
            return evaluate( "logs", arguments.mid(skipArguments + 1), argc, argv, sessionName );

#ifdef HAS_TESTS
        if ( needsTests(arg) ) {
            // Skip the "tests" argument and pass the rest to tests.
            return runTests(argc - skipArguments - 1, argv + skipArguments + 1);
        }
#endif
    }

    // If server hasn't been run yet and no argument were specified
    // then run this process as server.
    if ( arguments.size() - skipArguments == 0 )
        return startServer(argc, argv, sessionName);

    // If argument was specified and server is running
    // then run this process as client.
    return startClient(argc, argv, arguments.mid(skipArguments), sessionName);
}

} // namespace

int main(int argc, char **argv)
{
    try {
        return startApplication(argc, argv);
    } catch (const std::exception &e) {
        logException(e.what());
        throw;
    } catch (...) {
        logException();
        throw;
    }
}
