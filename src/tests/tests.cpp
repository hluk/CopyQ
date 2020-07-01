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

#include "tests.h"
#include "test_utils.h"

#include "common/appconfig.h"
#include "common/client_server.h"
#include "common/common.h"
#include "common/config.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/settings.h"
#include "common/shortcuts.h"
#include "common/sleeptimer.h"
#include "common/textdata.h"
#include "common/version.h"
#include "item/itemfactory.h"
#include "item/itemwidget.h"
#include "item/serialize.h"
#include "gui/tabicons.h"
#include "platform/platformnativeinterface.h"

#include <QClipboard>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMap>
#include <QMimeData>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTest>
#include <QTimer>

#include <memory>

#define WITH_TIMEOUT "afterMilliseconds(10000, fail); "

#define KEEP_STDIN_OPEN "KEEP_STDIN_OPEN"

// WORKAROUND: Checking clipboard right after closing menu gets stuck on OS X.
#define ACTIVATE_MENU_ITEM(MENU_ID, WIDGET_ID, CONTENT) \
    RUN("keys" << MENU_ID << "ENTER", ""); \
    RUN("keys" << WIDGET_ID, ""); \
    WAIT_FOR_CLIPBOARD(CONTENT)

namespace {

const int maxReadLogSize = 1 * 1024 * 1024;

const auto clipboardTabName = "CLIPBOARD";
const auto defaultSessionColor = "#ff8800";
const auto defaultTagColor = "#000000";

const auto clipboardBrowserId = "focus:ClipboardBrowser";
const auto clipboardBrowserRefreshButtonId = "focus:ClipboardBrowserRefreshButton";
const auto filterEditId = "focus:Utils::FilterLineEdit";
const auto trayMenuId = "focus:TrayMenu";
const auto menuId = "focus:Menu";
const auto customMenuId = "focus:CustomMenu";
const auto editorId = "focus::ItemEditorWidget";
const auto tabDialogLineEditId = "focus:lineEditTabName";
const auto commandDialogId = "focus:CommandDialog";
const auto commandDialogSaveButtonId = "focus::QPushButton in :QMessageBox";
const auto commandDialogListId = "focus:listWidgetItems";
const auto shortcutButtonId = "focus::QToolButton in CommandDialog";
const auto shortcutDialogId = "focus::QLineEdit in ShortcutDialog";
const auto actionDialogId = "focus:ActionDialog";
const auto aboutDialogId = "focus:AboutDialog";
const auto logDialogId = "focus:LogDialog";
const auto actionHandlerDialogId = "focus:ActionHandlerDialog";
const auto actionHandlerFilterId = "focus:filterLineEdit";
const auto actionHandlerTableId = "focus:tableView";
const auto clipboardDialogId = "focus:ClipboardDialog";
const auto clipboardDialogFormatListId = "focus:listWidgetFormats";
const auto confirmExitDialogId = "focus::QPushButton in :QMessageBox";
const auto itemPreviewId = "focus:in dockWidgetItemPreviewContents";

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

template <typename Fn1, typename Fn2>
void runMultiple(Fn1 f1, Fn2 f2)
{
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(0);
    QObject::connect(&timer, &QTimer::timeout, f2);
    timer.start();
    f1();
}

bool testStderr(const QByteArray &stderrData, TestInterface::ReadStderrFlag flag = TestInterface::ReadErrors)
{
    static const QRegularExpression reFailure("Warning:|ERROR:|ASSERT", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reClient(
        R"(CopyQ Note \[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}\] <Client-[^\n]*)");

    QString output = QString::fromUtf8(stderrData);

    // Ignore exceptions and errors from clients in application log
    // (these are expected in some tests).
    output.remove(reClient);

    // FIXME: These warnings should be fixed.
    // Ignore known warnings.
    output.remove("QtWarning: QXcbXSettings::QXcbXSettings(QXcbScreen*) Failed to get selection owner for XSETTINGS_S atom");
    output.remove("QtWarning: QXcbConnection: XCB error:");
    output.remove("QtWarning: QXcbClipboard: SelectionRequest too old");
    output.remove("QtWarning: libpng warning: iCCP: known incorrect sRGB profile");
    output.remove("QtWarning: QMime::convertToMime: unhandled mimetype: text/plain");
    output.remove("QtWarning: QWindowsWindow::setGeometry: Unable to set geometry");
    output.remove("QtWarning: setGeometry: Unable to set geometry");
    output.remove("ERROR: QtCritical: QWindowsPipeWriter::write failed. (The pipe is being closed.)");
    output.remove("ERROR: QtCritical: QWindowsPipeWriter: asynchronous write failed. (The pipe has been ended.)");
#ifdef Q_OS_MAC
    output.remove("QtWarning: Failed to get QCocoaScreen for NSObject(0x0)");
    output.remove("ERROR: Failed to open session mutex: QSystemSemaphore::handle:: ftok failed");

    static const QRegularExpression reBadPos(R"(QtWarning: Window position.* outside any known screen.*)");
    output.remove(reBadPos);
#endif

    if ( output.indexOf(reFailure) != -1 )
        return false;

    if (flag == TestInterface::ReadErrorsWithoutScriptException)
        return true;

    const QRegularExpression scriptExceptionError("ScriptError:");
    return output.indexOf(scriptExceptionError) == -1;
}

QByteArray getClipboard(const QString &mime = QString("text/plain"), ClipboardMode mode = ClipboardMode::Clipboard)
{
    const QMimeData *data = clipboardData(mode);
    return (data != nullptr) ? data->data(mime) : QByteArray();
}

bool waitWhileFileExists(const QFile &file)
{
    SleepTimer t(2000);
    while (file.exists() && t.sleep()) {}
    return !file.exists();
}

/// Generate unique data.
QByteArray generateData()
{
    static int i = 0;
    const QByteArray id = "tests_"
            + QByteArray::number(QDateTime::currentMSecsSinceEpoch() % 1000);
    return id + '_' + QByteArray::number(++i);
}

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
        m_env.insert("COPYQ_SESSION_NAME", "TEST");
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
            return QString("Failed to launch \"%1\": %2")
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
            return QString("Test failed: Unexpected exit code %1; expected was %2")
                    .arg(exitCode)
                    .arg(expectedExitCode)
                    .toUtf8()
                    + printClienAndServerStderr(stderrActual, exitCode);
        }

        if ( !stderrActual.contains(stderrContains) ) {
            return QString("Test failed: Expected error output on client side with \"%1\".")
                    .arg(QString::fromUtf8(stderrContains)).toUtf8()
                    + printClienAndServerStderr(stderrActual, exitCode);
        }

