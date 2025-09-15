// SPDX-License-Identifier: GPL-3.0-or-later

#include "tests.h"
#include "test_utils.h"
#include "tests_common.h"

#include "common/action.h"
#include "common/appconfig.h"
#include "common/client_server.h"
#include "common/common.h"
#include "common/config.h"
#include "common/log.h"
#include "common/settings.h"
#include "common/shortcuts.h"
#include "common/sleeptimer.h"
#include "common/textdata.h"
#include "item/itemfactory.h"
#include "item/itemwidget.h"
#include "item/serialize.h"
#include "gui/tabicons.h"
#include "platform/platformclipboard.h"
#include "platform/platformnativeinterface.h"

#include <QClipboard>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMap>
#include <QProcess>
#include <QRegularExpression>
#include <QTest>
#include <QTimer>

#include <memory>

namespace {

const QString defaultTestId = QStringLiteral("CORE");
const QString defaultTestPlugins = QStringLiteral("itemtext,itemnotes");

class PerformanceTimer final {
public:
    PerformanceTimer() {
        m_timer.start();
    }

    void printPerformance(const char *label, const QStringList &arguments = QStringList()) {
        const auto elapsedMs = m_timer.elapsed();
        if (elapsedMs > 500)
            qWarning() << "--- PERFORMANCE ---" << elapsedMs << "ms:" << label << arguments;
        m_timer.start();
    }

private:
    QElapsedTimer m_timer;
};

QByteArray decorateOutput(const QByteArray &label, const QByteArray &stderrOutput)
{
    QByteArray output = "\n" + stderrOutput;
    output.replace('\n', "\n    ");
    output.prepend("\n  " + label + ":");
    return output;
}

class TestInterfaceImpl final : public TestInterface {
public:
    TestInterfaceImpl()
        : m_server(nullptr)
        , m_env(QProcessEnvironment::systemEnvironment())
    {
        m_env.insert("COPYQ_LOG_LEVEL", "DEBUG");
        m_env.insert("COPYQ_SESSION_COLOR", defaultSessionColor);
        m_env.insert("COPYQ_SESSION_NAME", sessionName);
        m_env.insert("COPYQ_CLIPBOARD_COPY_TIMEOUT_MS", "2000");
    }

    ~TestInterfaceImpl()
    {
        stopServer();
    }

    QByteArray startServer() override
    {
        if ( isServerRunning() )
            return "Server is already running.";

        if ( !removeLogFiles() )
            return "Failed to remove log files";

        m_server.reset(new QProcess);
        if ( !startTestProcess(m_server.get(), QStringList(), QIODevice::NotOpen) ) {
            return QString::fromLatin1("Failed to launch \"%1\": %2")
                .arg(QCoreApplication::applicationFilePath())
                .arg(m_server->errorString())
                .toUtf8();
        }

        m_server->closeReadChannel(QProcess::StandardOutput);

        RETURN_ON_ERROR( readServerErrors(), "Failed to start server" );

        return waitForServerToStart();
    }

    QByteArray stopServer() override
    {
        if ( !isServerRunning() )
            return "Server is not running";

        QByteArray errors;
        const int exitCode = run(Args("exit"), nullptr, &errors);
        if ( !testStderr(errors) || exitCode != 0 ) {
            return "Command 'exit' failed."
                    + printClienAndServerStderr(errors, exitCode);
        }

        return waitForServerToStop();
    }

    QByteArray waitForServerToStop() override
    {
        PerformanceTimer perf;

        // Process events in case we own clipboard and the new process requests the contents.
        SleepTimer t(30000);
        while ( m_server->state() != QProcess::NotRunning && !m_server->waitForFinished(50) && t.sleep() ) {}

        perf.printPerformance("stopServer");

        if ( m_server->state() != QProcess::NotRunning ) {
            qWarning() << "terminating server process";
            m_server->terminate();

            if ( !m_server->waitForFinished() ) {
                qWarning() << "killing server process";
                terminateProcess(m_server.get());
            }

            return "Failed to close server properly!" + readServerErrors(ReadAllStderr);
        }

        return readServerErrors();
    }

    bool isServerRunning() override
    {
        return m_server != nullptr && m_server->state() == QProcess::Running;
    }

