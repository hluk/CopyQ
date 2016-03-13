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

#include "tests.h"
#include "test_utils.h"

#include "app/remoteprocess.h"
#include "common/client_server.h"
#include "common/common.h"
#include "common/mimetypes.h"
#include "common/monitormessagecode.h"
#include "common/version.h"
#include "item/itemfactory.h"
#include "item/itemwidget.h"
#include "item/serialize.h"
#include "gui/configtabshortcuts.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QMimeData>
#include <QProcess>
#include <QRegExp>
#include <QScopedPointer>
#include <QTemporaryFile>
#include <QTest>
#include <QThread>

namespace {

bool testStderr(const QByteArray &stderrData, TestInterface::ReadStderrFlag flag = TestInterface::ReadErrors)
{
    const QRegExp scriptExceptionError("Script \\d\\+: Error:");
    static const QRegExp re(scriptExceptionError.pattern() + "|^CopyQ Warning|^CopyQ ERROR|ASSERT", Qt::CaseInsensitive);
    int from = 0;
    bool skipScriptException = flag == TestInterface::ReadErrorsWithoutScriptException;
    const QString output = QString::fromUtf8(stderrData);
    forever {
        from = output.indexOf(re, from);
        if (from == -1)
            return true;

        if ( skipScriptException && output.indexOf(scriptExceptionError, from) )
            skipScriptException = false;
        else
            return false;

        // Skip processed line.
        from = output.indexOf('\n', from);
        if (from == -1)
            return true;
    }
}

QByteArray getClipboard(const QString &mime = QString("text/plain"))
{
    QApplication::processEvents();
    const QMimeData *data = QApplication::clipboard()->mimeData();
    return (data != NULL) ? data->data(mime) : QByteArray();
}

bool waitUntilClipboardSet(const QByteArray &data, const QString &mime = QString("text/plain"))
{
    SleepTimer t(200);
    do {
        if (getClipboard(mime) == data)
            return true;
    } while (t.sleep());

    return false;
}

bool waitForProcessFinished(QProcess *p)
{
    // Process events in case we own clipboard and the new process requests the contents.
    SleepTimer t(8000);
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
        p->kill();
    }

    return false;
}