        return readServerErrors(ReadErrorsWithoutScriptException);
    }

    QByteArray setClipboard(const QByteArray &bytes, const QString &mime, ClipboardMode mode) override
    {
        if ( getClipboard(mime, mode) == bytes )
            return QByteArray();

        waitFor(waitMsSetClipboard);

        auto mimeData = new QMimeData();
        mimeData->setData(mime, bytes);
        const auto qmode = mode == ClipboardMode::Clipboard ? QClipboard::Clipboard : QClipboard::Selection;
        QGuiApplication::clipboard()->setMimeData(mimeData, qmode);

        waitFor(waitMsSetClipboard);
        return verifyClipboard(bytes, mime);
    }

    QByteArray verifyClipboard(const QByteArray &data, const QString &mime, bool exact = true) override
    {
        PerformanceTimer perf;

        SleepTimer t(waitMsSetClipboard * 5);
        do {
            if ( exact ? getClipboard(mime) == data : getClipboard(mime).contains(data) ) {
                perf.printPerformance("verifyClipboard", QStringList() << QString::fromUtf8(data) << mime);
                waitFor(waitMsSetClipboard);
                RETURN_ON_ERROR( readServerErrors(), "Failed to set or test clipboard content" );
                return QByteArray();
            }
        } while (t.sleep());

        const QByteArray actualBytes = getClipboard(mime);
        return QString("Unexpected clipboard data for MIME \"%1\":")
                .arg(mime).toUtf8()
                + decorateOutput("Unexpected content", actualBytes)
                + decorateOutput("Expected content", data)
                + readServerErrors(ReadAllStderr);
    }

    QByteArray readServerErrors(ReadStderrFlag flag = ReadErrors) override
    {
        if (m_server) {
            QCoreApplication::processEvents();
            const QByteArray output = readLogFile(maxReadLogSize).toUtf8();
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

        RETURN_ON_ERROR( readServerErrors(), "Failed gettting client output" );

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

        return QString("Unexpected output for command \"%1\":")
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
        const QStringList settingsFileFilters("copyq.test*");
        // Omit using dangerous QDir::removeRecursively().
        for ( const auto &fileName : settingsDir.entryList(settingsFileFilters) ) {
            const auto path = settingsDir.absoluteFilePath(fileName);
            QFile settingsFile(path);
            if ( !settingsFile.remove() ) {
                return QString("Failed to remove settings file \"%1\": %2")
                    .arg(settingsPath, settingsFile.errorString())
                    .toUtf8();
            }
        }

        // Update settings for tests.
        {
            Settings settings;
            settings.clear();

            settings.beginGroup("Options");
            settings.setValue( Config::clipboard_tab::name(), clipboardTabName );
            settings.setValue( Config::close_on_unfocus::name(), false );
            // Exercise limiting rows in Process Manager dialog when testing.
            settings.setValue( Config::max_process_manager_rows::name(), 4 );
            settings.endGroup();

            if ( !m_settings.isEmpty() ) {
                const bool pluginsTest = m_testId != "CORE";

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
        RETURN_ON_ERROR( setClipboard(QByteArray(), mimeText, ClipboardMode::Clipboard), "Failed to reset clipboard" );
#ifdef HAS_MOUSE_SELECTIONS
        RETURN_ON_ERROR( setClipboard(QByteArray(), mimeText, ClipboardMode::Selection), "Failed to reset selection" );
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

    void setupTest(const QString &id, const QVariant &settings)
    {
        m_testId = id;
        m_settings = settings.toMap();
        m_env.insert("COPYQ_TEST_ID", id);
    }

    int runTests(QObject *testObject, int argc = 0, char **argv = nullptr)
    {
        int exitCode = QTest::qExec(testObject, argc, argv);

        const int maxRuns = m_env.value("COPYQ_TESTS_RERUN_FAILED", "0").toInt();
        for (int runCounter = 0; exitCode != 0 && !m_failed.isEmpty() && runCounter < maxRuns; ++runCounter) {
            qInfo() << QString("Rerunning %1 failed tests (%2/%3): %4")
                       .arg(m_failed.size())
                       .arg(runCounter + 1)
                       .arg(maxRuns)
                       .arg(m_failed.join(", "));
            QStringList args = m_failed;
            m_failed.clear();
            args.prepend( QString::fromUtf8(argv[0]) );
            exitCode = QTest::qExec(testObject, args);
        }

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
        QCOMPARE( savedTabs(), QStringList(clipboardTabName) );
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

    std::unique_ptr<QProcess> m_server;
    QProcessEnvironment m_env;
    QString m_testId;
    QVariantMap m_settings;

    QStringList m_failed;
};

QString keyNameFor(QKeySequence::StandardKey standardKey)
{
    return QKeySequence(standardKey).toString();
}

int count(const QStringList &items, const QString &pattern)
{
    int from = -1;
    int count = 0;
    const QRegularExpression re(pattern);
    while ( (from = items.indexOf(re, from + 1)) != -1 )
        ++count;
    return count;
}

QStringList splitLines(const QString &nativeText)
{
    return nativeText.split(QRegularExpression("\r\n|\n|\r"));
}

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

void Tests::readLog()
{
    QByteArray stdoutActual;
    QByteArray stderrActual;
    QCOMPARE( run(Args("info") << "log", &stdoutActual, &stderrActual), 0 );
    QVERIFY2( testStderr(stderrActual), stderrActual );
    QCOMPARE( logFileName() + "\n", QString::fromUtf8(stdoutActual) );
    QTRY_VERIFY( !readLogFile(maxReadLogSize).isEmpty() );

#define LOGGED_ONCE(PATTERN) \
    QTRY_COMPARE( count(splitLines(readLogFile(maxReadLogSize)), PATTERN), 1 )

    LOGGED_ONCE(
        R"(^CopyQ DEBUG \[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}\] <Server-\d+>: Loading configuration$)");

    LOGGED_ONCE(
        R"(^CopyQ DEBUG \[.*\] <Server-\d+>: Starting monitor$)");

    LOGGED_ONCE(
        R"(^.*<monitorClipboard-\d+>: Clipboard formats to save: .*$)");
}

void Tests::commandHelp()
{
    QByteArray stdoutActual;
    QByteArray stderrActual;
    QCOMPARE( run(Args("help"), &stdoutActual, &stderrActual), 0 );
    QVERIFY2( testStderr(stderrActual), stderrActual );
    QVERIFY(!stdoutActual.isEmpty());

    const QStringList commands = QStringList()
            << "show"
            << "hide"
            << "toggle"
            << "menu"
            << "exit"
            << "help"
            << "version"
            << "clipboard"
            << "copy"
            << "paste"
            << "action"
            << "add"
            << "remove";

    for (const auto &command : commands) {
        QCOMPARE( run(Args("help") << command, &stdoutActual, &stderrActual), 0 );
        QVERIFY2( testStderr(stderrActual), stderrActual );
        QVERIFY( !stdoutActual.isEmpty() );
        const QString help = QString::fromUtf8(stdoutActual);
        QVERIFY( help.contains(QRegularExpression("\\b" + QRegularExpression::escape(command) + "\\b")) );
    }

    // Print error on unknown function name.
    RUN_EXPECT_ERROR("help" << "xxx", CommandException);
}

void Tests::commandVersion()
{
    QByteArray stdoutActual;
    QByteArray stderrActual;
    QCOMPARE( run(Args("version"), &stdoutActual, &stderrActual), 0 );
    QVERIFY2( testStderr(stderrActual), stderrActual );
    QVERIFY( !stdoutActual.isEmpty() );

    const QString version = QString::fromUtf8(stdoutActual);
    // Version contains application name and version.
    QVERIFY( version.contains(QRegularExpression("\\bCopyQ\\b.*" + QRegularExpression::escape(COPYQ_VERSION))) );
    // Version contains Qt version.
    QVERIFY( version.contains(QRegularExpression("\\bQt:\\s+\\d")) );
}

void Tests::badCommand()
{
    RUN_EXPECT_ERROR_WITH_STDERR("xxx", CommandException, "xxx");
    RUN_EXPECT_ERROR_WITH_STDERR("tab" << testTab(1) << "yyy", CommandException, "yyy");

    // Bad command shoudn't create new tab.
    QByteArray stdoutActual;
    QByteArray stderrActual;
    QCOMPARE( run(Args("tab"), &stdoutActual, &stderrActual), 0 );
    QVERIFY2( testStderr(stderrActual), stderrActual );
    QVERIFY( !QString::fromUtf8(stdoutActual)
             .contains(QRegularExpression("^" + QRegularExpression::escape(testTab(1)) + "$")) );
}

void Tests::badSessionName()
{
    RUN_EXPECT_ERROR("-s" << "max_16_characters_in_session_name_allowed" << "", CommandBadSyntax);
    RUN_EXPECT_ERROR("-s" << "spaces disallowed" << "", CommandBadSyntax);
}

void Tests::commandExit()
{
    RUN("exit", "Terminating server.\n")

    TEST( m_test->waitForServerToStop() );

    QCOMPARE( run(Args("exit")), 1 );
}

void Tests::commandEval()
{
    RUN("eval" << "", "");
    RUN("eval" << "1", "1\n");
    RUN("eval" << "[1,2,3]", "1\n2\n3\n");
    RUN("eval" << "'123'", "123\n");
    RUN("eval" << "'123'", "123\n");

    RUN("-e" << "", "");
    RUN("-e" << "1", "1\n");
}

void Tests::commandEvalThrows()
{
    RUN_EXPECT_ERROR_WITH_STDERR("eval" << "throw 'TEST_EXCEPTION'", CommandException, "TEST_EXCEPTION");
}

void Tests::commandEvalSyntaxError()
{
    RUN_EXPECT_ERROR_WITH_STDERR("eval" << "(", CommandException, "syntax error");
}

void Tests::commandEvalArguments()
{
    RUN("eval" << "str(arguments[1]) + ', ' + str(arguments[2])" << "Test 1" << "Test 2",
        "Test 1, Test 2\n");
}

void Tests::commandPrint()
{
    RUN("print" << "1", "1");
    RUN("print" << "TEST", "TEST");
}

void Tests::commandAbort()
{
    RUN("eval" << "abort(); 1", "");
    RUN("eval" << "eval('abort(); print(1)'); 2", "");
    RUN("eval" << "execute('copyq', 'eval', '--', 'abort(); print(1)'); 2", "2\n");
}

void Tests::commandFail()
{
    QByteArray stdoutActual;
    QByteArray stderrActual;
    QCOMPARE( run(Args("fail"), &stdoutActual, &stderrActual), 1 );
    QVERIFY2( testStderr(stderrActual), stderrActual );
    QCOMPARE( stdoutActual, QByteArray() );
}

void Tests::commandSource()
{
    const auto script =
            R"(
            test = function() { return " TEST" }
            print("SOURCED")
            )";

    QTemporaryFile scriptFile;
    QVERIFY(scriptFile.open());
    scriptFile.write(script);
    scriptFile.close();
    const auto scriptFileName = scriptFile.fileName();

    RUN("source" << scriptFileName, "SOURCED")
    RUN("source" << scriptFileName << "test()", "SOURCED TEST\n")
}

void Tests::commandVisible()
{
    RUN("visible", "true\n");
}

void Tests::commandToggle()
{
    RUN("visible", "true\n");
    RUN("toggle", "false\n");
    WAIT_ON_OUTPUT("visible", "false\n");

    RUN("toggle", "true\n");
    WAIT_ON_OUTPUT("visible", "true\n");
}

void Tests::commandHide()
{
    RUN("visible", "true\n");
    RUN("hide", "");
    WAIT_ON_OUTPUT("visible", "false\n");
}

void Tests::commandShow()
{
    RUN("visible", "true\n");
    RUN("hide", "");
    WAIT_ON_OUTPUT("visible", "false\n");

    RUN("show", "");
    WAIT_ON_OUTPUT("visible", "true\n");
}

void Tests::commandShowAt()
{
    RUN("visible", "true\n");
    RUN("hide", "");
    WAIT_ON_OUTPUT("visible", "false\n");

    RUN("showAt", "");
    WAIT_ON_OUTPUT("visible", "true\n");
}

void Tests::commandFocused()
{
    RUN("focused", "true\n");
    RUN("hide", "");
    RUN("focused", "false\n");
}

void Tests::commandsUnicode()
{
    const auto text = QString::fromUtf8(QByteArray("Zkouška s různými českými znaky!"));
    RUN_WITH_INPUT("eval" << "input()", text, text);
    RUN_WITH_INPUT("eval" << "str(input())", text, text + "\n");

    RUN_WITH_INPUT("eval" << "fromUnicode(str(input()), 'utf8')", text, text);
    RUN_WITH_INPUT("eval" << "toUnicode(fromUnicode(str(input()), 'utf16'), 'utf16')", text, text + "\n");
    RUN_WITH_INPUT("eval" << "toUnicode(fromUnicode(str(input()), 'utf32le'), 'utf32le')", text, text + "\n");
    RUN_WITH_INPUT("eval" << "toUnicode( fromUnicode(str(input()), 'utf16le') )", text, text + "\n");
}

void Tests::commandsAddRead()
{
    RUN("add" << "A", "");
    RUN("read" << "0", "A");

    RUN("add" << "B", "");
    RUN("read" << "0", "B");
    RUN("read" << "1", "A");

    RUN("add" << "C" << "D", "");
    RUN("read" << "0", "D");
    RUN("read" << "1", "C");
    RUN("read" << "2", "B");
    RUN("read" << "3", "A");
}

void Tests::commandsWriteRead()
{
    const QByteArray input("\x00\x01\x02\x03\x04", 5);
    const auto arg1 = QString::fromLatin1("\x01\x02\x03\x04");
    const auto arg2 = QString::fromLatin1("\x7f\x6f\x5f\x4f");
    TEST( m_test->runClient(
              Args() << "write"
              << COPYQ_MIME_PREFIX "test1" << arg1
              << COPYQ_MIME_PREFIX "test2" << "-"
              << COPYQ_MIME_PREFIX "test3" << arg2, "",
              input) );
    RUN("read" << COPYQ_MIME_PREFIX "test1" << "0", arg1.toLatin1());
    RUN("read" << COPYQ_MIME_PREFIX "test2" << "0", input);
    RUN("read" << COPYQ_MIME_PREFIX "test3" << "0", arg2.toLatin1());
}

void Tests::commandChange()
{
    RUN("add" << "C" << "B" << "A", "");
    RUN("change" << "1" << "text/plain" << "b", "");
    RUN("separator" << " " << "read" << "0" << "1" << "2", "A b C");

    RUN("change" << "1" << "text/plain" << "B" << "text/html" << "<b>B</b>", "");
    RUN("read" << "text/html" << "1", "<b>B</b>");
    RUN("separator" << " " << "read" << "0" << "1" << "2", "A B C");

    RUN("change(1, 'text/html', undefined)", "");
    RUN("read" << "?" << "1", "text/plain\n");
}

void Tests::commandSetCurrentTab()
{
    const auto tab = testTab(1);
    RUN("setCurrentTab" << tab, "");
    RUN("testSelected", tab + "\n");
}

void Tests::commandConfig()
{
    QByteArray stdoutActual;
    QByteArray stderrActual;
    QCOMPARE( run(Args("config"), &stdoutActual, &stderrActual), 0 );
    QVERIFY2( testStderr(stderrActual), stderrActual );
    QVERIFY( !stdoutActual.isEmpty() );

    // invalid option
    RUN_EXPECT_ERROR_WITH_STDERR("config" << "xxx", CommandException, "xxx");

    QCOMPARE( run(Args("config") << "tab_tree", &stdoutActual, &stderrActual), 0 );
    QVERIFY2( testStderr(stderrActual), stderrActual );
    QVERIFY2( stdoutActual == "true\n" || stdoutActual == "false\n", stdoutActual);

    RUN("config" << "tab_tree" << "true", "true\n");
    RUN("config" << "tab_tree", "true\n");

    RUN("config" << "tab_tree" << "false", "false\n");
    RUN("config" << "tab_tree", "false\n");

    RUN("config" << "tab_tree" << "1", "true\n");
    RUN("config" << "tab_tree", "true\n");

    RUN("config" << "tab_tree" << "0", "false\n");
    RUN("config" << "tab_tree", "false\n");

    // Set multiple options.
    RUN("config" << "tab_tree" << "0" << "text_wrap" << "1",
        "tab_tree=false\n"
        "text_wrap=true\n");

    // Don't set any options if there is an invalid one.
    RUN_EXPECT_ERROR_WITH_STDERR("config" << "tab_tree" << "1" << "xxx" << "0", CommandException, "xxx");
    RUN("config" << "tab_tree", "false\n");
}

void Tests::commandToggleConfig()
{
    RUN("toggleConfig" << "check_clipboard", "false\n");
    RUN("config" << "check_clipboard", "false\n");
    RUN("toggleConfig" << "check_clipboard", "true\n");
    RUN("config" << "check_clipboard", "true\n");

    RUN_EXPECT_ERROR("toggleConfig", CommandException);
    RUN_EXPECT_ERROR_WITH_STDERR("toggleConfig" << "xxx", CommandException, "xxx");
    RUN_EXPECT_ERROR_WITH_STDERR("toggleConfig" << "clipboard_tab", CommandException, "clipboard_tab");
}

void Tests::commandDialog()
{
    RUN(Args() << "keys" << clipboardBrowserId, "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "dialog('text')", "TEST\n"); },
        [&]() { RUN(Args() << "keys" << "focus::QLineEdit in :QDialog" << ":TEST" << "ENTER", ""); }
    );

    RUN(Args() << "keys" << clipboardBrowserId, "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "dialog('text') === undefined", "true\n"); },
        [&]() { RUN(Args() << "keys" << "focus::QLineEdit in :QDialog" << "ESCAPE", ""); }
    );

    RUN(Args() << "keys" << clipboardBrowserId, "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "dialog('.defaultChoice', 2, 'list', [1, 2, 3])", "2\n"); },
        [&]() { RUN(Args() << "keys" << "focus::QComboBox in :QDialog" << "ENTER", ""); }
    );

    RUN(Args() << "keys" << clipboardBrowserId, "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "dialog('.defaultChoice', '', 'list', [1, 2, 3])", "\n"); },
        [&]() { RUN(Args() << "keys" << "focus::QComboBox in :QDialog" << "ENTER", ""); }
    );

    RUN(Args() << "keys" << clipboardBrowserId, "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "dialog('list', [0, 1, 2])", "0\n"); },
        [&]() { RUN(Args() << "keys" << "focus::QComboBox in :QDialog" << "ENTER", ""); }
    );

    // Can't focus configuration checkboxes on OS X
#ifndef Q_OS_MAC
    RUN(Args() << "keys" << clipboardBrowserId, "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "dialog('boolean', true) === true", "true\n"); },
        [&]() { RUN(Args() << "keys" << "focus::QCheckBox in :QDialog" << "ENTER", ""); }
    );
#endif
}

void Tests::commandDialogCloseOnDisconnect()
{
    RUN("afterMilliseconds(0, abort); dialog()", "");
}

void Tests::commandMenuItems()
{
    RUN(Args() << "keys" << clipboardBrowserId, "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "menuItems('a', 'b', 'c')", "a\n"); },
        [&]() { RUN(Args() << "keys" << customMenuId << "ENTER", ""); }
    );

    RUN(Args() << "keys" << clipboardBrowserId, "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "menuItems([{'text/plain': 'a'}, {'text/plain': 'b'}])", "0\n"); },
        [&]() { RUN(Args() << "keys" << customMenuId << "ENTER", ""); }
    );

    RUN(Args() << "keys" << clipboardBrowserId, "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "menuItems('a', 'b', 'c')", "\n"); },
        [&]() { RUN(Args() << "keys" << customMenuId << "ESCAPE", ""); }
    );

    RUN(Args() << "keys" << clipboardBrowserId, "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "menuItems([{'text/plain': 'a'}, {'text/plain': 'b'}])", "-1\n"); },
        [&]() { RUN(Args() << "keys" << customMenuId << "ESCAPE", ""); }
    );

    RUN(Args() << "keys" << clipboardBrowserId, "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "menuItems('a', 'b', 'c')", "b\n"); },
        [&]() { RUN(Args() << "keys" << customMenuId << ":b" << "ENTER", ""); }
    );

    RUN(Args() << "keys" << clipboardBrowserId, "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "menuItems([{'text/plain': 'a'}, {'text/plain': 'b'}])", "1\n"); },
        [&]() { RUN(Args() << "keys" << customMenuId << ":b" << "ENTER", ""); }
    );

    RUN("afterMilliseconds(0, abort); menuItems('a', 'b', 'c')", "");
}

void Tests::commandsPackUnpack()
{
    QMap<QByteArray, QByteArray> data;
    data[mimeText] = "plain text";
    data[mimeHtml] = "<b>HTML text</b>";
    data[COPYQ_MIME_PREFIX "test1"] = "test1 data";
    data[COPYQ_MIME_PREFIX "test2"] = "test2 data";

    auto args = Args() << "write";
    for (const auto &mime : data.keys())
        args << mime << data[mime];
    RUN(args, "");

    const QByteArray script1 =
            "var data = read('" + toByteArray(mimeItems) + "', 0); var item = unpack(data);";

    // Unpack item read from list.
    for (const auto &mime : data.keys()) {
        RUN("eval"
            << script1 + "var mime = '" + mime + "'; print(mime + ':' + str(item[mime]))",
            mime + ':' + data[mime]);
    }

    // Test pack and unpack consistency.
    const QByteArray script2 = "data = pack(item); item = unpack(data);";
    for (const auto &mime : data.keys()) {
        RUN("eval"
            << script1 + script2 + "var mime = '" + mime + "'; print(mime + ':' + str(item[mime]))",
            mime + ':' + data[mime]);
    }
}

void Tests::commandsBase64()
{
    const QByteArray data = "0123456789\001\002\003\004\005\006\007abcdefghijklmnopqrstuvwxyz!";
    const QByteArray base64 = data.toBase64();

    TEST( m_test->runClient(Args() << "eval" << "print(input())", data, data) );
    TEST( m_test->runClient(Args() << "eval" << "print(tobase64(input()))", base64, data) );

    // Line break is added only if return value is string;
    // tobase64() returns string, frombase64() returns byte array.
    RUN("tobase64" << data, base64 + '\n');
    RUN("frombase64" << base64, data);

    TEST( m_test->runClient(Args() << "eval" << "print(frombase64(tobase64(input())))", data, data) );

    // Test Base64 encoding and decoding consistency.
    TEST( m_test->runClient(
              Args() << "eval"
              << "var base64 = tobase64(input()); if (str(input()) === str(frombase64(base64))) print('OK')",
              "OK", data) );
}

void Tests::commandsGetSetItem()
{
    QMap<QByteArray, QByteArray> data;
    data["text/plain"] = "plain text";
    data["text/html"] = "<b>HTML text</b>";
    data[COPYQ_MIME_PREFIX "test1"] = "test1 data";
    data[COPYQ_MIME_PREFIX "test2"] = "test2 data";

    const QString tab = testTab(1);
    const Args args = Args("tab") << tab;

    Args args2 = args;
    args2 << "write";
    for (const auto &mime : data.keys())
        args2 << mime << data[mime];
    RUN(args2, "");

    // Get item from list.
    for (const auto &mime : data.keys()) {
        RUN(args << "eval"
            << "var mime = '" + mime + "'; print(mime + ':' + str(getitem(0)[mime]))",
            mime + ':' + data[mime]);
    }

    // Set item.
    RUN(args << "eval"
        << "setitem(1, { 'text/plain': 'plain text 2', 'text/html': '<b>HTML text 2</b>' })",
        "");

    RUN(args << "eval" << "print(getitem(1)['text/plain'])", "plain text 2");
    RUN(args << "eval" << "print(getitem(1)['text/html'])", "<b>HTML text 2</b>");
}

void Tests::commandsChecksums()
{
    RUN("md5sum" << "TEST", "033bd94b1168d7e4f0d644c3c95e35bf\n");
    RUN("sha1sum" << "TEST", "984816fd329622876e14907634264e6f332e9fb3\n");
    RUN("sha256sum" << "TEST", "94ee059335e587e501cc4bf90613e0814f00a7b08bc7c648fd865a2af6a22cc2\n");
    RUN("sha512sum" << "TEST", "7bfa95a688924c47c7d22381f20cc926f524beacb13f84e203d4bd8cb6ba2fce81c57a5f059bf3d509926487bde925b3bcee0635e4f7baeba054e5dba696b2bf\n");
}

void Tests::commandEscapeHTML()
{
    RUN("escapeHTML" << "&\n<\n>", "&amp;<br />&lt;<br />&gt;\n");
}

void Tests::commandExecute()
{
    const QByteArray script =
            "function test(c, expected_stdout, expected_exit_code) {"
            "    if (str(c.stdout) !== expected_stdout) print('Unexpected stdout: \"' + str(c.stdout) + '\"');"
            "    if (c.exit_code !== expected_exit_code) print('Unexpected exit_code: ' + str(c.exit_code));"
            "}";

    RUN("eval" << script +
        "c = execute('copyq', 'write', 'text/plain', 'plain text', 'text/html', '<b>test HTML</b>');"
        "test(c, '', 0);"
        , "");

    RUN("eval" << script +
        "c = execute('copyq', 'read', 'text/plain', 0);"
        "test(c, 'plain text', 0);"
        , "");

    RUN("eval" << script +
        "c = execute('copyq', 'read', 'text/html', 0);"
        "test(c, '<b>test HTML</b>', 0);"
        , "");

    RUN("eval" << script +
        "c = execute('copyq', 'read', 0, function(lines) { print(lines); });"
        "test(c, 'plain text', 0);"
        , "plain text");
}

void Tests::commandSettings()
{
    RUN("config" << "clipboard_tab" << "TEST", "TEST\n");

    RUN("settings" << "test_variable", "");
    RUN("settings" << "test_variable" << "TEST VALUE", "");
    RUN("settings" << "test_variable", "TEST VALUE");
    RUN("settings" << "test_variable" << "TEST VALUE 2", "");
    RUN("settings" << "test_variable", "TEST VALUE 2");

    RUN("config" << "clipboard_tab", "TEST\n");
}

void Tests::commandsEnvSetEnv()
{
    RUN("eval" <<
        "\n var name = 'COPYQ_ENV_TEST'"
        "\n if (setEnv(name, 'OK'))"
        "\n   print(env(name))"
        "\n else"
        "\n   print('FAILED')"
        , "OK"
        );
}

void Tests::commandSleep()
{
    QElapsedTimer t;

    t.start();
    RUN("sleep" << "100", "");
    const auto afterElapsed100Ms = t.elapsed();
    QVERIFY(afterElapsed100Ms > 100);

    t.start();
    RUN("sleep" << "1000", "");
    const auto afterElapsed1000Ms = t.elapsed();
    QVERIFY(afterElapsed1000Ms > 1000);
}

void Tests::commandsData()
{
    RUN("eval" << "setData('x', 'X'); data('x')", "X");
    RUN("eval" << "setData('x', 'X'); setData('y', 'Y'); str(data('x')) + str(data('y'))", "XY\n");

    RUN("dataFormats", "");
    RUN("eval" << "setData('x'); dataFormats()", "x\n");
    RUN("eval" << "setData('x'); setData('y'); dataFormats()", "x\ny\n");

    RUN("eval" << "setData('x'); setData('y'); removeData('x'); dataFormats()", "y\n");
    RUN("eval" << "setData('x'); setData('y'); removeData('y'); dataFormats()", "x\n");
}

void Tests::commandCurrentWindowTitle()
{
    RUN("disable", "");
#ifdef Q_OS_MAC
    WAIT_ON_OUTPUT("currentWindowTitle", "CopyQ\n");
#else
    WAIT_ON_OUTPUT("currentWindowTitle", "CopyQ-TEST\n");
#endif
    RUN("enable", "");
}

void Tests::commandCopy()
{
    RUN("copy" << "A", "true\n");
    WAIT_FOR_CLIPBOARD("A");

    RUN("copy" << "DATA" << "B", "true\n");
    WAIT_FOR_CLIPBOARD2("B", "DATA");

    RUN( Args() << "copy"
         << "DATA3" << "C"
         << "DATA4" << "D"
         , "true\n" );
    WAIT_FOR_CLIPBOARD2("C", "DATA3");
    WAIT_FOR_CLIPBOARD2("D", "DATA4");
}

void Tests::commandClipboard()
{
    TEST( m_test->setClipboard("A") );
    WAIT_FOR_CLIPBOARD("A");
    RUN("clipboard", "A");

    TEST( m_test->setClipboard("B", "DATA") );
    WAIT_FOR_CLIPBOARD2("B", "DATA");
    RUN("clipboard" << "DATA", "B");
}

void Tests::commandHasClipboardFormat()
{
    TEST( m_test->setClipboard("B", "DATA") );
    WAIT_FOR_CLIPBOARD2("B", "DATA");
    WAIT_ON_OUTPUT("hasClipboardFormat('DATA')", "true\n");
    WAIT_ON_OUTPUT("hasClipboardFormat('text/plain')", "false\n");
}

void Tests::commandEdit()
{
    SKIP_ON_ENV("COPYQ_TESTS_SKIP_COMMAND_EDIT");

    RUN("config" << "editor" << "", "\n");

    // Edit new item.
    RUN("edit", "");
    RUN("keys" << ":LINE 1" << "F2", "");
    RUN("read" << "0", "LINE 1");

    // Edit existing item.
    RUN("edit" << "0", "");
    RUN("keys" << "END" << "ENTER" << ":LINE 2" << "F2", "");
    RUN("read" << "0", "LINE 1\nLINE 2");
}

void Tests::commandGetSetCurrentPath()
{
    RUN("currentPath", QDir::currentPath() + "\n");

    const auto newPath = QDir::homePath().toUtf8();

    TEST( m_test->runClient(
              Args("eval") << "currentPath(input()); print(currentPath())",
              newPath, newPath) );

    TEST( m_test->runClient(
              Args("eval") << "currentPath(input()); print(Dir().absolutePath())",
              newPath, newPath) );
}

void Tests::commandSelectItems()
{
    RUN("add" << "C" << "B" << "A", "");

    RUN("selectItems" << "1", "true\n");
    RUN("testSelected", QString(clipboardTabName) + " 1 1\n");

    RUN("selectItems" << "0" << "2", "true\n");
    RUN("testSelected", QString(clipboardTabName) + " 2 0 2\n");

    const auto tab = testTab(1);
    const auto args = Args("tab") << tab;
    RUN(args << "add" << "C" << "B" << "A", "");
    RUN(args << "selectItems" << "1" << "2", "true\n");
    RUN("testSelected", QString(clipboardTabName) + " 2 0 2\n");
    RUN("setCurrentTab" << tab, "");
    RUN("testSelected", tab + " 2 1 2\n");
}

void Tests::commandsExportImport()
{
    const auto tab1 = testTab(1);
    RUN("tab" << tab1 << "add" << "C" << "B" << "A", "");

    const auto tab2 = testTab(2);
    RUN("tab" << tab2 << "add" << "3" << "2", "");

    RUN("config" << "maxitems" << "3", "3\n");
    RUN("config" << "editor" << "EDITOR1 %1", "EDITOR1 %1\n");

    QTemporaryFile tmp;
    QVERIFY(tmp.open());
    tmp.close();
    const auto fileName = tmp.fileName();

    RUN("exportData" << fileName, "");

    RUN("config" << "maxitems" << "1", "1\n");
    RUN("config" << "editor" << "EDITOR2 %1", "EDITOR2 %1\n");
    RUN("removetab" << tab1, "");
    RUN("tab" << tab2 << "add" << "1", "");

    RUN("importData" << fileName, "");

    RUN("config" << "maxitems", "3\n");
    RUN("config" << "editor", "EDITOR1 %1\n");

    const auto suffix = " (1)";
    RUN("tab",
        QString(clipboardTabName) + "\n"
        + tab2 + "\n"
        + clipboardTabName + suffix + "\n"
        + tab1 + "\n"
        + tab2 + suffix + "\n");

    RUN("tab" << tab1 << "read" << "0" << "1" << "2", "A\nB\nC");
    RUN("tab" << tab2 + suffix << "read" << "0" << "1", "2\n3");
    RUN("tab" << tab2 << "read" << "0", "1");
}

void Tests::commandsGetSetCommands()
{
    RUN("commands().length", "0\n");

    RUN("setCommands([{name: 'test', cmd: 'copyq help'}])", "");
    RUN("commands().length", "1\n");
    RUN("commands()[0].name", "test\n");
    RUN("commands()[0].cmd", "copyq help\n");
    RUN("commands()[0].enable", "true\n");

    RUN("setCommands(commands())", "");
    RUN("commands().length", "1\n");
    RUN("commands()[0].name", "test\n");
    RUN("commands()[0].enable", "true\n");
}

void Tests::commandsImportExportCommands()
{
   const QString commands =
           R"('
           [Commands]
           1\Name=Test 1
           2\Name=Test 2
           size=2
           ')";
   RUN("eval" << "importCommands(arguments[1]).length" << "--" << commands, "2\n");
   RUN("eval" << "importCommands(arguments[1])[0].name" << "--" << commands, "Test 1\n");
   RUN("eval" << "importCommands(arguments[1])[1].name" << "--" << commands, "Test 2\n");

   RUN("importCommands(exportCommands([{},{}])).length", "2\n");
   RUN("importCommands(exportCommands([{},{name: 'Test 2'}]))[1].name", "Test 2\n");
}

void Tests::commandsImportExportCommandsFixIndentation()
{
    {
        const QString commands =
                "[Command]\n"
                "Command=\"\n    1\n    2\n    3\"";
        RUN("eval" << "importCommands(arguments[1])[0].cmd" << "--" << commands, "1\n2\n3\n");
    }

    {
        const QString commands =
                "[Command]\n"
                "Command=\"\r\n    1\r\n    2\r\n    3\"";
        RUN("eval" << "importCommands(arguments[1])[0].cmd" << "--" << commands, "1\n2\n3\n");
    }
}

void Tests::commandScreenshot()
{
    RUN("screenshot().size() > 0", "true\n")
}

void Tests::commandNotification()
{
    const auto script = R"(
        notification(
            '.title', 'title',
            '.message', 'message',
            '.time', 1000,
            '.id', 'test',
            '.icon', 'copyq',
            '.button', 'OK', '', '',
            '.button', 'CANCEL', '', ''
        )
        )";
    RUN(script, "");

    RUN_EXPECT_ERROR_WITH_STDERR(
                "notification('.message', 'message', 'BAD')", CommandException, "Unknown argument: BAD");
}

void Tests::commandIcon()
{
    RUN("iconColor", QByteArray(defaultSessionColor) + "\n");

    RUN("iconColor" << "red", "");
    RUN("iconColor", "#ff0000\n");

    RUN_EXPECT_ERROR("iconColor" << "BAD_COLOR_NAME", CommandException);
    RUN("iconColor", "#ff0000\n");

    RUN("iconColor" << "", "");
    RUN("iconColor", QByteArray(defaultSessionColor) + "\n");

    RUN("iconColor" << defaultSessionColor, "");
    RUN("iconColor", QByteArray(defaultSessionColor) + "\n");
}

void Tests::commandIconTag()
{
    RUN("iconTag", "\n");

    RUN("iconTag" << "TEST", "");
    RUN("iconTag", "TEST\n");

    RUN("iconTag" << "", "");
    RUN("iconTag", "\n");
}

void Tests::commandIconTagColor()
{
    RUN("iconTagColor", QByteArray(defaultTagColor) + "\n");

    RUN("iconTagColor" << "red", "");
    RUN("iconTagColor", "#ff0000\n");

    RUN_EXPECT_ERROR("iconTagColor" << "BAD_COLOR_NAME", CommandException);
    RUN("iconTagColor", "#ff0000\n");

    RUN("iconTagColor" << defaultTagColor, "");
    RUN("iconTagColor", QByteArray(defaultTagColor) + "\n");
}

void Tests::commandLoadTheme()
{
    RUN_EXPECT_ERROR_WITH_STDERR(
        "loadTheme" << "a non-existent file", CommandException, "ScriptError: Failed to read theme");
    RUN_EXPECT_ERROR_WITH_STDERR(
        "loadTheme" << ".", CommandException, "ScriptError: Failed to read theme");

    {
        QTemporaryFile tmp;
        QVERIFY(tmp.open());
        tmp.write("INVALID INI FILE");
        tmp.close();
        RUN_EXPECT_ERROR_WITH_STDERR(
            "loadTheme" << tmp.fileName(), CommandException, "ScriptError: Failed to parse theme");
    }

    {
        QTemporaryFile tmp;
        QVERIFY(tmp.open());
        tmp.write("[General]");
        tmp.close();
        RUN("loadTheme" << tmp.fileName(), "");
    }

    // Verify default stylesheets - if there is a syntax error,
    // application prints a warning which should be captured by tests.
    {
        QTemporaryFile tmp;
        QVERIFY(tmp.open());
        tmp.write("[General]\nstyle_main_window=true");
        tmp.close();
        RUN("loadTheme" << tmp.fileName(), "");
    }
}

void Tests::commandDateString()
{
    const auto dateFormat = "TEST:yyyy-MM-dd";
    const auto dateTime = QDateTime::currentDateTime();
    const auto today = dateTime.toString(dateFormat);
    RUN("dateString" << dateFormat, today + "\n");
}

void Tests::commandAfterMilliseconds()
{
    const QString script = "afterMilliseconds(100, function(){ print('TEST'); abort(); });";
    RUN(script, "");
    RUN(script + "sleep(1)", "");
    RUN(script + "sleep(200)", "TEST");
}

void Tests::commandAsync()
{
    RUN("afterMilliseconds(0, function() { print(currentItem()); abort(); }); dialog()", "-1");
}

void Tests::commandFilter()
{
    RUN("filter", "\n");
    RUN("filter" << "test", "");
    RUN("filter", "test\n");
    RUN("filter" << "", "");
    RUN("filter", "\n");

    // Empty filter() after ESC.
    RUN("filter" << "test", "");
    RUN("filter", "test\n");
    RUN("keys" << "ESC", "");
    RUN("filter", "\n");
}

void Tests::commandMimeTypes()
{
    RUN("print(mimeText)", mimeText);
    RUN("print(mimeHtml)", mimeHtml);
    RUN("print(mimeUriList)", mimeUriList);
    RUN("print(mimeWindowTitle)", mimeWindowTitle);
    RUN("print(mimeItems)", mimeItems);
    RUN("print(mimeItemNotes)", mimeItemNotes);
    RUN("print(mimeOwner)", mimeOwner);
    RUN("print(mimeClipboardMode)", mimeClipboardMode);
    RUN("print(mimeCurrentTab)", mimeCurrentTab);
    RUN("print(mimeSelectedItems)", mimeSelectedItems);
    RUN("print(mimeCurrentItem)", mimeCurrentItem);
    RUN("print(mimeHidden)", mimeHidden);
    RUN("print(mimeShortcut)", mimeShortcut);
    RUN("print(mimeColor)", mimeColor);
    RUN("print(mimeOutputTab)", mimeOutputTab);
}

void Tests::commandUnload()
{
    // Failure if tab is visible.
    RUN("unload", "");

    const auto tab = testTab(1);

    // Success if tab doesn't exist.
    RUN("unload" << tab, tab + "\n");

    RUN("tab" << tab << "add" << "A", "");
    // Success if tab is not visible and editor is not open.
    RUN("unload" << tab, tab + "\n");

    RUN("tab" << tab << "add" << "B", "");
    RUN("unload", tab + "\n");
    // Success if tab is not loaded.
    RUN("unload", tab + "\n");

    // Success if tab does not exist.
    RUN("unload" << "missing-tab", "missing-tab\n");
}

void Tests::commandForceUnload()
{
    RUN("forceUnload", "");
    RUN_EXPECT_ERROR_WITH_STDERR("add" << "A", CommandException, "ScriptError: Invalid tab");

    RUN("keys" << clipboardBrowserRefreshButtonId << "Space", "");
    RUN("add" << "A", "");

    const auto tab = testTab(1);
    RUN("tab" << tab << "add" << "A", "");

    RUN("forceUnload" << tab, "");

    RUN("setCurrentTab" << tab, "");
    RUN_EXPECT_ERROR_WITH_STDERR(
        "tab" << tab << "add" << "B", CommandException, "ScriptError: Invalid tab");

    RUN("keys" << clipboardBrowserRefreshButtonId << "Space", "");
    RUN("add" << "B", "");
}

void Tests::commandServerLogAndLogs()
{
    const QByteArray data1 = generateData();
    QRegularExpression re("CopyQ Note \\[[^]]+\\] <Server-[0-9]+>: " + QRegularExpression::escape(data1));

    QByteArray stdoutActual;
    QByteArray stderrActual;

    QCOMPARE( run(Args("logs"), &stdoutActual, &stderrActual), 0 );
    QVERIFY2( testStderr(stderrActual), stderrActual );
    QVERIFY( !stdoutActual.isEmpty() );
    QVERIFY( !QString::fromUtf8(stdoutActual).contains(re) );

    RUN("serverLog" << data1, "");

    QCOMPARE( run(Args("logs"), &stdoutActual, &stderrActual), 0 );
    QVERIFY2( testStderr(stderrActual), stderrActual );
    QVERIFY( !stdoutActual.isEmpty() );
    QVERIFY( QString::fromUtf8(stdoutActual).contains(re) );
}

void Tests::classByteArray()
{
    RUN("ByteArray('test')", "test");
    RUN("b = ByteArray('0123'); b.chop(2); b", "01");
    RUN("ByteArray('0123').equals(ByteArray('0123'))", "true\n");
    RUN("ByteArray('0123').left(3)", "012");
    RUN("ByteArray('0123').mid(1, 2)", "12");
    RUN("ByteArray('0123').mid(1)", "123");
    RUN("ByteArray('0123').right(3)", "123");
    RUN("ByteArray(' 01  23 ').simplified()", "01 23");
    RUN("ByteArray('0123').toBase64()", "MDEyMw==");
    RUN("ByteArray('ABCd').toLower()", "abcd");
    RUN("ByteArray('abcD').toUpper()", "ABCD");
    RUN("ByteArray(' 01  23 ').trimmed()", "01  23");
    RUN("b = ByteArray('0123'); b.truncate(2); b", "01");
    RUN("ByteArray('0123').toLatin1String() == '0123'", "true\n");
    RUN("ByteArray('0123').valueOf() == '0123'", "true\n");
    RUN("ByteArray(8).size()", "8\n");
    RUN("b = ByteArray(); b.length = 10; b.length", "10\n");
}

void Tests::classFile()
{
    RUN("var f = new File('/copyq_missing_file'); f.exists()", "false\n");
}

void Tests::classDir()
{
    RUN("var d = new Dir('/missing_directory/')"
        "; d.exists()"
        , "false\n"
        );

    const auto home = QDir::homePath();
    RUN("Dir().homePath()", home + "\n");
    RUN("Dir().home().path()", home + "\n");

    const auto root = QDir::rootPath();
    RUN("Dir().rootPath()", root + "\n");
    RUN("Dir().root().path()", root + "\n");

    const auto temp = QDir::tempPath();
    RUN("Dir().tempPath()", temp + "\n");
    RUN("Dir().temp().path()", temp + "\n");

    RUN("Dir().separator()", QString(QDir::separator()) + "\n");

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QDir dir(tmpDir.path());
    const auto path = dir.path();
    const auto args = QString("var d = new Dir('%1')").arg(path);

    RUN(args << "d.exists()", "true\n");
    RUN(args << "d.isReadable()", "true\n");
    RUN(args << "d.isAbsolute()", "true\n");
    RUN(args << "d.isRelative()", "false\n");
    RUN(args << "d.absolutePath()", path + "\n");
    RUN(args << "d.path()", path + "\n");
    RUN(args << "d.makeAbsolute()", "true\n");

    RUN(args << "d.mkdir('test')", "true\n");
    QVERIFY( QDir(dir.filePath("test")).exists() );
    RUN(args << "d.exists('test')", "true\n");

    RUN(args << "d.mkpath('a/b/c')", "true\n");
    QVERIFY( QDir(dir.filePath("a/b/c")).exists() );
    RUN(args << "d.exists('a/b/c')", "true\n");
    RUN(args << "d.filePath('a/b/c')", dir.filePath("a/b/c") + "\n");
    RUN(args << "d.relativeFilePath('" + path + "/a/b/c')", "a/b/c\n");
    RUN("Dir('" + path + "/test/../a//b/c/..').canonicalPath()", dir.filePath("a/b") + "\n");
    RUN(args << "d.setPath('" + path + "/a/b/c')" << "d.path()", dir.filePath("a/b/c") + "\n");

    RUN(args << "d.cd('a')" << "d.cd('b')", "true\n");
    RUN(args << "d.cd('x')", "false\n");
    RUN(args << "d.cd('a')" << "d.cd('b')" << "d.path()", dir.filePath("a/b") + "\n");
    RUN(args << "d.cd('a')" << "d.cd('..')" << "d.path()", path + "\n");
    RUN(args << "d.cd('a')" << "d.cdUp()" << "d.path()", path + "\n");

    RUN(args << "d.count()", QString("%1\n").arg(dir.count()));
    RUN(args << "d.dirName()", QString("%1\n").arg(dir.dirName()));

    RUN(args << "d.match(['a*'], 'test')", "false\n");
    RUN(args << "d.match(['t*'], 'test')", "true\n");
    RUN(args << "d.entryList()", ".\n..\na\ntest\n");
    RUN(args << "d.entryList(['t*'])", "test\n");
    RUN(args << "d.entryList(['t?st', 'a*'])", "a\ntest\n");

    RUN(args << "d.setNameFilters(['t?st', 'a*'])" << "d.nameFilters()", "t?st\na*\n");

    QFile f(dir.filePath("test.txt"));
    QVERIFY( f.open(QIODevice::WriteOnly) );
    f.close();
    RUN(args << "d.exists('test.txt')", "true\n");
    RUN(args << "d.absoluteFilePath('test.txt')", dir.filePath("test.txt") + "\n");
    RUN(args << "d.rename('test.txt', 'test2.txt')", "true\n");
    RUN(args << "d.exists('test2.txt')", "true\n");
    RUN(args << "d.remove('test2.txt')", "true\n");
    RUN(args << "d.exists('test2.txt')", "false\n");

    RUN(args << "d.rmdir('test')", "true\n");
    RUN(args << "d.exists('test')", "false\n");

    RUN("Dir().cleanPath('/a//b/../c/')", QDir::cleanPath("/a//b/../c/") + "\n");
}

void Tests::classTemporaryFile()
{
    RUN("var f = new TemporaryFile(); f.open()", "true\n");

    QByteArray err;

    for ( const auto autoRemove : {true, false} ) {
        QByteArray fileName;
        const auto script =
                QString(R"(
                        var f = new TemporaryFile()
                        if (!f.open())
                            throw 'Failed to open temporary file'

                        f.setAutoRemove(%1)
                        print(f.fileName())
                        )").arg(autoRemove);
        run(Args() << script, &fileName, &err);
        QVERIFY2( testStderr(err), err );

        QFile f( QString::fromUtf8(fileName) );
        QVERIFY( f.exists() != autoRemove );

        if (!autoRemove)
            f.remove();
    }

    QByteArray fileName;
    const auto scriptWrite = R"(
        var f = new TemporaryFile()
        if (!f.open())
            throw 'Failed to open temporary file'

        if (!f.write('LINE'))
            throw 'Failed to write to temporary file'

        f.setAutoRemove(false)
        print(f.fileName())
        )";
    run(Args() << scriptWrite, &fileName);
    QVERIFY2( testStderr(err), err );
    QVERIFY( QFile::exists(QString::fromUtf8(fileName)) );

    QByteArray out;
    const auto scriptRead = R"(
        var f = new File(str(input()))
        if (!f.openReadOnly())
            throw 'Failed to open file'

        print(''
            + ' exists()=' + str(f.exists())
            + ' isOpen()=' + str(f.isOpen())
            + ' isReadable()=' + str(f.isReadable())
            + ' isWritable()=' + str(f.isWritable())
            + ' size()=' + str(f.size())
            + ' readAll()=' + str(f.readAll())
            + ' atEnd()=' + str(f.atEnd())
            + ' seek(0)=' + str(f.seek(0))
            + ' read(1)=' + str(f.read(1))
            + ' pos()=' + str(f.pos())
            + ' peek(1)=' + str(f.peek(1))
            + ' readLine()=' + str(f.readLine())
        )
        )";
    const QByteArray expectedOut =
        " exists()=true"
        " isOpen()=true"
        " isReadable()=true"
        " isWritable()=false"
        " size()=4"
        " readAll()=LINE"
        " atEnd()=true"
        " seek(0)=true"
        " read(1)=L"
        " pos()=1"
        " peek(1)=I"
        " readLine()=INE";
    run(Args() << scriptRead, &out, &err, fileName);
    QVERIFY2( testStderr(err), err );
    QCOMPARE(out, expectedOut);

    const auto scriptRemove = R"(
        var f = new File(str(input()))
        if (!f.remove())
            throw 'Failed to remove file'
        )";
    run(Args() << scriptRemove, &out, &err, fileName);
    QVERIFY2( testStderr(err), err );
    QCOMPARE(QByteArray(), out);
    QVERIFY( !QFile::exists(QString::fromUtf8(fileName)) );

    RUN("TemporaryFile().autoRemove()", "true\n");
    RUN("TemporaryFile().fileTemplate()", QDir::temp().filePath("copyq.test.XXXXXX") + "\n");
}