    int run(const QStringList &arguments, QByteArray *stdoutData = nullptr,
            QByteArray *stderrData = nullptr, const QByteArray &in = QByteArray(),
            const QStringList &environment = QStringList()) override
    {
        if (stdoutData != nullptr)
            stdoutData->clear();

        if (stderrData != nullptr)
            stderrData->clear();

        QProcess p;
        if (!startTestProcess(&p, arguments, QIODevice::ReadWrite, environment))
            return -1;

        if (in != KEEP_STDIN_OPEN) {
            if ( p.write(in) != in.size() )
                return -2;

            p.closeWriteChannel();
        }

        if (stdoutData == nullptr)
            p.closeReadChannel(QProcess::StandardOutput);

        if (stderrData == nullptr)
            p.closeReadChannel(QProcess::StandardError);

        PerformanceTimer perf;

        SleepTimer t(waitClientRun);
        while ( p.state() == QProcess::Running ) {
            if ( stdoutData != nullptr ) {
                const auto out = p.readAllStandardOutput();
                stdoutData->append(out);
            }

            if (stderrData != nullptr) {
                const auto err = p.readAllStandardError();
                stderrData->append(err);
            }

            if ( !t.sleep() ) {
                qWarning() << "Client process timed out" << arguments;
                return -1;
            }
        }

        if (stderrData != nullptr) {
            stderrData->append(p.readAllStandardError());
            stderrData->replace('\r', "");
        }

        if (stdoutData != nullptr) {
            stdoutData->append(p.readAllStandardOutput());
            stdoutData->replace('\r', "");
        }

        perf.printPerformance("run", arguments);

        return p.exitCode();
    }

    QByteArray printClienAndServerStderr(const QByteArray &clientStderr, int exitCode)
    {
        return "\n  Client exit code: " + QByteArray::number(exitCode) + "\n"
                + decorateOutput("Client STDERR", clientStderr)
                + readServerErrors(ReadAllStderr);
    }

    QByteArray runClient(const QStringList &arguments, const QByteArray &stdoutExpected,
                         const QByteArray &input = QByteArray()) override
    {
        if (!isServerRunning() )
            return "Server is not running!" + readServerErrors(ReadAllStderr);

        QByteArray stdoutActual;
        QByteArray stderrActual;
        const int exitCode = run(arguments, &stdoutActual, &stderrActual, input);

        if ( !testStderr(stderrActual) || exitCode != 0 )
            return printClienAndServerStderr(stderrActual, exitCode);

        if (stdoutActual != stdoutExpected) {
            return "Test failed:"
                    + decorateOutput("Unexpected output", stdoutActual)
                    + decorateOutput("Expected output", stdoutExpected)
                    + printClienAndServerStderr(stderrActual, exitCode);
        }

        return readServerErrors();
    }

    QByteArray runClientWithError(const QStringList &arguments, int expectedExitCode,
                                  const QByteArray &stderrContains = QByteArray()) override
    {
        Q_ASSERT(expectedExitCode != 0);

        if ( !isServerRunning() )
            return "Server is not running!" + readServerErrors(ReadAllStderr);

        QByteArray stdoutActual;
        QByteArray stderrActual;
        const int exitCode = run(arguments, &stdoutActual, &stderrActual);

        if ( testStderr(stderrActual) ) {
            return "Test failed: Expected error output on client side."
                    + printClienAndServerStderr(stderrActual, exitCode);
        }

        if ( !stdoutActual.isEmpty() ) {
            return "Test failed: Expected empty output."
                    + decorateOutput("Unexpected output", stdoutActual)
                    + printClienAndServerStderr(stderrActual, exitCode);
        }

        if (exitCode != expectedExitCode) {
            return QString::fromLatin1("Test failed: Unexpected exit code %1; expected was %2")
                    .arg(exitCode)
                    .arg(expectedExitCode)
                    .toUtf8()
                    + printClienAndServerStderr(stderrActual, exitCode);
        }

        if ( !stderrActual.contains(stderrContains) ) {
            return QString::fromLatin1("Test failed: Expected error output on client side with \"%1\".")
                    .arg(QString::fromUtf8(stderrContains)).toUtf8()
                    + printClienAndServerStderr(stderrActual, exitCode);
        }

        return readServerErrors(ReadErrorsWithoutScriptException);
    }

    QByteArray getClipboard(const QString &mime = QString("text/plain"), ClipboardMode mode = ClipboardMode::Clipboard)
    {
        return clipboard()->data(mode, QStringList(mime)).value(mime).toByteArray();
    }