/// Generate unique data.
QByteArray generateData()
{
    static const QByteArray id = "tests_"
            + QByteArray::number(QDateTime::currentMSecsSinceEpoch() % 1000);
    static int i = 0;
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
        : m_server(NULL)
        , m_monitor(NULL)
        , m_env(QProcessEnvironment::systemEnvironment())
    {
    }

    ~TestInterfaceImpl()
    {
        stopServer();
    }

    QByteArray startServer()
    {
        if ( isServerRunning() )
            return "Server is already running.";

        if ( isAnyServerRunning() ) {
            qWarning() << "closing existing test session";
            run(Args("exit"));
            waitForAnyServerToQuit();
        }

        m_server.reset(new QProcess);

        if ( !startTestProcess(m_server.data(), QStringList(), QIODevice::ReadOnly) ) {
            return QString("Failed to launch \"%1\": %2")
                .arg(QApplication::applicationFilePath())
                .arg(m_server->errorString())
                .toUtf8();
        }

        // Wait for client/server communication is established.
        SleepTimer t(5000);
        while( !isServerRunning() && t.sleep() ) {}

        if ( !isServerRunning() )
            return "Unable to start server!" + readServerErrors(ReadAllStderr);

        QByteArray errors = readServerErrors();
        if (!errors.isEmpty())
            return errors;

        m_env.remove("COPYQ_TEST_SETTINGS");
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

    QByteArray stopServer()
    {
        QByteArray errors;
        const int exitCode = run(Args("exit"), NULL, &errors);
        if ( !testStderr(errors) || exitCode != 0 ) {
            return "Command 'exit' failed."
                    + printClienAndServerStderr(errors, exitCode);
        }

        if ( !m_server.isNull() && !closeProcess(m_server.data()) )
            return "Failed to close server properly!" + readServerErrors(ReadAllStderr);

        return waitForAnyServerToQuit();
    }

    bool isServerRunning()
    {
        return m_server != NULL && m_server->state() == QProcess::Running && isAnyServerRunning();
    }

    int run(const QStringList &arguments, QByteArray *stdoutData = NULL,
            QByteArray *stderrData = NULL, const QByteArray &in = QByteArray())
    {
        QProcess p;
        if (!startTestProcess(&p, arguments))
            return -1;

        if ( p.write(in) != in.size() )
            return -2;
        p.closeWriteChannel();

        if (stdoutData != NULL || stderrData != NULL) {
            if (stdoutData != NULL) {
                stdoutData->clear();
            }
            if (stderrData != NULL) {
                stderrData->clear();
            }

            // Read data for 2s - done manually to ensure the out/err buffers are always emptied
            SleepTimer t(2000);
            do {
                QByteArray out = p.readAllStandardOutput();
                QByteArray err = p.readAllStandardError();

                if (stdoutData != NULL)
                    stdoutData->append(out);
                if (stderrData != NULL)
                    stderrData->append(err);
            } while ( p.state() == QProcess::Running && t.sleep() );
        }

        if ( !closeProcess(&p) )
            return -3;

        if (stderrData != NULL) {
            stderrData->append(p.readAllStandardError());
            stderrData->replace('\r', "");
        }

        if (stdoutData != NULL) {
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
                         const QByteArray &input = QByteArray())
    {
        if ( isMainThread() && !isServerRunning() )
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
                                  const QByteArray &stderrContains = QByteArray())
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

    QByteArray setClipboard(const QByteArray &bytes, const QString &mime)
    {
        if (m_monitor == NULL) {
            m_monitor.reset(new RemoteProcess);
            const QString name = "copyq_TEST";
            m_monitor->start( name, QStringList("monitor") << name );

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

    QByteArray readServerErrors(ReadStderrFlag flag = ReadErrors)
    {
        if (isMainThread() && m_server) {
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

    QByteArray getClientOutput(const QStringList &arguments, QByteArray *stdoutActual)
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

    QByteArray cleanup()
    {
        if ( !isServerRunning() )
            return QByteArray();

        QByteArray out = runClient(Args("eval") <<
            // Create tab.
            "print(1);"
            "tab('CLIPBOARD');"
            "add('');"
            "config('clipboard_tab', 'CLIPBOARD');"

            // Remove test tabs.
            "print(2);"
            "tabs = tab();"
            "for (i in tabs) {"
            "  if (tabs[i] != 'CLIPBOARD' && tabs[i]) {"
            "    removetab(tabs[i]);"
            "  }"
            "}"

            // Clear items in first tab.
            "print(3);"
            "while (size() > 0) {"
            "  remove(0);"
            "}"

            "print(4);"
            "print(tab());"
            "print(5);"
            , "1234CLIPBOARD5");

        return out;
    }

    QByteArray show()
    {
        const QByteArray out = runClient(Args("show"), "");
        waitFor(waitMsShow);
        return out;
    }

    QByteArray init()
    {
        if ( !isServerRunning() ) {
            QByteArray errors = startServer();
            if ( !errors.isEmpty() )
                return "Failed to start server:\n" + errors;
        }

        // Always show main window first so that the results are consistent with desktop environments
        // where user cannot hide main window (tiling window managers without tray).
        QByteArray errors = show();
        if ( !errors.isEmpty() )
            return errors;

        cleanup();

        // Enable clipboard monitoring.
        errors = runClient(Args("config") << "check_clipboard" << "true", "");
        if ( !errors.isEmpty() )
            return errors;

        return runClient(Args("enable"), "");
    }

    QString shortcutToRemove()
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
        return run(Args("size")) == 0;
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

    QScopedPointer<QProcess> m_server;
    QScopedPointer<RemoteProcess> m_monitor; /// Process to provide clipboard set by tests.
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
}

void Tests::cleanupTestCase()
{
    TEST( m_test->stopServer() );
}

void Tests::init()
{
    TEST(m_test->init());
}

void Tests::cleanup()
{
    TEST( m_test->cleanup() );
}

void Tests::showHide()
{
    RUN("visible", "true\n");

    RUN("hide", "");
    waitFor(waitMsShow);
    WAIT_ON_OUTPUT("visible", "false\n");

    RUN("showAt", "");
    waitFor(waitMsShow);
    WAIT_ON_OUTPUT("visible", "true\n");

    RUN("toggle", "false\n");
    waitFor(waitMsShow);
    WAIT_ON_OUTPUT("visible", "false\n");

    RUN("toggle", "true\n");
    waitFor(waitMsShow);
    WAIT_ON_OUTPUT("visible", "true\n");

    RUN("hide", "");
    waitFor(waitMsShow);
    WAIT_ON_OUTPUT("visible", "false\n");

    RUN("show", "");
    waitFor(waitMsShow);
    WAIT_ON_OUTPUT("visible", "true\n");
}

void Tests::windowTitle()
{
    RUN("disable", "");
    WAIT_ON_OUTPUT("currentWindowTitle", "CopyQ\n");
    RUN("enable", "");
}

void Tests::keysAndFocusing()
{
    RUN("disable", "");
    RUN("keys" << "CTRL+T", "");
    WAIT_ON_OUTPUT("currentWindowTitle", "CopyQ New Tab\n");
    RUN("keys" << "ESC", "");
    WAIT_ON_OUTPUT("currentWindowTitle", "CopyQ\n");
    RUN("enable", "");
}

void Tests::firstItemSelectedByDefault()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab;
    RUN(args << "add" << "C" << "B" << "A", "");

    RUN(args << "read" << "0" << "1" << "2", "A\nB\nC");

    // focus next tab (Alt+1 may not work on some systems/desktops)
    RUN(args << "keys" << "RIGHT", "");
    RUN(args << "testSelected", tab + " 0 0\n");
}

void Tests::selectItems()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab;
    RUN(args << "add" << "C" << "B" << "A", "");

    RUN(args << "keys" << "RIGHT" << "SHIFT+DOWN" << "SHIFT+DOWN", "");
    RUN(args << "testSelected", tab + " 2 0 1 2\n");

    RUN(args << "keys" << "SHIFT+UP", "");
    RUN(args << "testSelected", tab + " 1 0 1\n");

    RUN(args << "keys" << "END", "");
    RUN(args << "testSelected", tab + " 2 2\n");

    RUN(args << "keys" << "SHIFT+UP", "");
    RUN(args << "testSelected", tab + " 1 1 2\n");

    RUN(args << "keys" << "CTRL+A", "");
    RUN(args << "testSelected", tab + " 1 0 1 2\n");
}

void Tests::moveItems()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab << "separator" << " ";
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
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab << "separator" << ",";
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
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab << "separator" << " ";

    RUN(args << "add" << "JKL" << "GHI" << "DEF" << "ABC", "");

    // search and delete
    RUN(args << "keys" << "RIGHT" << ":^[DJ]" << "TAB", "");
    waitFor(waitMsSearch);
    RUN(args << "testSelected", tab + " 1 1\n");

    // "Down" doesn't leave the search box on OS X
    RUN(args << "keys" << "CTRL+A" << m_test->shortcutToRemove() << "ESC", "");
    RUN(args << "read" << "0" << "1", "ABC GHI");
    RUN(args << "size", "2\n");
}

void Tests::copyItems()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab;

    RUN(args << "add" << "GHI" << "DEF" << "ABC", "");

    // copy items to new tab
    RUN(args << "keys" << "RIGHT" << "CTRL+A" << keyNameFor(QKeySequence::Copy) << "CTRL+T", "");
    const QString tab2 = testTab(2);
    RUN(args << "keys" << ":" + tab2 << "ENTER" << keyNameFor(QKeySequence::Paste), "");
    const Args args2 = Args("tab") << tab2;
    RUN(args2 << "read" << "0" << "1" << "2", "ABC\nDEF\nGHI");
    RUN(args2 << "size", "3\n");
}

void Tests::helpCommand()
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

    foreach (const QString &command, commands) {
        QCOMPARE( run(Args("help") << command, &stdoutActual, &stderrActual), 0 );
        QVERIFY2( testStderr(stderrActual), stderrActual );
        QVERIFY( !stdoutActual.isEmpty() );
        const QString help = QString::fromUtf8(stdoutActual);
        QVERIFY( help.contains(QRegExp("\\b" + QRegExp::escape(command) + "\\b")) );
    }
}

void Tests::versionCommand()
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
    QByteArray stdoutActual;
    QByteArray stderrActual;
    TEST( m_test->runClientWithError(Args("xxx"), 1, "xxx") );

    TEST( m_test->runClientWithError(Args("tab") << testTab(1) << "yyy", 1, "yyy") );

    // Bad command shoudn't create new tab.
    QCOMPARE( run(Args("tab"), &stdoutActual, &stderrActual), 0 );
    QVERIFY2( testStderr(stderrActual), stderrActual );
    QVERIFY( !QString::fromUtf8(stdoutActual)
             .contains(QRegExp("^" + QRegExp::escape(testTab(1)) + "$")) );
}