void Tests::calledWithBadInstance()
{
    RUN_EXPECT_ERROR_WITH_STDERR(
        "f=ByteArray().size; f()", CommandException, "ScriptError: Invalid ByteArray object");
    RUN_EXPECT_ERROR_WITH_STDERR(
        "f=Dir().count; f()", CommandException, "ScriptError: Invalid Dir object");
    RUN_EXPECT_ERROR_WITH_STDERR(
        "f=File().open; f()", CommandException, "ScriptError: Invalid File object");
    RUN_EXPECT_ERROR_WITH_STDERR(
        "f=TemporaryFile().autoRemove; f()", CommandException, "ScriptError: Invalid TemporaryFile object");
}

void Tests::pipingCommands()
{
    const auto tab = testTab(1);
    const Args args = Args("tab") << tab << "separator" << ",";

    RUN(args << "action"
        << "copyq print HELLO | copyq print(str(input()).toLowerCase())", "");
    WAIT_ON_OUTPUT(args << "read" << "0" << "1", "hello,");

    RUN(args << "action"
        << "copyq print TEST"
        " | copyq 'print(str(input()) + 1)'"
        " | copyq 'print(str(input()) + 2)'"
        " | copyq 'print(str(input()) + 3)'"
        , "");
    WAIT_ON_OUTPUT(args << "read" << "0" << "1", "TEST123,hello");
}

