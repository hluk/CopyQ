// SPDX-License-Identifier: GPL-3.0-or-later

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
#include <QJSEngine>
#include <QProcess>
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

    QJSEngine engine;
    Scriptable scriptable(&engine, nullptr, nullptr);

    QJSValue function = engine.globalObject().property(functionName);
    QJSValueList functionArguments;

    functionArguments.reserve( arguments.size() );
    for (const auto &argument : arguments)
        functionArguments.append(argument);

    const auto result = function.call(functionArguments);
    const bool hasUncaughtException = result.isError() || scriptable.hasUncaughtException();

    const auto output = scriptable.fromString(result.toString());
    if ( !output.isEmpty() && canUseStandardOutput() ) {
        QFile f;
        if (hasUncaughtException)
            f.open(stderr, QIODevice::WriteOnly);
        else
            f.open(stdout, QIODevice::WriteOnly);

        f.write(output);
        if ( !output.endsWith("\n") )
            f.write("\n");
        f.close();
    }

    const int exitCode = hasUncaughtException ? CommandException : 0;
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
    const QSettings settings(
        QSettings::IniFormat,
        QSettings::UserScope,
        QStringLiteral("copyq"),
        QStringLiteral("copyq_no_session"));
    const auto sessionNameKey = QLatin1String("session_") + sessionId;
    const auto sessionName = settings.value(sessionNameKey).toString();
    return sessionName;
}

int startServer(int argc, char *argv[], QString sessionName)
{
    // By default, enable automatic screen scaling in Qt for high-DPI displays
    // (this works better at least in Windows).
    if ( qEnvironmentVariableIsEmpty("QT_AUTO_SCREEN_SCALE_FACTOR") )
        qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", QByteArrayLiteral("1"));

    auto qapp = platformNativeInterface()->createServerApplication(argc, argv);
    if ( qapp->isSessionRestored() ) {
        const auto sessionId = qapp->sessionId();
        sessionName = restoreSessionName(sessionId);
        COPYQ_LOG( QStringLiteral("Restoring session ID \"%1\", session name \"%2\"")
                   .arg(sessionId, sessionName) );
        if ( !sessionName.isEmpty() && !isValidSessionName(sessionName) ) {
            log("Failed to restore session name", LogError);
            return 2;
        }
    }

    ClipboardServer app(qapp, sessionName);
    return app.exec();
}

void startServerInBackground(const QString &applicationPath, QString sessionName)
{
    const bool couldUseStandardOutput = canUseStandardOutput();
    if (couldUseStandardOutput)
        qputenv("COPYQ_NO_OUTPUT", QByteArrayLiteral("1"));

    const QStringList arguments{QStringLiteral("-s"), sessionName};
    const bool started = QProcess::startDetached(applicationPath, arguments);

    if (!couldUseStandardOutput)
        qunsetenv("COPYQ_NO_OUTPUT");

    if (!started)
        log("Failed to start the server", LogError);
}

int startClient(int argc, char *argv[], const QStringList &arguments, const QString &sessionName)
{
    ClipboardClient app(argc, argv, arguments, sessionName);
    return app.exec();
}

bool needsHelp(const QString &arg)
{
    return arg == QLatin1String("-h") ||
           arg == QLatin1String("--help") ||
           arg == QLatin1String("help");
}

bool needsVersion(const QString &arg)
{
    return arg == QLatin1String("-v") ||
           arg == QLatin1String("--version") ||
           arg == QLatin1String("version");
}

bool needsInfo(const QString &arg)
{
    return arg == QLatin1String("--info") ||
           arg == QLatin1String("info");
}

bool needsLogs(const QString &arg)
{
    return arg == QLatin1String("--logs") ||
           arg == QLatin1String("logs");
}

bool needsStartServer(const QString &arg)
{
    return arg == QLatin1String("--start-server");
}

#ifdef HAS_TESTS
bool needsTests(const QString &arg)
{
    return arg == QLatin1String("--tests") ||
           arg == QLatin1String("tests");
}
#endif

QString getSessionName(const QStringList &arguments, int *skipArguments)
{
    if (arguments.size() > 0) {
        if ( arguments[0] == QLatin1String("-s")
          || arguments[0] == QLatin1String("--session")
          || arguments[0] == QLatin1String("session") )
        {
            *skipArguments = 2;
            return arguments.value(1);
        }

        if ( arguments[0].startsWith(QLatin1String("--session=")) ) {
            *skipArguments = 1;
            return arguments[0].mid( arguments[0].indexOf('=') + 1 );
        }

        // Skip session arguments passed from session manager.
        if (arguments.size() == 2 && arguments[0] == QLatin1String("-session"))
            *skipArguments = 2;
    }

    return getTextData( qgetenv("COPYQ_SESSION_NAME") );
}

int startApplication(int argc, char **argv)
{
    installMessageHandlerForQt();

#ifdef Q_OS_UNIX
    if ( !initUnixSignalHandler() )
        log("Failed to create handler for Unix signals!", LogError);
#endif

    const QStringList arguments =
            platformNativeInterface()->getCommandLineArguments(argc, argv);

    // Get session name (default is empty).
    int skipArguments = 0;
    const QString sessionName = getSessionName(arguments, &skipArguments);

    if ( !isValidSessionName(sessionName) ) {
        log( QObject::tr("Session name must contain at most 16 characters\n"
                         "which can be letters, digits, '-' or '_'!"), LogError );
        return 2;
    }

    // Print version, help or run tests.
    if ( arguments.size() > skipArguments ) {
        const auto arg = arguments[skipArguments];

        if ( needsStartServer(arg) ) {
            startServerInBackground( QString::fromUtf8(argv[0]), sessionName );
            return skipArguments + 1 == arguments.size() ? 0
                : startClient(argc, argv, arguments.mid(skipArguments + 1), sessionName);
        }

        if ( needsVersion(arg) )
            return evaluate( QStringLiteral("version"), QStringList(), argc, argv, sessionName );

        if ( needsHelp(arg) )
            return evaluate( QStringLiteral("help"), arguments.mid(skipArguments + 1), argc, argv, sessionName );

        if ( needsInfo(arg) )
            return evaluate( QStringLiteral("info"), arguments.mid(skipArguments + 1), argc, argv, sessionName );

        if ( needsLogs(arg) )
            return evaluate( QStringLiteral("logs"), arguments.mid(skipArguments + 1), argc, argv, sessionName );

#ifdef HAS_TESTS
        if ( needsTests(arg) ) {
            // Skip the "tests" argument and pass the rest to tests.
            return runTests(argc - skipArguments - 1, argv + skipArguments + 1);
        }
#endif
    }

    // If server hasn't been run yet and no argument were specified
    // then run this process as server.
    if ( skipArguments == arguments.size() )
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