void Tests::copyCommand()
{
    const QByteArray output = "true\n";

    const QByteArray data1 = generateData();
    RUN( Args() << "copy" << QString::fromUtf8(data1), output );
    QVERIFY( waitUntilClipboardSet(data1) );
    RUN( Args("clipboard"), data1 );

    const QByteArray data2 = generateData();
    RUN( Args() << "copy" << "DATA" << QString::fromUtf8(data2), output );
    QVERIFY( waitUntilClipboardSet(data2, "DATA") );
    RUN( Args("clipboard") << "DATA", data2 );

    const QByteArray data3 = generateData();
    const QByteArray data4 = generateData();
    RUN( Args() << "copy"
         << "DATA3" << QString::fromUtf8(data3)
         << "DATA4" << QString::fromUtf8(data4)
         , output );
    QVERIFY( waitUntilClipboardSet(data3, "DATA3") );
    QVERIFY( waitUntilClipboardSet(data4, "DATA4") );
    RUN( Args("clipboard") << "DATA3", data3 );
    RUN( Args("clipboard") << "DATA4", data4 );
}

void Tests::dialogCommand()
{
    class Thread : public QThread {
    public:
        Thread(const TestInterfacePtr &test, const QStringList &args, const QByteArray &expectedOutput)
            : m_test(test)
            , m_args(args)
            , m_expectedOutput(expectedOutput)
        {
        }

        ~Thread() { wait(); }

    protected:
        void run() {
            RUN(m_args, m_expectedOutput);
        }

    private:
        TestInterfacePtr m_test;
        QStringList m_args;
        QByteArray m_expectedOutput;
    };

    {
        Thread dialogThread(m_test, Args() << "dialog" << "text", "TEST\n");
        dialogThread.start();
        waitFor(waitMsShow);
        RUN("keys" << ":TEST" << "ENTER", "");
    }

    {
        Thread dialogThread(m_test, Args() << "eval" << "dialog('text') === undefined", "true\n");
        dialogThread.start();
        waitFor(waitMsShow);
        RUN("keys" << "ESCAPE", "");
    }

    {
        Thread dialogThread(m_test, Args() << "eval" << "dialog('list', [2, 1, 2, 3])", "2\n");
        dialogThread.start();
        waitFor(waitMsShow);
        RUN("keys" << "ENTER", "");
    }

    {
        Thread dialogThread(m_test, Args() << "eval" << "dialog('list', [0, 1, 2])", "0\n");
        dialogThread.start();
        waitFor(waitMsShow);
        RUN("keys" << "ENTER", "");
    }
}