void Tests::chainingCommands()
{
    const auto tab1 = testTab(1);
    RUN("tab" << tab1 << "add" << "C" << "B" << "A", "");
    RUN("tab" << tab1 << "separator" << " " << "read" << "0" << "1" << "2", "A B C");
    RUN("tab" << tab1 << "separator" << "\\t" << "showAt" << "read" << "0" << "1" << "2", "A\tB\tC");

    // Chain functions without arguments.
    RUN("enable" << "disable" << "monitoring", "false\n");
    RUN("if (!monitoring()) enable" << "monitoring", "true\n");

    // Don't treat arguments after "eval" as functions.
    RUN("eval" << "arguments[1]" << "TEST", "TEST");
    RUN("eval" << "arguments[1]" << "--" << "TEST", "TEST");
}

void Tests::configMaxitems()
{
    RUN("config" << "maxitems" << "3", "3\n");
    RUN("add" << "A" << "B" << "C", "");
    RUN("add" << "D", "");
    RUN("separator" << " " << "read" << "0" << "1" << "2", "D C B");
    RUN("size", "3\n");

    RUN("add" << "E" << "F", "");
    RUN("separator" << " " << "read" << "0" << "1" << "2", "F E D");
    RUN("size", "3\n");

    RUN("config" << "maxitems" << "2", "2\n");
    RUN("separator" << " " << "read" << "0" << "1", "F E");
    RUN("size", "2\n");

    // Ading too many items fails.
    RUN_EXPECT_ERROR("add" << "1" << "2" << "3", CommandException);
    RUN("separator" << " " << "read" << "0" << "1", "F E");
    RUN("size", "2\n");

    // Single item in tabs.
    RUN("config" << "maxitems" << "1", "1\n");
    RUN("separator" << " " << "read" << "0", "F");
    RUN("size", "1\n");

    RUN("add" << "G", "");
    RUN("separator" << " " << "read" << "0", "G");
    RUN("size", "1\n");

    RUN("write" << "1" << "text/plain" << "H", "");
    RUN("separator" << " " << "read" << "0", "H");
    RUN("size", "1\n");

    // No items in tabs.
    RUN("config" << "maxitems" << "0", "0\n");
    RUN("size", "0\n");

    RUN_EXPECT_ERROR("add" << "1", CommandException);
    RUN_EXPECT_ERROR("write" << "1", CommandException);
    RUN("size", "0\n");

    // Invalid value.
    RUN("config" << "maxitems" << "-99", "0\n");
    RUN("size", "0\n");
}