    QByteArray setClipboard(const QVariantMap &data, ClipboardMode mode) override
    {
        if ( !data.isEmpty() ) {
            // Wait for clipboard monitor
            QByteArray error;
            SleepTimer t(8000);
            do {
                error = runClient(
                    Args("monitoring() == isClipboardMonitorRunning()"),
                    QByteArrayLiteral("true\n"));
            } while (!error.isEmpty() && t.sleep());

            if (!error.isEmpty())
                return "Clipboard monitor is not running:" + error;
        }

        clipboard()->setData(mode, data);
        return {};
    }

    QByteArray setClipboard(const QByteArray &bytes, const QString &mime, ClipboardMode mode) override
    {
        if ( const QByteArray error = setClipboard(createDataMap(mime, bytes), mode);
                !error.isEmpty() )
        {
            return error;
        }

        return verifyClipboard(bytes, mime);
    }

    QByteArray verifyClipboard(const QByteArray &data, const QString &mime, bool exact = true) override
    {
        // Due to image conversions in clipboard check only png.
        if ( mime.startsWith(QStringLiteral("image/")) && mime != QStringLiteral("image/png") )
            return verifyClipboard("PNG", QStringLiteral("image/png"), false);

        PerformanceTimer perf;

        SleepTimer t(5000);
        QByteArray actualBytes;
        do {
            actualBytes = getClipboard(mime);
            if ( exact ? actualBytes == data : actualBytes.contains(data) ) {
                perf.printPerformance("verifyClipboard", QStringList() << QString::fromUtf8(data) << mime);
                waitFor(waitMsSetClipboard);
                RETURN_ON_ERROR( readServerErrors(), "Failed to set or test clipboard content" );
                return QByteArray();
            }
        } while (t.sleep());

        return QString::fromLatin1("Unexpected clipboard data for MIME \"%1\":")
                .arg(mime).toUtf8()
                + decorateOutput("Unexpected content", actualBytes)
                + decorateOutput("Expected content", data)
                + readServerErrors(ReadAllStderr);
    }

    QByteArray readServerErrors(ReadStderrFlag flag = ReadErrors) override
    {
        if (m_server) {
            QCoreApplication::processEvents();
            QByteArray output = readLogFile(maxReadLogSize);
            if ( flag == ReadAllStderr || !testStderr(output, flag) )
              return decorateOutput("Server STDERR", output);
        }

        return QByteArray();
    }

    QByteArray getClientOutput(const QStringList &arguments, QByteArray *stdoutActual) override
    {
        stdoutActual->clear();

        QByteArray stderrActual;
        int exitCode = run(arguments, stdoutActual, &stderrActual);
        if ( !testStderr(stderrActual) || exitCode != 0 )
            return printClienAndServerStderr(stderrActual, exitCode);

        RETURN_ON_ERROR( readServerErrors(), "Failed getting client output" );

        return "";
    }

    QByteArray waitOnOutput(const QStringList &arguments, const QByteArray &stdoutExpected) override
    {
        PerformanceTimer perf;
        QByteArray stdoutActual;

        SleepTimer t_(8000);
        do {
            RETURN_ON_ERROR( getClientOutput(arguments, &stdoutActual), "Failed to wait on client output" );
        } while (stdoutActual != stdoutExpected && t_.sleep());

        if (stdoutActual == stdoutExpected)
            return QByteArray();

        return QString::fromLatin1("Unexpected output for command \"%1\":")
                .arg(arguments.join(' ')).toUtf8()
                + decorateOutput("Unexpected content", stdoutActual)
                + decorateOutput("Expected content", stdoutExpected)
                + readServerErrors(ReadAllStderr);
    }

    QByteArray cleanupTestCase() override
    {
        return cleanup();
    }

    QByteArray initTestCase() override
    {
        return QByteArray();
    }