void Tests::createAndCopyNewItem()
{
    RUN("keys" << "CTRL+T", "");
    const QString tab = testTab(1);
    RUN("keys" << ":" + tab << "ENTER", "");

    const QStringList itemTexts = QStringList()
            << "Line 1\n"
               "Line 2"
            << "Line 3\n"
               "Line 4";

    RUN("config" << "edit_ctrl_return" << "true", "");

    foreach (const QString &itemText, itemTexts) {
        RUN("keys" << "CTRL+N", "");

        bool firstLine = true;
        foreach (const QString &itemLine, itemText.split('\n')) {
            if (firstLine)
                firstLine = false;
            else
                RUN("keys" << "ENTER", "");
            RUN("keys" << "CTRL+N" << ":" + itemLine, "");
        }

        RUN("keys" << "F2", "");

        RUN("tab" << tab << "read" << "0", itemText);

        RUN("keys" << keyNameFor(QKeySequence::Copy), "");
        QVERIFY( waitUntilClipboardSet(itemText.toUtf8()) );
        RUN("clipboard", itemText);
    }
}

void Tests::toggleClipboardMonitoring()
{
    const QByteArray data1 = generateData();
    TEST( m_test->setClipboard(data1) );
    RUN("clipboard", data1);
    RUN("read" << "0", data1);

    RUN("disable", "");
    RUN("monitoring", "false\n");

    const QByteArray data2 = generateData();
    TEST( m_test->setClipboard(data2) );
    RUN("clipboard", data2);
    RUN("read" << "0", data1);

    RUN("enable", "");
    RUN("monitoring", "true\n");

    const QByteArray data3 = generateData();
    TEST( m_test->setClipboard(data3) );
    RUN("clipboard", data3);
    RUN("read" << "0", data3);
}

void Tests::clipboardToItem()
{
    TEST( m_test->setClipboard("TEST0") );
    RUN("clipboard", "TEST0");

    TEST( m_test->setClipboard("TEST1") );
    RUN("clipboard", "TEST1");
    RUN("read" << "0", "TEST1");

    // NOTE: On Windows the "Fragment" parts are magically added - so it's easier to add them here.
    const QByteArray htmlBytes = "<!--StartFragment--><b>TEST2</b><!--EndFragment-->";
    TEST( m_test->setClipboard(htmlBytes, "text/html") );
    RUN("clipboard" << "text/html", htmlBytes.data());
    // This can be tested only if a plugin for HTML is loaded.
    //RUN("read" << "text/html" << "0", htmlBytes.data());

    // Unicode test.
    const QString test = QString::fromUtf8(QByteArray("Zkouška s různými českými znaky!"));
    const QByteArray bytes = test.toUtf8();
    TEST( m_test->setClipboard(bytes) );
    RUN("clipboard", bytes);
    RUN("read" << "0", bytes);
}