void Tests::keysAndFocusing()
{
#ifdef Q_OS_MAC
    SKIP("FIXME: currentWindowTitle() returns different window titles on OS X.");
#endif

    RUN("disable", "");
    RUN("keys" << clipboardBrowserId << "CTRL+T", "");

#ifdef Q_OS_WIN
    WAIT_ON_OUTPUT("currentWindowTitle", "New Tab - CopyQ-TEST\n");
#else
    WAIT_ON_OUTPUT("currentWindowTitle", "New Tab — CopyQ-TEST\n");
#endif

    RUN("keys" << tabDialogLineEditId << "ESC", "");
    WAIT_ON_OUTPUT("currentWindowTitle", "CopyQ-TEST\n");
    RUN("enable", "");
}

void Tests::selectItems()
{
    const auto tab = QString(clipboardTabName);
    RUN("add" << "C" << "B" << "A", "");

    RUN("keys" << "RIGHT" << "SHIFT+DOWN" << "SHIFT+DOWN", "");
    RUN("testSelected", tab + " 2 0 1 2\n");

    RUN("keys" << "SHIFT+UP", "");
    RUN("testSelected", tab + " 1 0 1\n");

    RUN("keys" << "END", "");
    RUN("testSelected", tab + " 2 2\n");

    RUN("keys" << "SHIFT+UP", "");
    RUN("testSelected", tab + " 1 1 2\n");

    RUN("keys" << "CTRL+A", "");
    RUN("testSelected", tab + " 1 0 1 2\n");
}

void Tests::moveItems()
{
    const auto tab = QString(clipboardTabName);
    const auto args = Args() << "separator" << " ";
    RUN(args << "add" << "C" << "B" << "A", "");

    // move item one down
    RUN(args << "keys" << "RIGHT" << "CTRL+DOWN", "");
    RUN(args << "read" << "0" << "1" << "2", "B A C");
    RUN(args << "testSelected", tab + " 1 1\n");

    // move items to top
    RUN(args << "keys" << "SHIFT+DOWN" << "CTRL+HOME", "");
    RUN(args << "read" << "0" << "1" << "2", "A C B");
    RUN(args << "testSelected", tab + " 1 0 1\n");
}

void Tests::deleteItems()
{
    const auto tab = QString(clipboardTabName);
    const auto args = Args() << "separator" << ",";
    RUN(args << "add" << "C" << "B" << "A", "");

    // delete first item
    RUN(args << "keys" << "RIGHT" << m_test->shortcutToRemove(), "");
    RUN(args << "read" << "0" << "1" << "2", "B,C,");
    RUN(args << "testSelected", tab + " 0 0\n");

    // select all and delete
    RUN(args << "keys" << "CTRL+A" << m_test->shortcutToRemove(), "");
    RUN(args << "size", "0\n");
}

void Tests::searchItems()
{
    RUN("add" << "a" << "b" << "c", "");
    RUN("keys" << ":b" << "TAB", "");
    RUN("testSelected", QString(clipboardTabName) + " 1 1\n");
}

void Tests::searchItemsAndSelect()
{
    RUN("add" << "xx2" << "a" << "xx" << "c", "");
    RUN("keys" << ":xx" << filterEditId, "");

    RUN("keys" << filterEditId << "DOWN" << clipboardBrowserId, "");
    RUN("testSelected", QString(clipboardTabName) + " 1 1\n");

    RUN("keys" << clipboardBrowserId << "DOWN" << clipboardBrowserId, "");
    RUN("testSelected", QString(clipboardTabName) + " 3 3\n");
}

void Tests::searchRowNumber()
{
    RUN("add" << "d2" << "c" << "b2" << "a", "");
    RUN("keys" << ":2" << "TAB", "");
    RUN("testSelected", QString(clipboardTabName) + " 2 2\n");
    RUN("keys" << "CTRL+A", "");
    RUN("testSelected", QString(clipboardTabName) + " 2 1 2 3\n");
}

void Tests::copyItems()
{
    const auto tab = QString(clipboardTabName);
    RUN("add" << "C" << "B" << "A", "");

    // Select and copy all items.
    RUN("keys" << "CTRL+A" << keyNameFor(QKeySequence::Copy), "");

    // This seems to be required on Windows.
    WAIT_ON_OUTPUT("clipboard", "A\nB\nC");

    // Paste all items.
    RUN("keys" << keyNameFor(QKeySequence::Paste), "");
    RUN("separator" << " " << "read" << "0" << "1" << "2" << "3" << "4" << "5", "A B C A B C");
    RUN("size", "6\n");
}

void Tests::createTabDialog()
{
    const auto tab1 = testTab(1);
    RUN("keys"
        << clipboardBrowserId << "CTRL+T"
        << tabDialogLineEditId << ":" + tab1 << "ENTER", "");
    RUN("testSelected", tab1 + "\n");
}

void Tests::copyPasteCommands()
{
    const QByteArray commands =
            "[Commands]\n"
            "1\\Name=Test 1\n"
            "2\\Name=Test 2\n"
            "size=2";

    RUN("keys" << clipboardBrowserId << "F6", "");
    TEST( m_test->setClipboard(commands) );
    RUN("keys" << commandDialogListId << keyNameFor(QKeySequence::Paste), "");

    TEST( m_test->setClipboard(QByteArray()) );
    RUN("keys" << commandDialogListId << keyNameFor(QKeySequence::Copy), "");
    WAIT_FOR_CLIPBOARD(commands);

    RUN("keys" << commandDialogListId << "Enter" << clipboardBrowserId, "");
    RUN("commands().length", "2\n")
}

void Tests::editItems()
{
    RUN("config" << "edit_ctrl_return" << "true", "true\n");

    RUN("add" << "Line 4" << "Line 1", "");

    RUN("keys"
        << clipboardBrowserId << "F2"
        << editorId << "END" << "ENTER" << ":Line 2" << "F2", "");
    RUN("read" << "0", "Line 1\nLine 2");

    RUN("keys"
        << clipboardBrowserId << "DOWN" << "F2"
        << editorId << "HOME" << ":Line 3" << "ENTER" << "F2", "");
    RUN("read" << "1", "Line 3\nLine 4");
    RUN("read" << "0", "Line 1\nLine 2");
}

void Tests::createNewItem()
{
    RUN("config" << "edit_ctrl_return" << "true", "true\n");

    RUN("keys" << "CTRL+N" << editorId << ":Line 1" << "ENTER" << ":Line 2" << "F2", "");
    RUN("read" << "0", "Line 1\nLine 2");

    RUN("keys" << "CTRL+N" << editorId << ":Line 3" << "ENTER" << ":Line 4" << "F2", "");
    RUN("read" << "0", "Line 3\nLine 4");
}

void Tests::editNotes()
{
    RUN("add" << "B" << "A", "");

    RUN("keys" << "SHIFT+F2" << ":A Note" << "F2", "");
    RUN("read" << mimeText << "0" << mimeItemNotes << "0" << "F2", "A\nA Note");
    RUN("read" << mimeText << "1" << mimeItemNotes << "1" << "F2", "B\n");

    RUN("keys" << "DOWN", "");

    RUN("keys" << "SHIFT+F2" << ":B Note" << "F2", "");
    RUN("read" << mimeText << "1" << mimeItemNotes << "1" << "F2", "B\nB Note");
    RUN("read" << mimeText << "0" << mimeItemNotes << "0" << "F2", "A\nA Note");
}

void Tests::toggleClipboardMonitoring()
{
    const QByteArray data1 = generateData();
    TEST( m_test->setClipboard(data1) );
    RUN("clipboard", data1);
    WAIT_ON_OUTPUT("read" << "0", data1);

    RUN("disable", "");
    RUN("monitoring", "false\n");

    const QByteArray data2 = generateData();
    TEST( m_test->setClipboard(data2) );
    RUN("clipboard", data2);
    WAIT_ON_OUTPUT("read" << "0", data1);

    RUN("enable", "");
    RUN("monitoring", "true\n");

    const QByteArray data3 = generateData();
    TEST( m_test->setClipboard(data3) );
    RUN("clipboard", data3);
    WAIT_ON_OUTPUT("read" << "0", data3);
}

void Tests::clipboardToItem()
{
    TEST( m_test->setClipboard("TEXT1") );
    RUN("clipboard", "TEXT1");
    WAIT_ON_OUTPUT("read" << "0", "TEXT1");

    TEST( m_test->setClipboard("DATA1", "DATA") );
    WAIT_ON_OUTPUT("clipboard" << "DATA", "DATA1");

    // Unicode test.
    const auto test = QString::fromUtf8(QByteArray("Zkouška s různými českými znaky!"));
    const auto bytes = test.toUtf8();
    TEST( m_test->setClipboard(bytes) );
    RUN("clipboard", bytes);
    WAIT_ON_OUTPUT("read" << "0", bytes);
}

void Tests::itemToClipboard()
{
    RUN("add" << "TESTING2" << "TESTING1", "");
    RUN("read" << "0" << "1", "TESTING1\nTESTING2");

    RUN("select" << "0", "");

    WAIT_FOR_CLIPBOARD("TESTING1");
    RUN("clipboard", "TESTING1");

    // select second item and move to top
    RUN("config" << "move" << "true", "true\n");
    RUN("select" << "1", "");
    RUN("read" << "0" << "1", "TESTING2\nTESTING1");

    WAIT_FOR_CLIPBOARD("TESTING2");
    RUN("clipboard", "TESTING2");

    // select without moving
    RUN("config" << "move" << "0", "false\n");
    RUN("select" << "1", "");
    RUN("read" << "0" << "1", "TESTING2\nTESTING1");

    WAIT_FOR_CLIPBOARD("TESTING1");
    RUN("clipboard", "TESTING1");
}

void Tests::tabAdd()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab << "separator" << " ";

    QVERIFY( !hasTab(tab) );
    RUN(args, "");
    RUN(args << "size", "0\n");
    RUN(args << "add" << "ghi" << "def" << "abc", "");
    QVERIFY( hasTab(tab) );

    // Restart server.
    TEST( m_test->stopServer() );
    TEST( m_test->startServer() );

    QVERIFY( hasTab(tab) );

    RUN(args << "size", "3\n");
    RUN(args << "read" << "0" << "1" << "2", "abc def ghi");
}

void Tests::tabRemove()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab << "separator" << " ";

    RUN(args << "add" << "", "");
    QVERIFY( hasTab(tab) );
    RUN(Args() << "removetab" << tab, "");
    QVERIFY( !hasTab(tab) );

    RUN_EXPECT_ERROR("removetab" << tab, CommandException);
}

void Tests::tabIcon()
{
    const QString tab = testTab(1);
    const QString icon = ":/images/icon";

    RUN("tab" << tab << "add" << "", "");
    RUN("tabIcon" << tab, "\n");
    RUN("tabicon" << tab << icon, "");
    RUN("tabIcon" << tab, icon + "\n");
    RUN("tabIcon" << tab << "", "");
    RUN("tabicon" << tab, "\n");
}

void Tests::action()
{
    const Args args = Args("tab") << testTab(1);
    const Args argsAction = Args(args) << "action";
    const QString action = QString("copyq %1 %2").arg(args.join(" "));

    // action with size
    RUN(argsAction << action.arg("size") << "", "");
    WAIT_ON_OUTPUT(args << "size", "1\n");
    RUN(args << "read" << "0", "0\n");

    // action with size
    RUN(argsAction << action.arg("size") << "", "");
    WAIT_ON_OUTPUT(args << "size", "2\n");
    RUN(args << "read" << "0", "1\n");

    // action with eval print
    RUN(argsAction << action.arg("eval 'print(\"A,B,C\")'") << "", "");
    WAIT_ON_OUTPUT(args << "size", "3\n");
    RUN(args << "read" << "0", "A,B,C");

    // action with read and comma separator for new items
    RUN(argsAction << action.arg("read 0") << ",", "");
    WAIT_ON_OUTPUT(args << "size", "6\n");
    RUN(args << "read" << "0" << "1" << "2", "C\nB\nA");
}

void Tests::insertRemoveItems()
{
    const Args args = Args("tab") << testTab(1) << "separator" << ",";

    RUN(args << "add" << "ghi" << "abc", "");
    RUN(args << "insert" << "1" << "def", "");
    RUN(args << "read" << "0" << "1" << "2" << "3" << "4", "abc,def,ghi,,");

    RUN(args << "insert" << "0" << "012", "");
    RUN(args << "read" << "0" << "1" << "2" << "3" << "4", "012,abc,def,ghi,");

    RUN(args << "remove" << "0" << "2", "");
    RUN(args << "read" << "0" << "1" << "2" << "3" << "4", "abc,ghi,,,");

    QByteArray in("ABC");
    QCOMPARE( run(Args(args) << "insert" << "1" << "-", nullptr, nullptr, in), 0);
    RUN(args << "read" << "0" << "1" << "2" << "3" << "4", "abc,ABC,ghi,,");
}

void Tests::renameTab()
{
    const QString tab1 = testTab(1);
    const QString tab2 = testTab(2);

    RUN("tab" << tab1 << "add" << "ghi" << "def" << "abc", "");

    RUN("renametab" << tab1 << tab2, "");
    RUN("tab" << tab2 << "size", "3\n");
    RUN("tab" << tab2 << "read" << "0" << "1" << "2", "abc\ndef\nghi");
    QVERIFY( !hasTab(tab1) );

    // Rename non-existing tab.
    RUN_EXPECT_ERROR("renametab" << tab1 << tab2, CommandException);

    // Rename to same name.
    RUN_EXPECT_ERROR("renametab" << tab2 << tab2, CommandException);

    // Rename to empty name.
    RUN_EXPECT_ERROR("renametab" << tab2 << "", CommandException);

    // Rename to existing tab.
    RUN_EXPECT_ERROR("renametab" << tab2 << clipboardTabName, CommandException);

    QVERIFY( !hasTab(tab1) );
    QVERIFY( hasTab(tab2) );

    RUN("renametab" << tab2 << tab1, "");
    RUN("tab" << tab1 << "read" << "0" << "1" << "2", "abc\ndef\nghi");

    QVERIFY( hasTab(tab1) );
    QVERIFY( !hasTab(tab2) );
}

void Tests::importExportTab()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab << "separator" << " ";

    RUN(args << "add" << "ghi" << "def" << "abc", "");

    QTemporaryFile tmp;
    QVERIFY(tmp.open());
    RUN(args << "exporttab" << tmp.fileName(), "");

    RUN("removetab" << tab, "");
    QVERIFY( !hasTab(tab) );

    RUN(args << "importtab" << tmp.fileName(), "");
    RUN(args << "read" << "0" << "1" << "2", "abc def ghi");
    RUN(args << "size", "3\n");

    // Export with relative path.
    QTemporaryFile tmp2;
    QVERIFY(tmp2.open());

    // Change back to original working directory once finished.
    struct CurrentDirectoryGuard {
        CurrentDirectoryGuard() : oldDir(QDir::currentPath()) {}
        ~CurrentDirectoryGuard() { QDir::setCurrent(oldDir); }
        const QString oldDir;
    } currentDirectoryGuard;

    QDir::setCurrent( QDir::cleanPath(tmp2.fileName() + "/..") );

    const QString fileName = QFileInfo( tmp2.fileName() ).fileName();

    RUN(args << "add" << "012", "");
    RUN(args << "exporttab" << fileName, "");

    RUN("removetab" << tab, "");
    QVERIFY( !hasTab(tab) );

    RUN(args << "importtab" << fileName, "");
    RUN(args << "read" << "0" << "1" << "2" << "3", "012 abc def ghi");
}