    QByteArray init() override
    {
        RETURN_ON_ERROR( cleanup(), "Failed to cleanup" );

        if ( isServerRunning() )
            RETURN_ON_ERROR( stopServer(), "Failed to stop server" );

        // Remove all configuration files and tab data.
        const auto settingsPath = settingsDirectoryPath();
        Q_ASSERT( !settingsPath.isEmpty() );
        QDir settingsDir(settingsPath);
        const QStringList settingsFileFilters(QStringLiteral("%1*").arg(appName));
        // Omit using dangerous QDir::removeRecursively().
        for ( const auto &fileName : settingsDir.entryList(settingsFileFilters) ) {
            const auto path = settingsDir.absoluteFilePath(fileName);
            QFile settingsFile(path);
            if ( settingsFile.exists() && !settingsFile.remove() ) {
                return QString::fromLatin1("Failed to remove settings file \"%1\": %2")
                    .arg(settingsPath, settingsFile.errorString())
                    .toUtf8();
            }
        }

        // Update settings for tests.
        {
            Settings settings;
            settings.clear();

            settings.beginGroup("Options");
            settings.setValue( QStringLiteral("language"), QStringLiteral("en") );
            settings.setValue( Config::clipboard_tab::name(), clipboardTabName );
            settings.setValue( Config::close_on_unfocus::name(), false );
            // Hide the main window even if there is no tray or minimize support.
            settings.setValue( Config::hide_main_window::name(), true );
            // Exercise limiting rows in Process Manager dialog when testing.
            settings.setValue( Config::max_process_manager_rows::name(), 4 );
            settings.endGroup();

            if ( !m_settings.isEmpty() ) {
                const bool pluginsTest = m_testId != defaultTestId;

                if (pluginsTest) {
                    settings.beginGroup("Plugins");
                    settings.beginGroup(m_testId);
                }

                for (auto it = m_settings.constBegin(); it != m_settings.constEnd(); ++it)
                    settings.setValue( it.key(), it.value() );

                if (pluginsTest) {
                    settings.endGroup();
                    settings.endGroup();
                }
            }
        }

        verifyConfiguration();

        // Clear clipboard.
        RETURN_ON_ERROR( setClipboard({}, ClipboardMode::Clipboard), "Failed to reset clipboard" );
#ifdef HAS_MOUSE_SELECTIONS
        RETURN_ON_ERROR( setClipboard({}, ClipboardMode::Selection), "Failed to reset selection" );
#endif

        RETURN_ON_ERROR( startServer(), "Failed to initialize server" );

        // Always show main window first so that the results are consistent with desktop environments
        // where user cannot hide main window (tiling window managers without tray).
        RETURN_ON_ERROR( runClient(Args("show"), ""), "Failed to show main window" );

        return QByteArray();
    }

    QByteArray cleanup() override
    {
        addFailedTest();
        return QByteArray();
    }

    QString shortcutToRemove() override
    {
        return ::shortcutToRemove();
    }

    void setEnv(const QString &name, const QString &value) override
    {
        m_env.insert(name, value);
    }

    bool writeOutErrors(const QByteArray &errors) override
    {
        if (errors.isEmpty())
            return false;

        QFile ferr;
        ferr.open(stderr, QIODevice::WriteOnly);
        ferr.write(errors);
        ferr.write("\n");
        ferr.close();
        return true;
    }

    void setupTest(const QString &id, const QString &allowPlugins, const QVariant &settings)
    {
        m_testId = id;
        m_settings = settings.toMap();
        m_env.insert("COPYQ_ALLOW_PLUGINS", allowPlugins);
    }

    int runTests(QObject *testObject, int argc = 0, char **argv = nullptr)
    {
        int exitCode = QTest::qExec(testObject, argc, argv);

        const int maxRuns = m_env.value("COPYQ_TESTS_RERUN_FAILED", "0").toInt();
        for (int runCounter = 0; exitCode != 0 && !m_failed.isEmpty() && runCounter < maxRuns; ++runCounter) {
            qInfo() << QString::fromLatin1("Rerunning %1 failed tests (%2/%3): %4")
                       .arg(m_failed.size())
                       .arg(runCounter + 1)
                       .arg(maxRuns)
                       .arg(m_failed.join(", "));
            QStringList args = m_failed;
            m_failed.clear();
            args.prepend( QString::fromUtf8(argv[0]) );
            exitCode = QTest::qExec(testObject, args);
        }
        m_failed.clear();

        return exitCode;
    }

private:
    void addFailedTest()
    {
        if ( QTest::currentTestFailed() )
            m_failed.append( QString::fromUtf8(QTest::currentTestFunction()) );
    }

    void verifyConfiguration()
    {
        AppConfig appConfig;
        QCOMPARE( appConfig.option<Config::close_on_unfocus>(), false );
        QCOMPARE( appConfig.option<Config::clipboard_tab>(), QString(clipboardTabName) );
        QCOMPARE( appConfig.option<Config::maxitems>(), Config::maxitems::defaultValue() );
        QCOMPARE( savedTabs().join(QStringLiteral(", ")), clipboardTabName );
        QCOMPARE( AppConfig().option<Config::tabs>(), QStringList() );
    }