void Tests::itemToClipboard()
{
    RUN("add" << "TESTING2" << "TESTING1", "");
    RUN("read" << "0" << "1", "TESTING1\nTESTING2");

    RUN("select" << "0", "");

    QVERIFY( waitUntilClipboardSet("TESTING1") );
    RUN("clipboard", "TESTING1");

    // select second item and move to top
    RUN("config" << "move" << "true", "");
    RUN("select" << "1", "");
    RUN("read" << "0" << "1", "TESTING2\nTESTING1");

    QVERIFY( waitUntilClipboardSet("TESTING2") );
    RUN("clipboard", "TESTING2");

    // select without moving
    RUN("config" << "move" << "0", "");
    RUN("select" << "1", "");
    RUN("read" << "0" << "1", "TESTING2\nTESTING1");

    QVERIFY( waitUntilClipboardSet("TESTING1") );
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

    RUN(Args() << "removetab" << tab, "");
    QVERIFY( !hasTab(tab) );
}

void Tests::tabRemove()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab << "separator" << " ";

    RUN(args << "add" << "", "");
    QVERIFY( hasTab(tab) );
    RUN(Args() << "removetab" << tab, "");
    QVERIFY( !hasTab(tab) );

    TEST( m_test->runClientWithError(Args("removetab") << tab, 1) );
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
    QCOMPARE( run(Args(args) << "insert" << "1" << "-", NULL, NULL, in), 0);
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
    TEST( m_test->runClientWithError(Args("renametab") << tab1 << tab2, 1) );

    // Rename to same name.
    TEST( m_test->runClientWithError(Args("renametab") << tab2 << tab2, 1) );

    // Rename to empty name.
    TEST( m_test->runClientWithError(Args("renametab") << tab2 << "", 1) );

    // Rename to existing tab.
    TEST( m_test->runClientWithError(Args("renametab") << tab2 << "CLIPBOARD", 1) );

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

void Tests::eval()
{
    const QString tab1 = testTab(1);
    const QString tab2 = testTab(2);

    RUN("eval" << "print('123')", "123");

    TEST( m_test->runClientWithError(Args("eval") << "x", 1) );

    RUN("eval" << QString("tab('%1');add('abc');tab('%2');add('def');").arg(tab1).arg(tab2), "");
    RUN("eval" << QString("tab('%1');if (size() === 1) print('ok')").arg(tab1), "ok");
    RUN("eval" << QString("tab('%1');if (size() === 1) print('ok')").arg(tab2), "ok");
    RUN("eval" << QString("tab('%1');if (str(read(0)) === 'abc') print('ok')").arg(tab1), "ok");
    RUN("eval" << QString("tab('%1');if (str(read(0)) === 'def') print('ok')").arg(tab2), "ok");
}

void Tests::rawData()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab;

    {
        QByteArray input("\x00\x01\x02\x03\x04", 5);
        QString arg1 = QString::fromLatin1("\x01\x02\x03\x04");
        QString arg2 = QString::fromLatin1("\x7f\x6f\x5f\x4f");
        TEST( m_test->runClient(
                  Args(args) << "write"
                  << COPYQ_MIME_PREFIX "test1" << arg1
                  << COPYQ_MIME_PREFIX "test2" << "-"
                  << COPYQ_MIME_PREFIX "test3" << arg2, "",
                  input) );
        RUN(args << "read" << COPYQ_MIME_PREFIX "test1" << "0", arg1.toLatin1());
        RUN(args << "read" << COPYQ_MIME_PREFIX "test2" << "0", input);
        RUN(args << "read" << COPYQ_MIME_PREFIX "test3" << "0", arg2.toLatin1());
    }
}

void Tests::nextPrevious()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab;
    RUN(args << "add" << "C" << "B" << "A", "");

    RUN(args << "next", "");
    QVERIFY( waitUntilClipboardSet("B") );

    RUN(args << "next", "");
    QVERIFY( waitUntilClipboardSet("C") );

    RUN(args << "next", "");
    QVERIFY( waitUntilClipboardSet("C") );

    RUN(args << "previous", "");
    QVERIFY( waitUntilClipboardSet("B") );

    RUN(args << "previous", "");
    QVERIFY( waitUntilClipboardSet("A") );

    RUN(args << "previous", "");
    QVERIFY( waitUntilClipboardSet("A") );
}