void Tests::removeAllFoundItems()
{
    auto args = Args("add");
    for (int i = 0; i < 50; ++i) {
        args << QString("a%1").arg(i);
        args << QString("b%1").arg(i);
    }

    RUN(args, "");
    RUN("size", "100\n");

    RUN("filter" << "a", "");
    RUN("keys" << "CTRL+A" << m_test->shortcutToRemove(), "");

    RUN("size", "50\n");
    RUN("read" << "49" << "48" << "47", "b0\nb1\nb2");
    RUN("read" << "0" << "1" << "2", "b49\nb48\nb47");
}

void Tests::nextPrevious()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab;
    RUN(args << "add" << "C" << "B" << "A", "");

    RUN(args << "next", "");
    WAIT_FOR_CLIPBOARD("B");

    RUN(args << "next", "");
    WAIT_FOR_CLIPBOARD("C");

    RUN(args << "next", "");
    WAIT_FOR_CLIPBOARD("C");

    RUN(args << "previous", "");
    WAIT_FOR_CLIPBOARD("B");

    RUN(args << "previous", "");
    WAIT_FOR_CLIPBOARD("A");

    RUN(args << "previous", "");
    WAIT_FOR_CLIPBOARD("A");
}

void Tests::externalEditor()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab;
    const QString editorTab = testTab(2);
    const Args editorArgs = Args("tab") << editorTab;
    const Args editorFileNameArgs = Args(editorArgs) << "read" << "0";
    const Args editorEndArgs = Args(editorArgs) << "remove" << "0";

    // Set editor command which add file name to edit to special editor tab.
    // The command finishes when the special tab is emptied by this test.
    // File to edit is removed by application when the command finished.
    const auto cmd = QString(
                R"(
                    "%1" tab "%2" eval
                    "add(arguments[1]); while(length()) sleep(100);"
                )"
                "--")
            .arg(QCoreApplication::applicationFilePath())
            .arg(editorTab)
            + " %1";
    RUN("config" << "editor" << cmd, cmd + "\n");

    // Set clipboard.
    const QByteArray data1 = generateData();
    TEST( m_test->setClipboard(data1) );
    RUN("clipboard", data1);

#define EDIT(DATA1, DATA2) \
    do { \
        WAIT_ON_OUTPUT(editorArgs << "size", "1\n"); \
        QByteArray out; \
        QByteArray err; \
        run(editorFileNameArgs, &out, &err); \
        QVERIFY2( testStderr(err), err ); \
        QFile file(out); \
        QVERIFY( file.exists() ); \
        QVERIFY( file.open(QIODevice::ReadWrite) ); \
        QVERIFY( file.readAll() == (DATA1) ); \
        file.write(DATA2); \
        file.close(); \
        RUN(editorEndArgs, ""); \
        waitWhileFileExists(file); \
    } while(false)

    // Edit clipboard.
    RUN("edit" << "-1", "");
    const QByteArray data2 = generateData();
    EDIT(data1, data2);

    // Check if clipboard changed.
    WAIT_ON_OUTPUT("read" << "0", data1 + data2);
    WAIT_FOR_CLIPBOARD(data1 + data2);

    // Edit existing item.
    const QString text =
            "Some text to edit,\n"
            "with second line!\n"
            + generateData();
    RUN(args << "add" << text, "");

    // Modify first item.
    RUN(args << "edit" << "0", "");
    const QByteArray data3 = generateData();
    EDIT(text.toUtf8(), data3);

    // Check first item.
    WAIT_ON_OUTPUT(args << "read" << "0", text.toUtf8() + data3);

    // Edit new item.
    RUN(args << "edit", "");
    const QByteArray data4 = generateData();
    EDIT(QByteArray(), data4);

    // Check first item.
    WAIT_ON_OUTPUT(args << "read" << "0", data4);

#undef EDIT
}

void Tests::nextPreviousTab()
{
    const auto tab1 = testTab(1);
    RUN("tab" << tab1 << "size", "0\n");

    const auto tab2 = testTab(2);
    RUN("tab" << tab2 << "size", "0\n");

    using KeyPair = QPair<QString, QString>;
    const QList<KeyPair> keyPairs = QList<KeyPair>()
            << KeyPair(keyNameFor(QKeySequence::NextChild), keyNameFor(QKeySequence::PreviousChild))
            << KeyPair("RIGHT", "LEFT");

    for (const auto &keyPair : keyPairs) {
        for (const auto &optionValue : {"false", "true"}) {
            RUN("config" << "tab_tree" << optionValue, QString(optionValue) + "\n");

            RUN("keys" << keyPair.first, "");
            RUN("testSelected", tab1 + "\n");
            RUN("keys" << keyPair.first, "");
            RUN("testSelected", tab2 + "\n");
            RUN("keys" << keyPair.first, "");

            RUN("keys" << keyPair.second, "");
            RUN("testSelected", tab2 + "\n");
            RUN("keys" << keyPair.second, "");
            RUN("testSelected", tab1 + "\n");
            RUN("keys" << keyPair.second, "");
        }
    }
}

void Tests::itemPreview()
{
    const auto tab1 = testTab(1);
    RUN("tab" << tab1 << "add" << "def" << "abc", "");
    RUN("setCurrentTab" << tab1, "");

    RUN("preview", "false\n");
    RUN("keys" << clipboardBrowserId << "F7", "");
    RUN("preview", "true\n");

    RUN("keys" << clipboardBrowserId << "TAB" << itemPreviewId, "")
    RUN("keys" << itemPreviewId << "HOME", "")
    RUN("keys" << itemPreviewId << "RIGHT", "")
    RUN("keys" << itemPreviewId << "SHIFT+RIGHT", "")
    RUN("keys" << itemPreviewId << keyNameFor(QKeySequence::Copy), "")
    WAIT_FOR_CLIPBOARD("b");

    RUN("keys" << itemPreviewId << "F7" << clipboardBrowserId, "")

    RUN("preview" << "true", "false\n");
    RUN("preview" << "false", "true\n");
    RUN("preview" << "1", "false\n");
    RUN("preview" << "0", "true\n");
    RUN("preview(true)", "false\n");
    RUN("preview(false)", "true\n");
    RUN("preview", "false\n");
}

void Tests::openAndSavePreferences()
{
#ifdef Q_OS_MAC
    SKIP("Can't focus configuration checkboxes on OS X");
#endif

    RUN("config" << "check_clipboard" << "false", "false\n");

    // Open preferences dialog.
    RUN("keys" << "Ctrl+P", "");

    // Focus and set wrap text option.
    // This behavior could differ on some systems and in other languages.
    RUN("keys" << "ALT+1", "");
    RUN("keys" << "ENTER", "");
    RUN("config" << "check_clipboard", "true\n");
}

void Tests::pasteFromMainWindow()
{
    RUN("config"
        << "activate_closes" << "true"
        << "activate_focuses" << "true"
        << "activate_pastes" << "true"
        ,
        "activate_closes=true\n"
        "activate_focuses=true\n"
        "activate_pastes=true\n"
        );

    RUN("add" << "TEST", "");
    RUN("hide", "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "dialog('text')", "TEST\n"); },
        [&]() {
            RUN("keys" << "focus::QLineEdit in :QDialog", "");
            RUN("show", "");
            RUN("keys" << clipboardBrowserId << "ENTER", "");

            WAIT_FOR_CLIPBOARD("TEST");
            waitFor(waitMsPasteClipboard);

            RUN("keys" << "focus::QLineEdit in :QDialog" << "ENTER", "");
        }
    );
}

void Tests::tray()
{
    RUN("add" << "A", "");
    RUN("keys" << clipboardBrowserId, "");
    RUN("menu", "");
    ACTIVATE_MENU_ITEM(trayMenuId, clipboardBrowserId, "A");
}

void Tests::menu()
{
    const auto tab = testTab(1);

    RUN("tab" << tab << "add" << "D" << "C" << "B" << "A", "");

    RUN("keys" << clipboardBrowserId, "");
    RUN("menu" << tab, "");
    ACTIVATE_MENU_ITEM(menuId, clipboardBrowserId, "A");

    // Show menu with 2 items from the tab and select last one.
    RUN("keys" << clipboardBrowserId, "");
    RUN("menu" << tab << "2", "");
    RUN("keys" << menuId << "END", "");
    ACTIVATE_MENU_ITEM(menuId, clipboardBrowserId, "B");
}

void Tests::traySearch()
{
    RUN("add" << "C" << "B" << "A", "");

    RUN("keys" << clipboardBrowserId, "");
    RUN("menu", "");
    RUN("keys" << trayMenuId << "B", "");
    ACTIVATE_MENU_ITEM(trayMenuId, clipboardBrowserId, "B");
}

void Tests::trayPaste()
{
    RUN("config" << "tray_tab_is_current" << "false", "false\n");

    const auto tab1 = testTab(1);
    RUN("setCurrentTab" << tab1, "");
    RUN("keys"
        << clipboardBrowserId << "CTRL+N"
        << editorId << ":NEW ", "");

    RUN("add" << "TEST", "");
    RUN("keys" << editorId, "");
    RUN("menu", "");
    ACTIVATE_MENU_ITEM(trayMenuId, editorId, "TEST");
    waitFor(waitMsPasteClipboard);

    RUN("keys" << editorId << "F2", "");
    RUN("tab" << tab1 << "read" << "0", "NEW TEST");

    RUN("keys"
        << clipboardBrowserId << "CTRL+N"
        << editorId << ":NEW ", "");

    RUN("config" << "tray_item_paste" << "false", "false\n");
    RUN("add" << "TEST2", "");
    RUN("keys" << editorId, "");
    RUN("menu", "");
    ACTIVATE_MENU_ITEM(trayMenuId, editorId, "TEST2");

    RUN("keys" << editorId << "F2", "");
    RUN("tab" << tab1 << "read" << "0", "NEW ");
}

void Tests::configTrayTab()
{
    const auto tab1 = testTab(1);
    RUN("tab" << tab1 << "add" << "A", "");

    const auto tab2 = testTab(2);
    RUN("tab" << tab2 << "add" << "B", "");

    RUN("config" << "tray_tab_is_current" << "false", "false\n");

    RUN("config" << "tray_item_paste" << "false", "false\n");

    RUN("config" << "tray_tab" << tab1, tab1 + "\n");

    RUN("keys" << clipboardBrowserId, "");
    RUN("menu", "");
    ACTIVATE_MENU_ITEM(trayMenuId, clipboardBrowserId, "A");

    RUN("config" << "tray_tab" << tab2, tab2 + "\n");

    RUN("keys" << clipboardBrowserId, "");
    RUN("menu", "");
    ACTIVATE_MENU_ITEM(trayMenuId, clipboardBrowserId, "B");
}

void Tests::configMove()
{
    SKIP_ON_ENV("COPYQ_TESTS_SKIP_CONFIG_MOVE");

    RUN("add" << "B" << "A", "");

    RUN("config" << "tray_item_paste" << "false", "false\n");

    RUN("config" << "move" << "true", "true\n");

    RUN("keys" << clipboardBrowserId, "");
    RUN("menu", "");
    RUN("keys" << trayMenuId << "DOWN", "");
    ACTIVATE_MENU_ITEM(trayMenuId, clipboardBrowserId, "B");
    RUN("read" << "0" << "1", "B\nA");

    RUN("config" << "move" << "false", "false\n");

    RUN("keys" << clipboardBrowserId, "");
    RUN("menu", "");
    RUN("keys" << trayMenuId << "DOWN", "");
    ACTIVATE_MENU_ITEM(trayMenuId, clipboardBrowserId, "A");
    RUN("read" << "0" << "1", "B\nA");
}

void Tests::configTrayTabIsCurrent()
{
    const auto tab1 = testTab(1);
    RUN("tab" << tab1 << "add" << "A", "");

    const auto tab2 = testTab(2);
    RUN("tab" << tab2 << "add" << "B", "");

    RUN("config" << "tray_tab_is_current" << "true", "true\n");

    RUN("setCurrentTab" << tab1, "");
    RUN("keys" << clipboardBrowserId, "");
    RUN("menu", "");
    ACTIVATE_MENU_ITEM(trayMenuId, clipboardBrowserId, "A");

    RUN("setCurrentTab" << tab2, "");
    RUN("keys" << clipboardBrowserId, "");
    RUN("menu", "");
    ACTIVATE_MENU_ITEM(trayMenuId, clipboardBrowserId, "B");
}

void Tests::configAutostart()
{
    if ( !platformNativeInterface()->canAutostart() ) {
        SKIP("Autostart is unsupported on this platform");
        return;
    }

    RUN("config" << "autostart" << "true", "true\n");
    RUN("config" << "autostart", "true\n");
    RUN("config" << "autostart" << "false", "false\n");
    RUN("config" << "autostart", "false\n");
}

void Tests::configPathEnvVariable()
{
    const auto path = QDir::home().absoluteFilePath("copyq-settings");
    const auto environment = QStringList("COPYQ_SETTINGS_PATH=" + path);

    QByteArray out;
    QByteArray err;
    run(Args() << "info" << "config", &out, &err, QByteArray(), environment);
    QVERIFY2( testStderr(err), err );

    const auto expectedOut = path.toUtf8();
    QCOMPARE( out.left(expectedOut.size()), expectedOut );
}

void Tests::shortcutCommand()
{
    RUN("setCommands([{name: 'test', inMenu: true, shortcuts: ['Ctrl+F1'], cmd: 'copyq add OK'}])", "");
    RUN("keys" << "CTRL+F1", "");
    WAIT_ON_OUTPUT("read" << "0", "OK");
}

void Tests::shortcutCommandOverrideEnter()
{
    RUN("setCommands([{name: 'test', inMenu: true, shortcuts: ['Enter'], cmd: 'copyq add OK'}])", "");
    RUN("keys" << "ENTER" << "ENTER", "");
    WAIT_ON_OUTPUT("read" << "0" << "1", "OK\nOK");
}

void Tests::shortcutCommandMatchInput()
{
    // Activate only one of the two actions depending on input MIME format.
    const auto script = R"(
        function cmd(name) {
          var format = 'application/x-copyq-' + name
          return {
            name: name,
            inMenu: true,
            shortcuts: ['Ctrl+F1'],
            input: format,
            cmd: 'copyq add ' + name
          }
        }
        setCommands([ cmd('test1'), cmd('test2') ])
        )";
    RUN(script, "");

    RUN("write" << "application/x-copyq-test1" << "", "");
    RUN("keys" << "CTRL+F1", "");
    WAIT_ON_OUTPUT("read" << "0", "test1");
    RUN("tab" << QString(clipboardTabName) << "size", "2\n");

    RUN("write" << "application/x-copyq-test2" << "", "");
    RUN("keys" << "CTRL+F1", "");
    WAIT_ON_OUTPUT("read" << "0", "test2");
    RUN("tab" << QString(clipboardTabName) << "size", "4\n");
}