    bool startTestProcess(QProcess *p, const QStringList &arguments,
                          QIODevice::OpenMode mode = QIODevice::ReadWrite,
                          const QStringList &environment = QStringList())
    {
        if ( environment.isEmpty() ) {
            p->setProcessEnvironment(m_env);
        } else {
            auto env = m_env;
            for (const QString &nameValue : environment) {
                const auto i = nameValue.indexOf(QLatin1Char('='));
                Q_ASSERT(i != -1);
                const auto name = nameValue.left(i);
                const auto value = nameValue.mid(i+1);
                env.insert(name, value);
            }
            p->setProcessEnvironment(env);
        }

        p->start( QCoreApplication::applicationFilePath(), arguments, mode );
        return p->waitForStarted(10000);
    }

    QByteArray waitForServerToStart()
    {
        SleepTimer t(15000);
        do {
            if ( run(Args() << "") == 0 )
                return QByteArray();
        } while ( t.sleep() );

        return "Unable to start server!" + readServerErrors(ReadAllStderr);
    }

    PlatformClipboard *clipboard()
    {
        if (m_clipboard == nullptr)
            m_clipboard = platformNativeInterface()->clipboard();
        return m_clipboard.get();
    }

    std::unique_ptr<QProcess> m_server;
    QProcessEnvironment m_env;
    QString m_testId;
    QVariantMap m_settings;

    QStringList m_failed;

    PlatformClipboardPtr m_clipboard;
};

} // namespace

Tests::Tests(const TestInterfacePtr &test, QObject *parent)
    : QObject(parent)
    , m_test(test)
{
}

void Tests::initTestCase()
{
    TEST(m_test->initTestCase());
}

void Tests::cleanupTestCase()
{
    TEST(m_test->cleanupTestCase());
}

void Tests::init()
{
    TEST(m_test->init());
}

void Tests::cleanup()
{
    TEST( m_test->cleanup() );
}

int Tests::run(
        const QStringList &arguments, QByteArray *stdoutData, QByteArray *stderrData, const QByteArray &in,
        const QStringList &environment)
{
    return m_test->run(arguments, stdoutData, stderrData, in, environment);
}

bool Tests::hasTab(const QString &tabName)
{
    QByteArray out;
    run(Args("tab"), &out);
    return splitLines(out).contains(tabName);
}

int runTests(int argc, char *argv[])
{
    QRegularExpression onlyPlugins;
    bool runPluginTests = true;

    if (argc > 1) {
        QString arg = argv[1];
        if (arg.startsWith("PLUGINS:")) {
            arg.remove(QRegularExpression("^PLUGINS:"));
            onlyPlugins = QRegularExpression(arg, QRegularExpression::CaseInsensitiveOption);
            --argc;
            ++argv;
        } else {
            // Omit plugin tests if specific core tests requested.
            const QString lastArg(argv[argc - 1]);
            runPluginTests = lastArg.startsWith("-");
        }
    }

    const auto platform = platformNativeInterface();
    std::unique_ptr<QGuiApplication> app( platform->createTestApplication(argc, argv) );
    Q_UNUSED(app)

    QCoreApplication::setOrganizationName(appName);
    QCoreApplication::setApplicationName(appName);

    // Set higher default tests timeout.
    // The default value is 5 minutes (in Qt 5.15) which is not enough to run
    // all tests in Tests class on some systems.
    bool ok;
    const int timeout = qEnvironmentVariableIntValue("QTEST_FUNCTION_TIMEOUT", &ok);
    if (!ok || timeout <= 0)
        qputenv("QTEST_FUNCTION_TIMEOUT", QByteArray::number(15 * 60 * 1000));

    int exitCode = 0;
    std::shared_ptr<TestInterfaceImpl> test(new TestInterfaceImpl);
    const auto runTests = [&](QObject *tests){
        exitCode = std::max(exitCode, test->runTests(tests, argc, argv));
        test->stopServer();
    };

    if (onlyPlugins.pattern().isEmpty()) {
        test->setupTest("CORE", defaultTestPlugins, QVariant());
        Tests tc(test);
        runTests(&tc);
    }

    if (runPluginTests) {
        ItemFactory itemFactory;
        itemFactory.loadPlugins();
        for ( const auto &loader : itemFactory.loaders() ) {
            if ( loader->id().contains(onlyPlugins) ) {
                std::unique_ptr<QObject> pluginTests( loader->tests(test) );
                if ( pluginTests != nullptr ) {
                    const auto pluginId = loader->id();
                    const auto settings = pluginTests->property("CopyQ_test_settings");
                    test->setupTest(pluginId, pluginId, settings);
                    runTests(pluginTests.get());
                }
            }
        }
    }

    return exitCode;
}