void Tests::options()
{
    QByteArray stdoutActual;
    QByteArray stderrActual;
    QCOMPARE( run(Args("config"), &stdoutActual, &stderrActual), 0 );
    QVERIFY2( testStderr(stderrActual), stderrActual );
    QVERIFY( !stdoutActual.isEmpty() );

    // invalid option
    TEST( m_test->runClientWithError(Args("config") << "xxx", 1, "xxx") );

    QCOMPARE( run(Args("config") << "tab_tree", &stdoutActual, &stderrActual), 0 );
    QVERIFY2( testStderr(stderrActual), stderrActual );
    QVERIFY2( stdoutActual == "true\n" || stdoutActual == "false\n", stdoutActual);

    RUN("config" << "tab_tree" << "true", "");
    RUN("config" << "tab_tree", "true\n");

    RUN("config" << "tab_tree" << "false", "");
    RUN("config" << "tab_tree", "false\n");

    RUN("config" << "tab_tree" << "1", "");
    RUN("config" << "tab_tree", "true\n");

    RUN("config" << "tab_tree" << "0", "");
    RUN("config" << "tab_tree", "false\n");
}

void Tests::editCommand()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab;

    RUN("config" << "editor" << "", "");

    // Edit new item.
    const QByteArray data1 = generateData();
    RUN(args << "edit", "");
    RUN(args << "keys" << ":" + QString::fromLatin1(data1) << "F2", "");
    RUN(args << "read" << "0", data1);

    // Edit existing item.
    const QByteArray data2 = generateData();
    RUN(args << "edit" << "0", "");
    RUN(args << "keys" << "END" << ":" + QString::fromLatin1(data2) << "F2", "");
    RUN(args << "read" << "0", data1 + data2);
}

void Tests::externalEditor()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab;
    const QString editorTab = testTab(2);
    const Args editorArgs = Args("tab") << editorTab;
    const Args editorFileNameArgs = Args(editorArgs) << "read" << "0";
    const QString endEditor = "END_EDITOR";
    const Args editorEndArgs = Args(editorArgs) << "add" << endEditor;

    // Set edit command add first argument (filename to edit) to list.
    // Script ends when first item is "ABORT".
    QString cmd = QString(
                "\"%1\" tab \"%2\" eval "
                "\"add(arguments[1]);while(str(read(0)) != '%3');\" "
                "--")
            .arg(QApplication::applicationFilePath())
            .arg(editorTab)
            .arg(endEditor)
            + " %1";
    RUN("config" << "editor" << cmd, "");

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
    WAIT_UNTIL(editorEndArgs, !file.exists(), out);
    QCOMPARE(out.data(), "");
    RUN(editorArgs << "remove" << "0", "");

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
    WAIT_UNTIL(editorEndArgs, !file.exists(), out);
    QCOMPARE(out.data(), "");
    RUN(editorArgs << "remove" << "0", "");

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
    WAIT_UNTIL(editorEndArgs, !file.exists(), out);
    QCOMPARE(out.data(), "");
    RUN(editorArgs << "remove" << "0", "");

    // Check first item.
    WAIT_ON_OUTPUT(args << "read" << "0", data4);
}

void Tests::editNotes()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab;

    RUN(args << "add" << "XXX" << "YYY", "");

    RUN("keys" << keyNameFor(QKeySequence::NextChild), "");

    RUN(args << "read" << "?" << "0" << "1", "text/plain\n" "\n" "text/plain\n");

    const QString note1 = "Note 1";
    const QString note2 = "Note 2";
    RUN("keys" << "HOME" << "SHIFT+F2"
        << ":" + note1 << "ENTER" << ":" + note2 << "F2" , "");
    RUN(args << "read" << mimeItemNotes << "0", note1 + "\n" + note2);

    const QByteArray data1 = generateData();
    RUN(args << "write" << "1" << mimeItemNotes << data1, "");
    RUN(args << "size", "3\n");
    RUN(args << "read" << mimeItemNotes << "1", data1);

    const QByteArray data2 = generateData();
    const QByteArray data3 = generateData();

    RUN("keys" << "DOWN" << "SHIFT+F2"
        << "CTRL+A" << ":" + data2 << "ENTER" << ":" + data3 << "F2", "");

    RUN(args << "read" << mimeItemNotes << "1", data2 + "\n" + data3);
    RUN(args << "read" << mimeItemNotes << "2", "");
    RUN(args << "read" << mimeItemNotes << "0", note1 + "\n" + note2);
    RUN(args << "size", "3\n");
}

