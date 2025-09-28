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

const QString defaultSessionName = QStringLiteral("copyq");

enum class AppType {
    Version,
    Help,
    Info,
    Logs,
    Tests,
    Server,
    StartServerInBackground,
    Client
};

struct AppArguments {
    AppType appType;
    QString sessionName;
    QStringList arguments;
};

int evaluate(
        const QString &functionName,
        const QStringList &arguments, int argc, char **argv,
        const QString &sessionName)
{
    App app( platformNativeInterface()->createConsoleApplication(argc, argv), sessionName );

    QJSEngine engine;
    Scriptable scriptable(&engine, nullptr, nullptr);

    QJSValue function = engine.globalObject().property(functionName);
    QJSValueList functionArguments;

    functionArguments.reserve( arguments.size() );
    for (const auto &argument : arguments)
        functionArguments.append(argument);

    const auto result = function.call(functionArguments);
    const bool hasUncaughtException = result.isError() || scriptable.hasUncaughtException();

    const auto output = scriptable.serializeScriptValue(result);
    if ( !output.isEmpty() ) {
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
    const QStringList arguments{QStringLiteral("-s"), sessionName};
    const bool started = QProcess::startDetached(applicationPath, arguments);

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

QString takeSessionName(QStringList &arguments)
{
    if (arguments.size() > 0) {
        if ( arguments[0] == QLatin1String("-s")
          || arguments[0] == QLatin1String("--session")
          || arguments[0] == QLatin1String("session") )
        {
            arguments.removeAt(0);
            return arguments.takeAt(0);
        }

        if ( arguments[0].startsWith(QLatin1String("--session=")) ) {
            const QString sessionArg = arguments.takeAt(0);
            return sessionArg.mid(std::char_traits<char>::length("--session="));
        }

        // Skip session arguments passed from session manager.
        if (arguments.size() == 2 && arguments[0] == QLatin1String("-session"))
            arguments.clear();
    }

    return getTextData( qgetenv("COPYQ_SESSION_NAME") );
}

AppArguments parseArguments(int argc, char **argv)
{
    QStringList arguments =
        platformNativeInterface()->getCommandLineArguments(argc, argv);

#ifdef HAS_TESTS
    if ( !arguments.isEmpty() && needsTests(arguments[0]) )
        return {AppType::Tests, QString(""), {}};
#endif

    const QString sessionName = takeSessionName(arguments);

    if (arguments.isEmpty())
        return {AppType::Server, sessionName, {}};

    const QString &arg = arguments[0];

    if ( needsStartServer(arg) )
        return {AppType::StartServerInBackground, sessionName, arguments.mid(1)};

    if ( needsVersion(arg) )
        return {AppType::Version, sessionName, arguments.mid(1)};

    if ( needsHelp(arg) )
        return {AppType::Help, sessionName, arguments.mid(1)};

    if ( needsInfo(arg) )
        return {AppType::Info, sessionName, arguments.mid(1)};

    if ( needsLogs(arg) )
        return {AppType::Logs, sessionName, arguments.mid(1)};

    // If argument was specified and server is running
    // then run this process as client.
    return {AppType::Client, sessionName, arguments};
}

/// Return log label for client based on the command called if available
/// Examples:
/// - cmd/monitorClipboard
/// - cmd/synchronizeToSelection
/// - cmd/copy
QByteArray logLabelForClient(const QStringList &arguments)
{
    int i = 0;
    for (; i < arguments.size(); ++i) {
        const auto &arg = arguments[i];
        if (arg == QLatin1String("tab") || arg == QLatin1String("separator")) {
            ++i;
            break;
        }
        if (!arg.startsWith('-') && arg != QLatin1String("eval"))
            break;
    }
    if (i < arguments.size()) {
        const auto &command = arguments[i];
        for (const auto &c : command) {
            if ( !c.isLetterOrNumber() && c != '_' )
                return "Client";
        }
        return "cmd/" + command.toUtf8();
    }
    return "Client";
}

QByteArray logLabelForType(AppType appType, const QStringList &arguments)
{
    switch (appType) {
    case AppType::Server:
        return "Server";
    case AppType::Client:
        return logLabelForClient(arguments);
    case AppType::StartServerInBackground:
        return logLabelForClient(arguments);
    default:
        return "Prompt";
    }
}

void setSessionName(const QString &sessionName)
{
    QCoreApplication::setOrganizationName(sessionName);
    QCoreApplication::setApplicationName(sessionName);
}

int startApplication(int argc, char **argv)
{
    setSessionName(defaultSessionName);

    const AppArguments args = parseArguments(argc, argv);

    setLogLabel( logLabelForType(args.appType, args.arguments) );
    installMessageHandlerForQt();

#ifdef Q_OS_UNIX
    if ( !initUnixSignalHandler() )
        log("Failed to create handler for Unix signals!", LogError);
#endif

    if ( !isValidSessionName(args.sessionName) ) {
        log( QObject::tr("Session name must contain at most 16 characters\n"
                         "which can be letters, digits, '-' or '_'!"), LogError );
        return 2;
    }

    if ( args.sessionName.isEmpty() ) {
        setSessionName(defaultSessionName);
    } else {
        const QString session = QStringLiteral("%1-%2")
            .arg(defaultSessionName, args.sessionName);
        setSessionName(session);
    }

    switch (args.appType) {
    // If server hasn't been run yet and no argument were specified
    // then run this process as server.
    case AppType::Server:
        return startServer(argc, argv, args.sessionName);

    // If argument was specified and server is running
    // then run this process as client.
    case AppType::Client:
        return startClient(argc, argv, args.arguments, args.sessionName);

    case AppType::StartServerInBackground:
        startServerInBackground( QString::fromUtf8(argv[0]), args.sessionName );
        if (args.arguments.isEmpty())
            return 0;
        return startClient(argc, argv, args.arguments, args.sessionName);

    case AppType::Version:
        return evaluate( QStringLiteral("version"), {}, argc, argv, args.sessionName );

    case AppType::Help:
        return evaluate( QStringLiteral("help"), args.arguments, argc, argv, args.sessionName );

    case AppType::Info:
        return evaluate( QStringLiteral("info"), args.arguments, argc, argv, args.sessionName );

    case AppType::Logs:
        return evaluate( QStringLiteral("logs"), args.arguments, argc, argv, args.sessionName );

#ifdef HAS_TESTS
    case AppType::Tests:
        return runTests(argc - 1, argv + 1);
#endif
    }
    return -1;
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
