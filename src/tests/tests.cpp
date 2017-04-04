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

#include "tests.h"
#include "test_utils.h"

#include "app/remoteprocess.h"
#include "common/client_server.h"
#include "common/common.h"
#include "common/mimetypes.h"
#include "common/monitormessagecode.h"
#include "common/shortcuts.h"
#include "common/textdata.h"
#include "common/version.h"
#include "item/itemfactory.h"
#include "item/itemwidget.h"
#include "item/serialize.h"
#include "gui/configtabshortcuts.h"

#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QMap>
#include <QMimeData>
#include <QProcess>
#include <QRegExp>
#include <QTemporaryFile>
#include <QTest>
#include <QTimerEvent>

#include <memory>

namespace {

const auto clipboardTabName = "CLIPBOARD";

/**
 * Run a process after a delay.
 *
 * This relies on other occational call to QCoreApplication::processEvents()
 * so the process can be started and others can run simultaneously.
 */
class PostponedProcess : public QObject {
public:
    PostponedProcess(const TestInterfacePtr &test, const QStringList &args)
        : m_test(test)
        , m_args(args)
    {
        startTimer(1000);
    }

protected:
    void timerEvent(QTimerEvent *event) override
    {
        killTimer( event->timerId() );
        RUN(m_args, "");
    }

private:
    TestInterfacePtr m_test;
    QStringList m_args;
};

bool testStderr(const QByteArray &stderrData, TestInterface::ReadStderrFlag flag = TestInterface::ReadErrors)
{
    const QRegExp re("^Warning:|^ERROR:|ASSERT", Qt::CaseInsensitive);

    const QString output = QString::fromUtf8(stderrData);
    if ( output.indexOf(re) != -1 )
        return false;

    if (flag == TestInterface::ReadErrorsWithoutScriptException)
        return true;

    const QRegExp scriptExceptionError("ScriptError:");
    return output.indexOf(scriptExceptionError) == -1;
}

QByteArray getClipboard(const QString &mime = QString("text/plain"))
{
    QApplication::processEvents();
    const QMimeData *data = QApplication::clipboard()->mimeData();
    return (data != nullptr) ? data->data(mime) : QByteArray();
}

QByteArray waitUntilClipboardSet(const QByteArray &data, const QString &mime = QString("text/plain"))
{
    SleepTimer t(200);
    do {
        if (getClipboard(mime) == data)
            return data;
    } while (t.sleep());

    return getClipboard(mime);
}

bool waitWhileFileExists(const QFile &file)
{
    SleepTimer t(2000);
    while (file.exists() && t.sleep()) {}
    return !file.exists();
}

bool waitForProcessFinished(QProcess *p)
{
    // Process events in case we own clipboard and the new process requests the contents.
    SleepTimer t(30000);
    while ( p->state() != QProcess::NotRunning && !p->waitForFinished(50) && t.sleep() ) {}
    return p->state() == QProcess::NotRunning;
}

bool closeProcess(QProcess *p)
{
    if ( waitForProcessFinished(p) )
        return true;

    qWarning() << "terminating process";
    p->terminate();

    if ( !waitForProcessFinished(p) ) {
        qWarning() << "killing process";
        terminateProcess(p);
    }

    return false;
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

class TestInterfaceImpl : public TestInterface {
public:
    TestInterfaceImpl()
        : m_server(nullptr)
        , m_monitor(nullptr)
        , m_env(QProcessEnvironment::systemEnvironment())
    {
        m_env.insert("COPYQ_LOG_LEVEL", "DEBUG");
    }

    ~TestInterfaceImpl()
    {
        stopServer();
    }

    QByteArray startServer() override
    {
        if ( isServerRunning() )
            return "Server is already running.";

        if ( isAnyServerRunning() ) {
            qWarning() << "closing existing test session";
            run(Args("exit"));
            waitForAnyServerToQuit();
        }

        m_server.reset(new QProcess);

        if ( !startTestProcess(m_server.get(), QStringList(), QIODevice::ReadOnly) ) {
            return QString("Failed to launch \"%1\": %2")
                .arg(QApplication::applicationFilePath())
                .arg(m_server->errorString())
                .toUtf8();
        }

        // Wait for client/server communication is established.
        SleepTimer t(15000);
        while( !isServerRunning() && t.sleep() ) {}

        if ( !isServerRunning() )
            return "Unable to start server!" + readServerErrors(ReadAllStderr);

        QByteArray errors = readServerErrors();
        if (!errors.isEmpty())
            return errors;

        return QByteArray();
    }

    QByteArray waitForAnyServerToQuit()
    {
        SleepTimer t(8000);
        while ( isAnyServerRunning() && t.sleep() ) {}

        if ( isAnyServerRunning() )
            return "Unable to stop server!" + readServerErrors(ReadAllStderr);

        return QByteArray();
    }

    QByteArray stopServer() override
    {
        QByteArray errors;
        const int exitCode = run(Args("exit"), nullptr, &errors);
        if ( !testStderr(errors) || exitCode != 0 ) {
            return "Command 'exit' failed."
                    + printClienAndServerStderr(errors, exitCode);
        }

        if ( m_server != nullptr && !closeProcess(m_server.get()) )
            return "Failed to close server properly!" + readServerErrors(ReadAllStderr);

        return waitForAnyServerToQuit();
    }

    bool isServerRunning() override
    {
        return m_server != nullptr && m_server->state() == QProcess::Running && isAnyServerRunning();
    }

    int run(const QStringList &arguments, QByteArray *stdoutData = nullptr,
            QByteArray *stderrData = nullptr, const QByteArray &in = QByteArray()) override
    {
        QProcess p;
        if (!startTestProcess(&p, arguments))
            return -1;

        if ( p.write(in) != in.size() )
            return -2;

        p.closeWriteChannel();

        if (stdoutData != nullptr)
            stdoutData->clear();

        if (stderrData != nullptr)
            stderrData->clear();

        while ( p.state() == QProcess::Running ) {
            const auto out = p.readAllStandardOutput();
            const auto err = p.readAllStandardError();

            if (stdoutData != nullptr)
                stdoutData->append(out);
            if (stderrData != nullptr)
                stderrData->append(err);

            QCoreApplication::processEvents();
        }

        if (stderrData != nullptr) {
            stderrData->append(p.readAllStandardError());
            stderrData->replace('\r', "");
        }

        if (stdoutData != nullptr) {
            stdoutData->append(p.readAllStandardOutput());
            stdoutData->replace('\r', "");
        }

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
            return "Test failed: "
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

    QByteArray setClipboard(const QByteArray &bytes, const QString &mime) override
    {
        if (m_monitor == nullptr) {
            m_monitor.reset(new RemoteProcess);
            const QString name = "copyq_TEST";
            m_monitor->start( name, QStringList() << "-s" << "" << "monitor" << name );

            SleepTimer t(4000);
            while( !m_monitor->isConnected() && t.sleep() ) {}
        }

        if ( !m_monitor->isConnected() )
            return "Failed to start clipboard monitor!";

        const QVariantMap data = createDataMap(mime, bytes);
        m_monitor->writeMessage( serializeData(data), MonitorChangeClipboard );

        waitUntilClipboardSet(bytes, mime);
        QByteArray error = testClipboard(bytes, mime);
        if ( !error.isEmpty() )
            return "Failed to set clipboard! " + error;

        waitFor(200);

        error = testClipboard(bytes, mime);
        if ( !error.isEmpty() )
            return "Clipboard was unexpectedly changed! " + error;

        return "";
    }

    QByteArray readServerErrors(ReadStderrFlag flag = ReadErrors) override
    {
        if (m_server) {
            QByteArray output = m_server->readAllStandardError();

            // Flush server output.
            if (m_server->state() != QProcess::NotRunning) {
                const QByteArray data = generateData();
                if ( run(Args("flush") << data) == 0 ) {
                    SleepTimer t(5000);
                    bool isFlushed = false;
                    do {
                        output.append(m_server->readAllStandardError());
                        isFlushed = output.contains("ID: " + data);
                    } while (!isFlushed && t.sleep());

                    if (!isFlushed)
                        qWarning() << "failed to flush server output";
                }
            }

            output.replace('\r', "");

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

        const QByteArray errors = readServerErrors();
        if ( !errors.isEmpty() )
            return errors;

        return "";
    }

    QByteArray show() override
    {
        const QByteArray out = runClient(Args("show"), "");
        waitFor(waitMsShow);
        return out;
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
        auto errors = cleanup();
        if ( !errors.isEmpty() )
            return errors;

        errors = startServer();
        if ( !errors.isEmpty() )
            return "Failed to start server:\n" + errors;

        errors = setClipboard(QByteArray(), mimeText);
        if ( !errors.isEmpty() )
            return "Failed to reset clipboard:\n" + errors;

        errors = runClient(Args("resetTestSession") << clipboardTabName, "");
        if ( !errors.isEmpty() )
            return errors;

        // Always show main window first so that the results are consistent with desktop environments
        // where user cannot hide main window (tiling window managers without tray).
        return show();
    }

    QByteArray cleanup() override
    {
        return isServerRunning() ? stopServer() : QByteArray();
    }

    QString shortcutToRemove() override
    {
        return ::shortcutToRemove();
    }

    void setupTest(const QString &id, const QVariant &settings)
    {
        m_env.insert("COPYQ_TEST_ID", id);
        QByteArray data;
        QDataStream out(&data, QIODevice::WriteOnly);
        out << settings;
        m_env.insert("COPYQ_TEST_SETTINGS", data.toBase64());
    }

private:
    bool isAnyServerRunning()
    {
        return run(Args("eval")) == 0;
    }

    bool startTestProcess(QProcess *p, const QStringList &arguments,
                          QIODevice::OpenMode mode = QIODevice::ReadWrite)
    {
        p->setProcessEnvironment(m_env);
        p->start( QApplication::applicationFilePath(), arguments, mode );
        return p->waitForStarted(10000);
    }

    QByteArray testClipboard(const QByteArray &bytes, const QString &mime)
    {
        if ( !m_monitor || !m_monitor->isConnected() )
            return "Clipboard monitor is not running!";

        const QByteArray actualBytes = getClipboard(mime);
        if (actualBytes != bytes) {
            return QString("Test failed (clipboard data for MIME \"%1\"): ")
                    .arg(mime).toUtf8()
                    + decorateOutput("Unexpected content", actualBytes)
                    + decorateOutput("Expected content", bytes);
        }

        return "";
    }

    std::unique_ptr<QProcess> m_server;
    std::unique_ptr<RemoteProcess> m_monitor; /// Process to provide clipboard set by tests.
    QProcessEnvironment m_env;
    QVariantMap m_settings;
};

QString keyNameFor(QKeySequence::StandardKey standardKey)
{
    return QKeySequence(standardKey).toString();
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
        QVERIFY( help.contains(QRegExp("\\b" + QRegExp::escape(command) + "\\b")) );
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
    QVERIFY( version.contains(QRegExp("\\bCopyQ\\b.*" + QRegExp::escape(COPYQ_VERSION))) );
    // Version contains Qt version.
    QVERIFY( version.contains(QRegExp("\\bQt\\s+\\d")) );
    // Version contains Qxt version.
    QVERIFY( version.contains(QRegExp("\\bLibQxt\\s+\\d")) );
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
             .contains(QRegExp("^" + QRegExp::escape(testTab(1)) + "$")) );
}

void Tests::commandExit()
{
    TEST( m_test->stopServer() );
    QCOMPARE( run(Args("exit")), 1 );
    TEST( m_test->startServer() );
}

void Tests::commandEval()
{
    RUN("eval" << "", "");
    RUN("eval" << "1", "1\n");
    RUN("eval" << "[1,2,3]", "1\n2\n3\n");
    RUN("eval" << "'123'", "123\n");
    RUN("eval" << "'123'", "123\n");
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
    waitFor(waitMsShow);
    WAIT_ON_OUTPUT("visible", "false\n");

    RUN("toggle", "true\n");
    waitFor(waitMsShow);
    WAIT_ON_OUTPUT("visible", "true\n");
}

void Tests::commandHide()
{
    RUN("visible", "true\n");
    RUN("hide", "");
    waitFor(waitMsShow);
    WAIT_ON_OUTPUT("visible", "false\n");
}

void Tests::commandShow()
{
    RUN("visible", "true\n");
    RUN("hide", "");
    waitFor(waitMsShow);
    WAIT_ON_OUTPUT("visible", "false\n");

    RUN("show", "");
    waitFor(waitMsShow);
    WAIT_ON_OUTPUT("visible", "true\n");
}

void Tests::commandShowAt()
{
    RUN("visible", "true\n");
    RUN("hide", "");
    waitFor(waitMsShow);
    WAIT_ON_OUTPUT("visible", "false\n");

    RUN("showAt", "");
    waitFor(waitMsShow);
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

void Tests::commandDialog()
{
    {
        PostponedProcess dialogThread(m_test, Args() << "keys" << ":TEST" << "ENTER");
        RUN("dialog" << "text", "TEST\n");
    }

    {
        PostponedProcess dialogThread(m_test, Args() << "keys" << "ESCAPE");
        RUN("eval" << "dialog('text') === undefined", "true\n");
    }

    {
        PostponedProcess dialogThread(m_test, Args() << "keys" << "ENTER");
        RUN("eval" << "dialog('.defaultChoice', 2, 'list', [1, 2, 3])", "2\n");
    }

    {
        PostponedProcess dialogThread(m_test, Args() << "keys" << "ENTER");
        RUN("eval" << "dialog('.defaultChoice', '', 'list', [1, 2, 3])", "\n");
    }

    {
        PostponedProcess dialogThread(m_test, Args() << "keys" << "ENTER");
        RUN("eval" << "dialog('list', [0, 1, 2])", "0\n");
    }
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
    WAIT_ON_OUTPUT("currentWindowTitle", "CopyQ\n");
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

    RUN("exportData" << fileName, "true\n");

    RUN("config" << "maxitems" << "1", "1\n");
    RUN("config" << "editor" << "EDITOR2 %1", "EDITOR2 %1\n");
    RUN("removetab" << tab1, "");
    RUN("tab" << tab2 << "add" << "1", "");

    RUN("importData" << fileName, "true\n");

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

void Tests::commandScreenshot()
{
    RUN("screenshot().size() > 0", "true\n")
}

void Tests::classFile()
{
    RUN("eval" <<
        "; var f = new File('/missing.file')"
        "; f.exists()"
        , "false\n"
        );
}

void Tests::classDir()
{
    RUN("eval" <<
        "; var d = new Dir('/missing_directory/')"
        "; d.exists()"
        , "false\n"
        );

    RUN("eval" << "Dir().homePath()" , QDir::homePath() + "\n");
}

void Tests::classTemporaryFile()
{
    RUN("eval" << "var f = new TemporaryFile(); f.open()", "true\n");

    for ( const auto autoRemove : {true, false} ) {
        QByteArray out;
        const auto cmd =
                QString(R"(
                        var f = new TemporaryFile()
                        if (!f.open())
                            throw 'Failed to open temporary file'
                        f.setAutoRemove(%1)
                        print(f.fileName())
                        )").arg(autoRemove);
        run( Args("eval") << cmd, &out );

        QFile f( QString::fromUtf8(out) );
        QVERIFY( f.exists() != autoRemove );

        if (!autoRemove)
            f.remove();
    }
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
    RUN("keys" << "CTRL+T", "");

    WAIT_ON_OUTPUT("currentWindowTitle", "CopyQ New Tab\n");

    RUN("keys" << "ESC", "");
    WAIT_ON_OUTPUT("currentWindowTitle", "CopyQ\n");
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

    // WORKAROUND: In Qt 4.8.7, selection is broken after moving items.
#if QT_VERSION < 0x050000
    RUN(args << "keys" << "UP" << "DOWN", "");
#endif

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
    RUN("keys" << "CTRL+T" << ":" + tab1 << "ENTER", "");
    RUN("testSelected", tab1 + "\n");
}

void Tests::editItems()
{
    RUN("config" << "edit_ctrl_return" << "true", "true\n");

    RUN("add" << "Line 4" << "Line 1", "");

    RUN("keys" << "F2" << "END" << "ENTER" << ":Line 2" << "F2", "");
    RUN("read" << "0", "Line 1\nLine 2");

    RUN("keys" << "DOWN", "");
    RUN("keys" << "F2" << "HOME" << ":Line 3" << "ENTER" << "F2", "");
    RUN("read" << "1", "Line 3\nLine 4");
    RUN("read" << "0", "Line 1\nLine 2");
}

void Tests::createNewItem()
{
    RUN("config" << "edit_ctrl_return" << "true", "true\n");

    RUN("keys" << "CTRL+N" << ":Line 1" << "ENTER" << ":Line 2" << "F2", "");
    RUN("read" << "0", "Line 1\nLine 2");

    RUN("keys" << "CTRL+N" << ":Line 3" << "ENTER" << ":Line 4" << "F2", "");
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
    const QString icon = ":/images/icon.svg";

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
            .arg(QApplication::applicationFilePath())
            .arg(editorTab)
            + " %1";
    RUN("config" << "editor" << cmd, cmd + "\n");

    // Set clipboard.
    const QByteArray data1 = generateData();
    TEST( m_test->setClipboard(data1) );
    RUN("clipboard", data1);

    QByteArray out;

    // Edit clipboard.
    RUN("edit" << "-1", "");

    // Get name of the filename to edit.
    WAIT_UNTIL(editorFileNameArgs, !out.isEmpty(), out);
    QFile file(out);
    QVERIFY( file.exists() );
    QVERIFY( file.open(QIODevice::ReadWrite) );
    QVERIFY( file.readAll() == data1 );

    // Modify clipboard.
    const QByteArray data2 = generateData();
    file.write(data2);
    file.close();

    // Close editor command.
    RUN(editorEndArgs, "");
    waitWhileFileExists(file);

    // Check if clipboard changed.
    WAIT_ON_OUTPUT("read" << "0", data1 + data2);
    RUN("clipboard", data1 + data2);

    // Edit existing item.
    const QString text =
            "Some text to edit,\n"
            "with second line!\n"
            + generateData();
    RUN(args << "add" << text, "");
    RUN(args << "edit" << "0", "");

    // Get name of the filename to edit.
    WAIT_UNTIL(editorFileNameArgs, !out.isEmpty(), out);
    file.setFileName(out);
    QVERIFY( file.exists() );
    QVERIFY( file.open(QIODevice::ReadWrite) );
    QVERIFY( file.readAll() == text.toUtf8() );

    // Modify first item.
    const QByteArray data3 = generateData();
    file.write(data3);
    file.close();

    // Close editor command.
    RUN(editorEndArgs, "");
    waitWhileFileExists(file);

    // Check first item.
    WAIT_ON_OUTPUT(args << "read" << "0", text.toUtf8() + data3);

    // Edit new item.
    RUN(args << "edit", "");

    // Get name of the filename to edit.
    WAIT_UNTIL(editorFileNameArgs, !out.isEmpty(), out);
    file.setFileName(out);
    QVERIFY( file.exists() );
    QVERIFY( file.open(QIODevice::ReadWrite) );
    QCOMPARE( file.readAll().data(), "" );

    // Modify first item.
    const QByteArray data4 = generateData();
    file.write(data4);
    file.close();

    // Close editor command.
    RUN(editorEndArgs, "");
    waitWhileFileExists(file);

    // Check first item.
    WAIT_ON_OUTPUT(args << "read" << "0", data4);
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

void Tests::openAndSavePreferences()
{
#ifdef Q_OS_MAC
    SKIP("Can't focus configuration checkboxes on OS X");
#endif

    RUN("config" << "check_clipboard" << "false", "false\n");

    // Open preferences dialog.
    RUN("keys" << ConfigTabShortcuts::tr("Ctrl+P"), "");

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
    const auto pasteFromMainWindowScript = QString(
                R"(
                show()
                sleep(%1)
                keys('ENTER')
                sleep(%1)
                keys('ENTER')
                )"
                ).arg(waitMsShow);
    PostponedProcess dialogThread(m_test, Args("eval") << pasteFromMainWindowScript);
    RUN("dialog" << "text", "TEST\n");
}

void Tests::tray()
{
#ifdef Q_OS_MAC
    SKIP("FIXME: This often fails on Travis CI for OS X.");
#endif

    RUN("add" << "A", "");
    RUN("menu", "");
    waitFor(waitMsShow);
    RUN("keys" << "ENTER", "");
    WAIT_FOR_CLIPBOARD("A");
}

void Tests::menu()
{
#ifdef Q_OS_MAC
    SKIP("FIXME: This often fails on Travis CI for OS X.");
#endif

    const auto tab = testTab(1);

    RUN("tab" << tab << "add" << "D" << "C" << "B" << "A", "");

    RUN("menu" << tab, "");
    waitFor(waitMsShow);
    RUN("keys" << "ENTER", "");
    WAIT_FOR_CLIPBOARD("A");

    // Show menu with 2 items from the tab and select last one.
    RUN("menu" << tab << "2", "");
    waitFor(waitMsShow);
    RUN("keys" << "END" << "ENTER", "");
    WAIT_FOR_CLIPBOARD("B");
}

void Tests::traySearch()
{
#ifdef Q_OS_MAC
    SKIP("FIXME: This often fails on Travis CI for OS X.");
#endif

    RUN("add" << "C" << "B" << "A", "");

    RUN("menu", "");
    waitFor(waitMsShow);
    RUN("keys" << "B" << "ENTER", "");
    WAIT_FOR_CLIPBOARD("B");
}

void Tests::configTrayTab()
{
#ifdef Q_OS_MAC
    SKIP("FIXME: This often fails on Travis CI for OS X.");
#endif

    const auto tab1 = testTab(1);
    RUN("tab" << tab1 << "add" << "A", "");

    const auto tab2 = testTab(2);
    RUN("tab" << tab2 << "add" << "B", "");

    RUN("config" << "tray_tab_is_current" << "false", "false\n");

    RUN("config" << "tray_tab" << tab1, tab1 + "\n");

    RUN("menu", "");
    waitFor(waitMsShow);
    RUN("keys" << "ENTER", "");
    WAIT_FOR_CLIPBOARD("A");

    RUN("config" << "tray_tab" << tab2, tab2 + "\n");

    RUN("menu", "");
    waitFor(waitMsShow);
    RUN("keys" << "ENTER", "");
    WAIT_FOR_CLIPBOARD("B");
}

void Tests::configMove()
{
#ifdef Q_OS_MAC
    SKIP("FIXME: This often fails on Travis CI for OS X.");
#endif

    RUN("add" << "B" << "A", "");

    RUN("config" << "move" << "true", "true\n");

    RUN("menu", "");
    waitFor(waitMsShow);
    RUN("keys" << "DOWN" << "ENTER", "");
    RUN("read" << "0" << "1", "B\nA");

    RUN("config" << "move" << "false", "false\n");

    RUN("menu", "");
    waitFor(waitMsShow);
    RUN("keys" << "DOWN" << "ENTER", "");
    RUN("read" << "0" << "1", "B\nA");
}

void Tests::configTrayTabIsCurrent()
{
#ifdef Q_OS_MAC
    SKIP("FIXME: This often fails on Travis CI for OS X.");
#endif

    const auto tab1 = testTab(1);
    RUN("tab" << tab1 << "add" << "A", "");

    const auto tab2 = testTab(2);
    RUN("tab" << tab2 << "add" << "B", "");

    RUN("config" << "tray_tab_is_current" << "true", "true\n");

    RUN("setCurrentTab" << tab1, "");
    RUN("menu", "");
    waitFor(waitMsShow);
    RUN("keys" << "ENTER", "");
    WAIT_FOR_CLIPBOARD("A");

    RUN("setCurrentTab" << tab2, "");
    RUN("menu", "");
    waitFor(waitMsShow);
    RUN("keys" << "ENTER", "");
    WAIT_FOR_CLIPBOARD("B");
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
    // Activate only one of the two actions depending on exit code of command which matches input MIME format.
    const auto script = R"(
        function cmd(name) {
          var format = 'application/x-copyq-' + name
          return {
            name: name,
            inMenu: true,
            shortcuts: ['Ctrl+F1'],
            matchCmd: 'copyq: str(data("' + format + '")) || fail()',
            cmd: 'copyq add ' + name
          }
        }
        setCommands([ cmd('test1'), cmd('test2') ])
        )";
    RUN(script, "");

    RUN("write" << "application/x-copyq-test1" << "1", "");
    waitFor(500);
    WAIT_ON_OUTPUT("keys('Ctrl+F1'); read(0)", "test1");
    RUN("tab" << QString(clipboardTabName) << "size", "2\n");

    RUN("write" << "application/x-copyq-test2" << "1", "");
    waitFor(500);
    WAIT_ON_OUTPUT("keys('Ctrl+F1'); read(0)", "test2");
    RUN("tab" << QString(clipboardTabName) << "size", "4\n");
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
    RUN("tab" << tab1 << "read" << "0", "C");
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
    RUN("read" << "2", "X");
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
    RUN("tab" << tab1 << "read" << "0", "B,C,");
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
    RUN("read" << "0", "A");
    RUN("read" << "1", "X");
    RUN("read" << "2", "Y");
}

void Tests::automaticCommandIgnore()
{
    RUN("setCommands([{automatic: true, cmd: 'copyq ignore; copyq add OK'}])", "");
    TEST( m_test->setClipboard("SHOULD BE IGNORED") );
    WAIT_ON_OUTPUT("read" << "0", "OK");
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

int Tests::run(const QStringList &arguments, QByteArray *stdoutData, QByteArray *stderrData, const QByteArray &in)
{
    return m_test->run(arguments, stdoutData, stderrData, in);
}

bool Tests::hasTab(const QString &tabName)
{
    QByteArray out;
    run(Args("tab"), &out);
    return QString::fromUtf8(out).split(QRegExp("\r\n|\n|\r")).contains(tabName);
}

int runTests(int argc, char *argv[])
{
    QRegExp onlyPlugins;
    bool runPluginTests = true;

    if (argc > 1) {
        QString arg = argv[1];
        if (arg.startsWith("PLUGINS:")) {
            arg.remove(QRegExp("^PLUGINS:"));
            onlyPlugins.setPattern(arg);
            onlyPlugins.setCaseSensitivity(Qt::CaseInsensitive);
            --argc;
            ++argv;
        } else {
            // Omit plugin tests if specific core tests requested.
            const QString lastArg(argv[argc - 1]);
            runPluginTests = lastArg.startsWith("-");
        }
    }

    QApplication app(argc, argv);
    int exitCode = 0;
    std::shared_ptr<TestInterfaceImpl> test(new TestInterfaceImpl);
    Tests tc(test);

    if (onlyPlugins.isEmpty()) {
        test->setupTest("CORE", QVariant());
        exitCode = QTest::qExec(&tc, argc, argv);
    }

    if (runPluginTests) {
        ItemFactory itemFactory;
        for ( const auto &loader : itemFactory.loaders() ) {
            if ( loader->id().contains(onlyPlugins) ) {
                std::unique_ptr<QObject> pluginTests( loader->tests(test) );
                if ( pluginTests != nullptr ) {
                    test->setupTest(loader->id(), pluginTests->property("CopyQ_test_settings"));
                    const int pluginTestsExitCode = QTest::qExec(pluginTests.get(), argc, argv);
                    exitCode = qMax(exitCode, pluginTestsExitCode);
                    test->stopServer();
                }
            }
        }
    }

    return exitCode;
}