void Tests::exitCommand()
{
    for (int i = 0; i < 4; ++i) {
        const QByteArray data1 = generateData();
        RUN("add" << data1, "");
        RUN("keys" << "CTRL+C", "");

        TEST( m_test->stopServer() );
        QCOMPARE( run(Args("exit")), 1 );
        TEST( m_test->startServer() );

        TEST( m_test->stopServer() );
        QCOMPARE( run(Args("exit")), 1 );
        TEST( m_test->startServer() );

        RUN("show", "");
    }
}

void Tests::abortCommand()
{
    RUN("eval" << "print(1); abort(); print(2)", "1");
}

void Tests::nextPreviousTab()
{
    const QString script = QString(
                "tabs = [\"%1\", \"%2\"];"
                "for (i in tabs) {"
                "  tab(tabs[i]);"
                "  add('Item in tab ' + tabs[i]);"
                "}"
                )
            .arg(testTab(1))
            .arg(testTab(2));

    RUN("eval" << script, "");
    const QString testTabsList =
            "CLIPBOARD\n" +
            testTab(1) + "\n" +
            testTab(2) + "\n";
    RUN("tab", testTabsList);

    typedef QPair<QString, QString> KeyPair;
    const QList<KeyPair> keyPairs = QList<KeyPair>()
            << KeyPair(keyNameFor(QKeySequence::NextChild), keyNameFor(QKeySequence::PreviousChild))
            << KeyPair("RIGHT", "LEFT");

    foreach (const KeyPair &keyPair, keyPairs) {
        foreach (const QString &optionValue, QStringList() << "false" << "true") {
            RUN("config" << "tab_tree" << optionValue, "");
            RUN("config" << "tab_tree", optionValue + "\n");

            RUN("keys" << keyPair.first, "");
            RUN("testSelected", testTab(1) + " 0 0\n");
            RUN("keys" << keyPair.first, "");
            RUN("testSelected", testTab(2) + " 0 0\n");
            RUN("keys" << keyPair.first, "");

            RUN("keys" << "CTRL+T" << ":" + testTab(3) << "ENTER", "");
            RUN("testSelected", testTab(3) + "\n");
            RUN("removetab" << testTab(3), "");

            RUN("keys" << keyPair.second, "");
            RUN("testSelected", testTab(2) + " 0 0\n");
            RUN("keys" << keyPair.second, "");
            RUN("testSelected", testTab(1) + " 0 0\n");
            RUN("keys" << keyPair.second, "");
        }
    }
}

void Tests::openAndSavePreferences()
{
    const Args args = Args() << "config" << "check_clipboard";

    RUN(args << "true", "");
    RUN(args, "true\n");
    RUN(args << "false", "");
    RUN(args, "false\n");

    // Can't focus checkbox on OSX
#ifdef Q_OS_MAC
    SKIP("Can't focus configuration checkboxes on OS X");
#endif

    // Open preferences dialog.
    RUN("keys" << ConfigTabShortcuts::tr("Ctrl+P"), "");

    // Focus and set wrap text option.
    // This behavior could differ on some systems and in other languages.
    RUN("keys" << "ALT+1" << "ENTER", "");
    RUN(args, "true\n");

    RUN(args << "false", "");
    RUN(args, "false\n");
    RUN(args << "true", "");
    RUN(args, "true\n");
}

void Tests::tray()
{
    RUN("tab" << testTab(1) << "add" << "C" << "B" << "A", "");
    RUN("tab" << testTab(2) << "add" << "Z" << "Y" << "X", "");

    RUN("config" << "move" << "false", "");
    RUN("config" << "move", "false\n");
    RUN("config" << "tray_tab_is_current" << "false", "");
    RUN("config" << "tray_tab_is_current", "false\n");
    RUN("config" << "tray_tab" << testTab(1), "");
    RUN("config" << "tray_tab", testTab(1) + "\n");
    RUN("config" << "tray_items" << "3", "");
    RUN("config" << "tray_items", "3\n");

    RUN("menu", "");
    RUN("keys" << "DOWN" << "ENTER", "");
    QVERIFY( waitUntilClipboardSet("B") );
    RUN("clipboard", "B");

    RUN("config" << "tray_tab" << testTab(2), "");
    RUN("config" << "tray_tab", testTab(2) + "\n");

    RUN("menu", "");
    RUN("keys" << "DOWN" << "DOWN" << "ENTER", "");
    QVERIFY( waitUntilClipboardSet("Z") );
    RUN("clipboard", "Z");

    // Current tab in tray.
    RUN("config" << "tray_tab_is_current" << "true", "");
    RUN("config" << "tray_tab_is_current", "true\n");
    RUN("keys" << "RIGHT", "");

    RUN("menu", "");
    RUN("keys" << "DOWN" << "DOWN" << "ENTER", "");
    QVERIFY( waitUntilClipboardSet("C") );
    RUN("clipboard", "C");

    RUN("keys" << "RIGHT", "");

    RUN("menu", "");
    RUN("keys" << "ENTER", "");
    QVERIFY( waitUntilClipboardSet("X") );
    RUN("clipboard", "X");
}