void Tests::shortcutCommandMatchCmd()
{
    const auto tab = testTab(1);
    const Args args = Args("tab") << tab;

    // Activate only one of the three actions depending on exit code of command which matches input MIME format.
    const auto script = R"(
        function cmd(name) {
          var format = 'application/x-copyq-' + name
          return {
            name: name,
            inMenu: true,
            shortcuts: ['Ctrl+F1'],
            matchCmd: 'copyq: str(data("' + format + '")) || fail()',
            cmd: 'copyq tab )" + tab + R"( add ' + name
          }
        }
        setCommands([ cmd('test1'), cmd('test2') ])
        )";
    RUN(script, "");

    RUN("write" << "application/x-copyq-test1" << "1", "");
    WAIT_ON_OUTPUT(args << "keys('Ctrl+F1'); read(0)", "test1");

    RUN("write" << "application/x-copyq-test2" << "2", "");
    WAIT_ON_OUTPUT(args << "keys('Ctrl+F1'); read(0)", "test2");
}

void Tests::shortcutCommandSelectedItemData()
{
    const auto tab1 = testTab(1);
    const auto script = R"(
        setCommands([{
            name: 'Move Second Selected Item to Other Tab',
            inMenu: true,
            shortcuts: ['Ctrl+F1'],
            output: 'text/plain',
            outputTab: ')" + tab1 + R"(',
            cmd: 'copyq: selectedItemData(1)["text/plain"]'
        }])
        )";
    RUN(script, "");

    RUN("add" << "C" << "B" << "A", "");
    RUN("selectItems" << "1" << "2", "true\n");
    RUN("keys" << "CTRL+F1", "");
    WAIT_ON_OUTPUT("tab" << tab1 << "read" << "0", "C");
}

void Tests::shortcutCommandSetSelectedItemData()
{
    const auto script = R"(
        setCommands([{
            name: 'Set Data for Second Selected Item',
            inMenu: true,
            shortcuts: ['Ctrl+F1'],
            cmd: 'copyq: setSelectedItemData(1, {"text/plain": "X", "DATA": "TEST"})'
        }])
        )";
    RUN(script, "");

    RUN("add" << "C" << "B" << "A", "");
    RUN("selectItems" << "1" << "2", "true\n");
    RUN("keys" << "CTRL+F1", "");
    WAIT_ON_OUTPUT("read" << "2", "X");
    RUN("read" << "DATA" << "2", "TEST");
}

void Tests::shortcutCommandSelectedItemsData()
{
    const auto tab1 = testTab(1);
    const auto script = R"(
        setCommands([{
            name: 'Concatenate Selected Items to Other Tab',
            inMenu: true,
            shortcuts: ['Ctrl+F1'],
            output: 'text/plain',
            outputTab: ')" + tab1 + R"(',
            cmd: 'copyq: d = selectedItemsData();'
               + 'for (i in d) { print(d[i][mimeText]); print(",") }'
        }])
        )";
    RUN(script, "");

    RUN("add" << "C" << "B" << "A", "");
    RUN("selectItems" << "1" << "2", "true\n");
    RUN("keys" << "CTRL+F1", "");
    WAIT_ON_OUTPUT("tab" << tab1 << "read" << "0", "B,C,");
}

void Tests::shortcutCommandSetSelectedItemsData()
{
    const auto script = R"(
        setCommands([{
            name: 'Set Data for Second Selected Item',
            inMenu: true,
            shortcuts: ['Ctrl+F1'],
            cmd: 'copyq: setSelectedItemsData([{"text/plain": "X"}, {"text/plain": "Y"}])'
        }])
        )";
    RUN(script, "");

    RUN("add" << "C" << "B" << "A", "");
    RUN("selectItems" << "1" << "2", "true\n");
    RUN("keys" << "CTRL+F1", "");
    WAIT_ON_OUTPUT("read" << "0" << "1" << "2", "A\nX\nY");
}

void Tests::shortcutCommandSelectedAndCurrent()
{
    const auto script = R"(
        setCommands([{
            name: 'Set Data for Second Selected Item',
            inMenu: true,
            shortcuts: ['Ctrl+F1'],
            output: 'text/plain',
            cmd: 'copyq: print(selectedItems() + "|" + currentItem() + "|" + selectedTab())'
        }])
        )";
    RUN(script, "");

    const auto tab1 = testTab(1);
    RUN("tab" << tab1 << "add" << "C" << "B" << "A", "");

    RUN("tab" << tab1 << "setCurrentTab" << tab1 << "selectItems" << "1" << "2", "true\n");
    RUN("keys" << "CTRL+F1", "");
    WAIT_ON_OUTPUT("tab" << tab1 << "read(0)", "1,2|2|" + tab1.toUtf8());
}

void Tests::shortcutCommandMoveSelected()
{
    const QString script = R"(
        setCommands([{
            name: 'Move Selected',
            inMenu: true,
            shortcuts: ['Ctrl+F1'],
            output: 'text/plain',
            cmd: 'copyq: move(%1); settings("done", 1)'
        }])
        )";
    RUN(script.arg(1), "");

    const Args args = Args("tab") << testTab(1) << "separator" << ",";
    RUN("setCurrentTab" << testTab(1), "");
    RUN(args << "add" << "4" << "3" << "2" << "1", "");

#define MOVE_SELECTED(EXPECTED_ITEMS) \
    RUN("settings" << "done" << "0" << "keys" << "CTRL+F1", ""); \
    WAIT_ON_OUTPUT("settings" << "done", "1\n"); \
    RUN(args << "read(0,1,2,3,4)", EXPECTED_ITEMS)

    RUN(args << "selectItems" << "1" << "2", "true\n");
    MOVE_SELECTED("1,2,3,4,");

    RUN(args << "selectItems" << "2" << "3", "true\n");
    MOVE_SELECTED("1,3,4,2,");

    RUN(script.arg(5), "");
    MOVE_SELECTED("1,3,4,2,");

    RUN(script.arg(-1), "");
    MOVE_SELECTED("1,3,4,2,");

    RUN(script.arg(4), "");
    MOVE_SELECTED("1,2,3,4,");

    RUN(script.arg(0), "");
    MOVE_SELECTED("3,4,1,2,");

#undef MOVE_SELECTED
}

void Tests::automaticCommandIgnore()
{
    const auto script = R"(
        setCommands([
            { automatic: true, cmd: 'copyq ignore; copyq add OK' },
            { automatic: true, cmd: 'copyq add "SHOULD NOT BE EXECUTED"' }
        ])
        )";
    RUN(script, "");

    TEST( m_test->setClipboard("SHOULD BE IGNORED 1") );
    WAIT_ON_OUTPUT("read" << "0", "OK");

    TEST( m_test->setClipboard("SHOULD BE IGNORED 2") );
    WAIT_ON_OUTPUT("size", "2\n");

    RUN("separator" << "," << "read" << "0" << "1" << "2", "OK,OK,");
}

void Tests::automaticCommandRemove()
{
    const auto script = R"(
        setCommands([
            { automatic: true, remove: true, cmd: 'copyq add OK' },
            { automatic: true, cmd: 'copyq add "SHOULD NOT BE EXECUTED"' }
        ])
        )";
    RUN(script, "");

    TEST( m_test->setClipboard("SHOULD BE IGNORED 1") );
    WAIT_ON_OUTPUT("read" << "0", "OK");

    TEST( m_test->setClipboard("SHOULD BE IGNORED 2") );
    WAIT_ON_OUTPUT("size", "2\n");

    RUN("separator" << "," << "read" << "0" << "1" << "2", "OK,OK,");
}

void Tests::automaticCommandInput()
{
    const auto script = R"(
        setCommands([
            { automatic: true, input: 'DATA', cmd: 'copyq: setData("DATA", "???")' },
            { automatic: true, input: 'text/plain', cmd: 'copyq: setData("text/plain", "OK")' },
        ])
        )";
    RUN(script, "");

    TEST( m_test->setClipboard("SHOULD BE CHANGED") );
    WAIT_ON_OUTPUT("read" << "0", "OK");
    RUN("read" << "DATA" << "0", "");
}

void Tests::automaticCommandRegExp()
{
    const auto script = R"(
        setCommands([
            { automatic: true, re: 'SHOULD BE CHANGED$', cmd: 'copyq: setData("text/plain", "CHANGED")' },
            { automatic: true, cmd: 'copyq: setData("DATA", "DONE")' },
        ])
        )";
    RUN(script, "");

    TEST( m_test->setClipboard("SHOULD BE CHANGED") );
    WAIT_ON_OUTPUT("read" << "DATA" << "0", "DONE");
    RUN("read" << "0", "CHANGED");
    RUN("remove" << "0", "");

    TEST( m_test->setClipboard("SHOULD NOT BE CHANGED") );
    WAIT_ON_OUTPUT("read" << "DATA" << "0", "DONE");
    RUN("read" << "0", "SHOULD NOT BE CHANGED");
}

void Tests::automaticCommandSetData()
{
    const auto script = R"(
        setCommands([{automatic: true, cmd: 'copyq: setData("text/plain", "OK")'}])
        )";
    RUN(script, "");
    TEST( m_test->setClipboard("SHOULD BE CHANGED") );
    WAIT_ON_OUTPUT("read" << "0", "OK");
}

void Tests::automaticCommandOutputTab()
{
    const auto tab1 = testTab(1);
    const auto script = R"(
        var tab1 = ')" + tab1 + R"('
        setCommands([{automatic: true, cmd: 'copyq: setData(mimeOutputTab, "' + tab1 + '")'}])
        )";
    RUN(script, "");
    TEST( m_test->setClipboard("TEST") );
    WAIT_ON_OUTPUT("tab" << tab1 << "read" << "0", "TEST");
    RUN("tab" << QString(clipboardTabName) << "size", "0\n");
}

void Tests::automaticCommandNoOutputTab()
{
    const auto script = R"(
        setCommands([{automatic: true, cmd: 'copyq: removeData(mimeOutputTab)'}])
        )";
    RUN(script, "");
    TEST( m_test->setClipboard("TEST") );
    waitFor(1000);
    RUN("tab" << QString(clipboardTabName) << "size", "0\n");
}

void Tests::automaticCommandChaining()
{
    const auto script = R"(
        setCommands([
            {automatic: true, cmd: 'copyq: setData(mimeText, 1)'},
            {automatic: true, cmd: 'copyq: setData(mimeText, str(data(mimeText)) + 2)'},
            {automatic: true, cmd: 'copyq: setData(mimeText, str(data(mimeText)) + 3)'}
        ])
        )";
    RUN(script, "");
    TEST( m_test->setClipboard("TEST") );
    WAIT_ON_OUTPUT("read" << "0", "123");
}

void Tests::automaticCommandCopyToTab()
{
    const auto tab1 = testTab(1);
    const auto script = R"(
        setCommands([{automatic: true, tab: ')" + tab1 + R"('}])
        )";
    RUN(script, "");
    TEST( m_test->setClipboard("TEST") );
    WAIT_ON_OUTPUT("tab" << QString(clipboardTabName) << "read" << "0", "TEST");
    RUN("tab" << tab1 << "read" << "0", "TEST");
}

void Tests::automaticCommandStoreSpecialFormat()
{
    const auto script = R"(
        setCommands([
            { automatic: true, name: 'CMD1', input: 'test-format' }
        ])
        )";
    RUN(script, "");

    TEST( m_test->setClipboard("DATA", "test-format") );
    WAIT_ON_OUTPUT("separator" << "," << "read" << "test-format" << "0" << "1", "DATA,");
}

void Tests::automaticCommandIgnoreSpecialFormat()
{
    const auto script = R"(
        setCommands([
            { automatic: true, name: 'CMD1', cmd: 'copyq add CMD1', input: 'test-format', remove: true },
            { automatic: true, name: 'CMD2', cmd: 'copyq add CMD2' },
            { automatic: true, name: 'CMD3', cmd: 'copyq add CMD3', input: 'test-format' }
        ])
        )";
    RUN(script, "");

    TEST( m_test->setClipboard("SHOULD BE IGNORED", "test-format") );
    WAIT_ON_OUTPUT("separator" << "," << "read" << "0" << "1", "CMD1,");

    TEST( m_test->setClipboard("SHOULD NOT BE IGNORED") );
    WAIT_ON_OUTPUT("separator" << "," << "read" << "0" << "1" << "2" << "3", "SHOULD NOT BE IGNORED,CMD2,CMD1,");
}

void Tests::scriptCommandLoaded()
{
    const auto script = R"(
        setCommands([{
            isScript: true,
            cmd: 'add("LOADED")'
        }])
        )";
    RUN(script, "");
    RUN("read(0)", "LOADED");
}

void Tests::scriptCommandAddFunction()
{
    const auto script = R"(
        setCommands([{
            isScript: true,
            cmd: 'global.test = function() { return "TEST"; }'
        }])
        )";
    RUN(script, "");
    RUN("test", "TEST\n");
}

void Tests::scriptCommandOverrideFunction()
{
    const auto script = R"(
        setCommands([{
            isScript: true,
            cmd: 'popup = function(msg) { return msg; }'
        }])
        )";
    RUN(script, "");
    RUN("popup" << "test" << "xxx", "test");
}

void Tests::displayCommand()
{
    const auto testMime = COPYQ_MIME_PREFIX "test";
    const auto script = QString(R"(
        setCommands([{
            display: true,
            input: '%1',
            cmd: 'copyq:'
               + 'text = str(data(mimeText));'
               + 'currentTab = str(data(mimeCurrentTab));'
               + 'add(currentTab + "/" + text);'
        }])
        )").arg(testMime);

    RUN(script, "");

    RUN("write" << "0" << testMime << "" << mimeText << "a", "");
    WAIT_ON_OUTPUT(
                "read(0,1,2)",
                QString("%1/a\na\n")
                .arg(clipboardTabName)
                .toUtf8() );

    RUN("write" << "0" << testMime << "" << mimeText << "b", "");
    WAIT_ON_OUTPUT(
                "read(0,1,2,3,4)",
                QString("%1/b\nb\n%1/a\na\n")
                .arg(clipboardTabName)
                .toUtf8() );
}

void Tests::queryKeyboardModifiersCommand()
{
    RUN("queryKeyboardModifiers()", "");
    // TODO: Is there a way to press modifiers?
}

void Tests::pointerPositionCommand()
{
    QCursor::setPos(1, 2);
    RUN("pointerPosition", "1\n2\n");
    QCursor::setPos(2, 3);
    RUN("pointerPosition", "2\n3\n");
}

void Tests::setPointerPositionCommand()
{
    RUN("setPointerPosition(1,2)", "");
    QCOMPARE(QPoint(1, 2), QCursor::pos());
    RUN("setPointerPosition(2,3)", "");
    QCOMPARE(QPoint(2, 3), QCursor::pos());
}