void Tests::packUnpackCommands()
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
    foreach (const QByteArray &mime, data.keys())
        args2 << mime << data[mime];
    RUN(args2, "");

    const QByteArray script1 =
            "var data = read('" + toByteArray(mimeItems) + "', 0); var item = unpack(data);";

    // Unpack item read from list.
    foreach (const QByteArray &mime, data.keys()) {
        RUN(args << "eval"
            << script1 + "var mime = '" + mime + "'; print(mime + ':' + str(item[mime]))",
            mime + ':' + data[mime]);
    }

    // Test pack and unpack consistency.
    const QByteArray script2 = "data = pack(item); item = unpack(data);";
    foreach (const QByteArray &mime, data.keys()) {
        RUN(args << "eval"
            << script1 + script2 + "var mime = '" + mime + "'; print(mime + ':' + str(item[mime]))",
            mime + ':' + data[mime]);
    }
}

void Tests::base64Commands()
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

void Tests::getSetItemCommands()
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
    foreach (const QByteArray &mime, data.keys())
        args2 << mime << data[mime];
    RUN(args2, "");

    // Get item from list.
    foreach (const QByteArray &mime, data.keys()) {
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

void Tests::escapeHTMLCommand()
{
    RUN("escapeHTML" << "&\n<\n>", "&amp;<br />&lt;<br />&gt;\n");
}

void Tests::executeCommand()
{
    const QString tab = testTab(1);

    const QByteArray script =
            "function test(c, expected_stdout, expected_exit_code) {"
            "    if (str(c.stdout) !== expected_stdout) print('Unexpected stdout: \"' + str(c.stdout) + '\"');"
            "    if (c.exit_code !== expected_exit_code) print('Unexpected exit_code: ' + str(c.exit_code));"
            "}";

    RUN("eval" << script +
        "c = execute('copyq', 'tab', '" + tab + "', 'write', 'text/plain', 'plain text', 'text/html', '<b>test HTML</b>');"
        "test(c, '', 0);"
        , "");

    RUN("eval" << script +
        "c = execute('copyq', 'tab', '" + tab + "', 'read', 'text/plain', 0);"
        "test(c, 'plain text', 0);"
        , "");

    RUN("eval" << script +
        "c = execute('copyq', 'tab', '" + tab + "', 'read', 'text/html', 0);"
        "test(c, '<b>test HTML</b>', 0);"
        , "");
}

void Tests::settingsCommand()
{
    RUN("config" << "clipboard_tab" << "TEST", "");
    RUN("config" << "clipboard_tab", "TEST\n");

    RUN("settings" << "test_variable", "");
    RUN("settings" << "test_variable" << "TEST VALUE", "");
    RUN("settings" << "test_variable", "TEST VALUE\n");
    RUN("settings" << "test_variable" << "TEST VALUE 2", "");
    RUN("settings" << "test_variable", "TEST VALUE 2\n");

    RUN("config" << "clipboard_tab", "TEST\n");
}

void Tests::fileClass()
{
    RUN("eval" <<
        "; var f = new File('/missing.file')"
        "; f.exists()"
        , "false\n"
        );
}

void Tests::dirClass()
{
    RUN("eval" <<
        "; var d = new Dir('/missing_directory/')"
        "; d.exists()"
        , "false\n"
        );

    RUN("eval" << "Dir().homePath()" , QDir::homePath() + "\n");
}

void Tests::setEnvCommand()
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
    QSharedPointer<TestInterfaceImpl> test(new TestInterfaceImpl);
    Tests tc(test);

    if (onlyPlugins.isEmpty()) {
        test->setupTest("CORE", QVariant());
        exitCode = QTest::qExec(&tc, argc, argv);
    }

    if (runPluginTests) {
        ItemFactory itemFactory;
        foreach( const ItemLoaderInterface *loader, itemFactory.loaders() ) {
            if ( loader->id().contains(onlyPlugins) ) {
                QScopedPointer<QObject> pluginTests( loader->tests(test) );
                if ( !pluginTests.isNull() ) {
                    test->setupTest(loader->id(), pluginTests->property("CopyQ_test_settings"));
                    const int pluginTestsExitCode = QTest::qExec(pluginTests.data(), argc, argv);
                    exitCode = qMax(exitCode, pluginTestsExitCode);
                    test->stopServer();
                }
            }
        }
    }

    return exitCode;
}