void Tests::setTabName()
{
    const auto script = R"(
        tab('1')
        add(1)
        tab1Size = size()

        tab('2')
        tab2Size = size()

        print(tab1Size + ',' + tab2Size)
        )";
    RUN(script, "1,0");
}

void Tests::showHideAboutDialog()
{
    const auto aboutShortcut = keyNameFor(QKeySequence::QKeySequence::WhatsThis);
    RUN("keys" << clipboardBrowserId << aboutShortcut << aboutDialogId, "");
    RUN("keys" << aboutDialogId << "ESCAPE" << clipboardBrowserId, "");
}

void Tests::showHideClipboardDialog()
{
    TEST( m_test->setClipboard("TEST", "test-format") );
    RUN("keys" << clipboardBrowserId << "CTRL+SHIFT+C" << clipboardDialogId, "");

    RUN("keys" << clipboardDialogId << "DOWN" << "HOME" << clipboardDialogFormatListId, "");

    RUN("keys" << clipboardDialogFormatListId << keyNameFor(QKeySequence::Copy), "");
#ifdef Q_OS_WIN
    WAIT_FOR_CLIPBOARD("application/x-qt-windows-mime;value=\"test-format\"");
#else
    WAIT_FOR_CLIPBOARD("test-format");
#endif

    RUN("keys" << clipboardDialogId << "ESCAPE" << clipboardBrowserId, "");
}

void Tests::showHideItemDialog()
{
    RUN("write" << "test-format" << "TEST", "")
    RUN("selectItems" << "0", "true\n");

    RUN("keys" << clipboardBrowserId << "F4" << clipboardDialogId, "");

    RUN("keys" << clipboardDialogId << "DOWN" << clipboardDialogFormatListId, "");
    RUN("keys" << clipboardDialogFormatListId << keyNameFor(QKeySequence::Copy), "");
    WAIT_FOR_CLIPBOARD("test-format");

    RUN("keys" << clipboardDialogFormatListId << "DOWN", "");
    RUN("keys" << clipboardDialogFormatListId << keyNameFor(QKeySequence::Copy), "");
    WAIT_FOR_CLIPBOARD("test-format");

    RUN("keys" << clipboardDialogId << "ESCAPE" << clipboardBrowserId, "");
}

void Tests::showHideLogDialog()
{
    RUN("keys" << clipboardBrowserId << "F12" << logDialogId, "");

    RUN("keys" << logDialogId << "CTRL+A" << "CTRL+C" << logDialogId, "");
    const QByteArray expectedLog = "Starting callback: onStart";
    TEST( m_test->verifyClipboard(expectedLog, mimeHtml, false) );

    RUN("keys" << logDialogId << "ESCAPE" << clipboardBrowserId, "");
}

void Tests::showHideActionHandlerDialog()
{
    RUN("keys" << clipboardBrowserId << "CTRL+SHIFT+Z" << actionHandlerDialogId, "");

    RUN("keys" << actionHandlerFilterId << ":onstart" << "TAB" << actionHandlerTableId, "");

    RUN("keys" << actionHandlerTableId << "RIGHT" << "CTRL+C", "");
    WAIT_FOR_CLIPBOARD("copyq onStart");

    RUN("keys" << actionHandlerDialogId << "ESCAPE" << clipboardBrowserId, "");
}

void Tests::shortcutDialogAddShortcut()
{
#ifdef Q_OS_MAC
    SKIP("Mnemonic for focusing shortcut button doesn't work on OS X");
#endif

    RUN("setCommands([{name: 'test', inMenu: true, cmd: 'copyq add OK'}])", "");
    RUN("commands()[0].shortcuts", "")

    RUN("keys" << clipboardBrowserId << "F6" << commandDialogId, "");
    RUN("keys" << commandDialogId << "ALT+S" << shortcutButtonId, "");
    RUN("keys" << shortcutButtonId << "Space" << shortcutDialogId, "");
    RUN("keys" << shortcutDialogId << "CTRL+F1" << shortcutButtonId, "");

    RUN("keys" << commandDialogId << "ESCAPE" << commandDialogSaveButtonId, "");
    RUN("keys" << commandDialogSaveButtonId << "Enter" << clipboardBrowserId, "");
    RUN("commands()[0].shortcuts", "ctrl+f1\n")
}

void Tests::shortcutDialogAddTwoShortcuts()
{
#ifdef Q_OS_MAC
    SKIP("Mnemonic for focusing shortcut button doesn't work on OS X");
#endif

    RUN("setCommands([{name: 'test', inMenu: true, shortcuts: ['ctrl+f1'], cmd: 'copyq add OK'}])", "");
    RUN("commands()[0].shortcuts", "ctrl+f1\n")

    RUN("keys" << clipboardBrowserId << "F6" << commandDialogId, "");
    RUN("keys" << commandDialogId << "ALT+S" << shortcutButtonId, "");

    RUN("keys" << shortcutButtonId << "TAB" << shortcutButtonId, "");
    RUN("keys" << shortcutButtonId << "Space" << shortcutDialogId, "");
    RUN("keys" << shortcutDialogId << "F1" << shortcutButtonId, "");

    RUN("keys" << shortcutButtonId << "Space" << shortcutDialogId, "");
    RUN("keys" << shortcutDialogId << "F2" << shortcutButtonId, "");

    RUN("keys" << commandDialogId << "ESCAPE" << commandDialogSaveButtonId, "");
    RUN("keys" << commandDialogSaveButtonId << "Enter" << clipboardBrowserId, "");
    RUN("commands()[0].shortcuts", "ctrl+f1\nf1\nf2\n")
}

void Tests::shortcutDialogChangeShortcut()
{
#ifdef Q_OS_MAC
    SKIP("Mnemonic for focusing shortcut button doesn't work on OS X");
#endif

    RUN("setCommands([{name: 'test', inMenu: true, shortcuts: ['f1','f2','f3'], cmd: 'copyq add OK'}])", "");
    RUN("commands()[0].shortcuts", "f1\nf2\nf3\n")

    RUN("keys" << clipboardBrowserId << "F6" << commandDialogId, "");
    RUN("keys" << commandDialogId << "ALT+S" << shortcutButtonId, "");
    RUN("keys" << commandDialogId << "TAB" << shortcutButtonId, "");
    RUN("keys" << shortcutButtonId << "Space" << shortcutDialogId, "");
    RUN("keys" << shortcutDialogId << "F4" << shortcutButtonId, "");

    RUN("keys" << commandDialogId << "ESCAPE" << commandDialogSaveButtonId, "");
    RUN("keys" << commandDialogSaveButtonId << "Enter" << clipboardBrowserId, "");
    RUN("commands()[0].shortcuts", "f1\nf4\nf3\n")
}

void Tests::shortcutDialogSameShortcut()
{
#ifdef Q_OS_MAC
    SKIP("Mnemonic for focusing shortcut button doesn't work on OS X");
#endif

    RUN("setCommands([{name: 'test', inMenu: true, shortcuts: ['ctrl+f1'], cmd: 'copyq add OK'}])", "");
    RUN("commands()[0].shortcuts", "ctrl+f1\n")

    RUN("keys" << clipboardBrowserId << "F6" << commandDialogId, "");
    RUN("keys" << commandDialogId << "ALT+S" << shortcutButtonId, "");
    RUN("keys" << shortcutButtonId << "TAB" << shortcutButtonId, "");
    RUN("keys" << shortcutButtonId << "Space" << shortcutDialogId, "");
    RUN("keys" << shortcutDialogId << "CTRL+F1" << shortcutButtonId, "");

    RUN("keys" << commandDialogId << "ESCAPE" << clipboardBrowserId, "");
    RUN("commands()[0].shortcuts", "ctrl+f1\n")
}

void Tests::shortcutDialogCancel()
{
#ifdef Q_OS_MAC
    SKIP("Mnemonic for focusing shortcut button doesn't work on OS X");
#endif

    RUN("setCommands([{name: 'test', inMenu: true, shortcuts: ['ctrl+f1'], cmd: 'copyq add OK'}])", "");
    RUN("commands()[0].shortcuts", "ctrl+f1\n")

    RUN("keys" << clipboardBrowserId << "F6" << commandDialogId, "");
    RUN("keys" << commandDialogId << "ALT+S" << shortcutButtonId, "");
    RUN("keys" << commandDialogId << "TAB" << shortcutButtonId, "");
    RUN("keys" << shortcutButtonId << "Space" << shortcutDialogId, "");
    RUN("keys" << shortcutDialogId << "ESCAPE" << shortcutButtonId, "");

    RUN("keys" << commandDialogId << "ESCAPE" << clipboardBrowserId, "");
    RUN("commands()[0].shortcuts", "ctrl+f1\n")
}

void Tests::actionDialogCancel()
{
    const auto script = R"(
        setCommands([{
            name: 'test',
            inMenu: true,
            shortcuts: ['ctrl+f1'],
            wait: true,
            cmd: 'copyq settings test SHOULD_NOT_BE_SET'
        }])
        )";
    RUN(script, "");

    RUN("keys" << clipboardBrowserId << "CTRL+F1" << actionDialogId, "");
    RUN("keys" << actionDialogId << "ESCAPE" << clipboardBrowserId, "");
    RUN("settings" << "test", "");
}

void Tests::actionDialogAccept()
{
    const auto script = R"(
        setCommands([{
            name: 'test',
            inMenu: true,
            shortcuts: ['ctrl+f1'],
            wait: true,
            cmd: 'copyq settings test SHOULD_BE_SET'
        }])
        )";
    RUN(script, "");

    RUN("keys" << clipboardBrowserId << "CTRL+F1" << actionDialogId, "");
    RUN("keys" << actionDialogId << "BACKTAB" << "ENTER" << clipboardBrowserId, "");
    WAIT_ON_OUTPUT("settings" << "test", "SHOULD_BE_SET");
}

void Tests::actionDialogSelection()
{
    const auto script = R"(
        setCommands([{
            name: 'test',
            inMenu: true,
            shortcuts: ['ctrl+f1'],
            wait: true,
            cmd: 'copyq settings test %1'
        }])
        )";
    RUN(script, "");

    const auto tab = testTab(1);
    const auto args = Args("tab") << tab;
    RUN(args << "add" << "C" << "B" << "A", "");
    RUN("setCurrentTab" << tab, "");
    RUN(args << "selectItems" << "0" << "2", "true\n");

    RUN("keys" << clipboardBrowserId << "CTRL+F1" << actionDialogId, "");
    RUN("keys" << actionDialogId << "BACKTAB" << "ENTER" << clipboardBrowserId, "");
    WAIT_ON_OUTPUT("settings" << "test", "A\nC");
}

void Tests::actionDialogSelectionInputOutput()
{
    const auto script = R"(
        setCommands([{
            name: 'test',
            inMenu: true,
            shortcuts: ['ctrl+f1'],
            wait: true,
            input: 'text/plain',
            output: 'text/plain',
            cmd: 'copyq input'
        }])
        )";
    RUN(script, "");

    const auto tab = testTab(1);
    const auto args = Args("tab") << tab;
    RUN(args << "add" << "C" << "B" << "A", "");
    RUN("setCurrentTab" << tab, "");
    RUN(args << "selectItems" << "0" << "2", "true\n");

    RUN("keys" << clipboardBrowserId << "CTRL+F1" << actionDialogId, "");
    RUN("keys" << actionDialogId << "BACKTAB" << "ENTER" << clipboardBrowserId, "");
    WAIT_ON_OUTPUT(args << "read" << "0", "A\nC");
}

void Tests::exitConfirm()
{
    RUN("keys" << clipboardBrowserId << "CTRL+Q" << confirmExitDialogId, "");
    RUN("keys" << confirmExitDialogId << "ENTER", "");
    TEST( m_test->waitForServerToStop() );
}

void Tests::exitNoConfirm()
{
    RUN("config" << "confirm_exit" << "false", "false\n")
    RUN("keys" << clipboardBrowserId << "CTRL+Q", "");
    TEST( m_test->waitForServerToStop() );
}

void Tests::abortInputReader()
{
    RUN_WITH_INPUT("afterMilliseconds(0, abort); input(); 'DONE'", KEEP_STDIN_OPEN, "");
    RUN_WITH_INPUT("afterMilliseconds(50, abort); input(); 'DONE'", KEEP_STDIN_OPEN, "");
    RUN_WITH_INPUT("afterMilliseconds(250, abort); input(); 'DONE'", KEEP_STDIN_OPEN, "");
}

void Tests::changeAlwaysOnTop()
{
    // The window should be still visible and focused after changing always-on-top flag.
    RUN("visible", "true\n");
    RUN("focused", "true\n");
    RUN("config" << "always_on_top", "false\n");

    RUN("config" << "always_on_top" << "true", "true\n");
    WAIT_ON_OUTPUT("visible", "true\n");
    WAIT_ON_OUTPUT("focused", "true\n");

    RUN("config" << "always_on_top" << "false", "false\n");
    WAIT_ON_OUTPUT("visible", "true\n");
    WAIT_ON_OUTPUT("focused", "true\n");

    RUN("hide", "");
    RUN("visible", "false\n");
    RUN("focused", "false\n");

    RUN("config" << "always_on_top" << "true", "true\n");
    WAIT_ON_OUTPUT("visible", "false\n");
    WAIT_ON_OUTPUT("focused", "false\n");

    RUN("config" << "always_on_top" << "false", "false\n");
    WAIT_ON_OUTPUT("visible", "false\n");
    WAIT_ON_OUTPUT("focused", "false\n");
}

void Tests::networkGet()
{
    SKIP_ON_ENV("COPYQ_TESTS_NO_NETWORK");

    RUN("r = networkGet('https://example.com'); r.data; r.status", "200\n");
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
    return splitLines(QString::fromUtf8(out)).contains(tabName);
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
    Q_UNUSED(app);

    const QString session = "copyq.test";
    QCoreApplication::setOrganizationName(session);
    QCoreApplication::setApplicationName(session);
    Settings::canModifySettings = true;
    platform->loadSettings();

    int exitCode = 0;
    std::shared_ptr<TestInterfaceImpl> test(new TestInterfaceImpl);
    Tests tc(test);

    if (onlyPlugins.pattern().isEmpty()) {
        test->setupTest("CORE", QVariant());
        exitCode = test->runTests(&tc, argc, argv);
    }

    if (runPluginTests) {
        ItemFactory itemFactory;
        itemFactory.loadPlugins();
        for ( const auto &loader : itemFactory.loaders() ) {
            if ( loader->id().contains(onlyPlugins) ) {
                std::unique_ptr<QObject> pluginTests( loader->tests(test) );
                if ( pluginTests != nullptr ) {
                    test->setupTest(loader->id(), pluginTests->property("CopyQ_test_settings"));
                    const int pluginTestsExitCode = test->runTests(pluginTests.get(), argc, argv);
                    exitCode = qMax(exitCode, pluginTestsExitCode);
                    test->stopServer();
                }
            }
        }
    }

    return exitCode;
}
