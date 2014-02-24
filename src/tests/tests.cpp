/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

#include "app/remoteprocess.h"
#include "common/client_server.h"
#include "common/common.h"
#include "item/clipboarditem.h"
#include "item/itemfactory.h"
#include "item/itemwidget.h"
#include "item/serialize.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMimeData>
#include <QProcess>
#include <QScopedPointer>
#include <QTemporaryFile>
#include <QTest>

#define VERIFY_SERVER_OUTPUT() \
do { \
    QByteArray errors = m_test->readServerErrors(); \
    QVERIFY2(errors.isEmpty(), errors); \
} while (0)

#define RUN(arguments, stdoutExpected) \
    TEST( m_test->runClient(arguments, stdoutExpected) );

#define WAIT_UNTIL(arguments, CONDITION, stdoutActual) \
do { \
    QElapsedTimer t; \
    t.start(); \
    bool success = false; \
    while(true) { \
        stdoutActual.clear(); \
        waitFor(200); \
        QVERIFY( isServerRunning() ); \
        QByteArray stderrActual; \
        QCOMPARE( run(arguments, &stdoutActual, &stderrActual), 0 ); \
        stdoutActual.replace('\r', ""); \
        QVERIFY2( testStderr(stderrActual), stderrActual ); \
        VERIFY_SERVER_OUTPUT(); \
        success = CONDITION; \
        if (success || t.elapsed() > 8000) \
            break; \
    } \
    QVERIFY(success); \
} while (0)

namespace {

/// Interval to wait (in ms) until window is shown and focused.
const int waitMsShow = 250;

/// Interval to wait (in ms) until search is complete.
const int waitMsSearch = 250;

typedef QStringList Args;

void waitFor(int ms)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms)
        QApplication::processEvents(QEventLoop::AllEvents, ms);
}

/// Naming scheme for test tabs in application.
QString testTab(int index)
{
    return QString("TEST_&%1").arg(index);
}

bool testStderr(const QByteArray &stderrData)
{
    return !stderrData.contains("warning: ") && !stderrData.contains("ERROR: ") && !stderrData.contains("Error: ");
}

QByteArray getClipboard(const QString &mime = QString("text/plain"))
{
    QApplication::processEvents();
    const QMimeData *data = QApplication::clipboard()->mimeData();
    return (data != NULL) ? data->data(mime) : QByteArray();
}

bool waitUntilClipboardSet(const QByteArray &data, const QString &mime = QString("text/plain"))
{
    QElapsedTimer t;
    t.start();

    while ( t.elapsed() < 4000 ) {
        waitFor(200);
        if (getClipboard(mime) == data)
            return true;
    }

    return false;
}

bool waitForProcessFinished(QProcess *p)
{
    // Process events in case we own clipboard and the new process requests the contens.
    for ( int i = 0; i < 100 && !p->waitForFinished(200); ++i )
        QApplication::processEvents();

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
QByteArray generateData(const QByteArray &data)
{
    static int i = 0;
    return data + '_' + QByteArray::number(QDateTime::currentMSecsSinceEpoch())
            + '_' + QByteArray::number(++i);
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

    bool startServer()
    {
        m_server.reset(new QProcess);

        if ( !startTestProcess(m_server.data(), QStringList(), QIODevice::ReadOnly) ) {
            log( QString("Failed to launch \"%1\": %2")
                 .arg(QApplication::applicationFilePath())
                 .arg(m_server->errorString()),
                 LogError );
            return false;
        }

        // Wait for client/server communication is established.
        int tries = 0;
        while( !isServerRunning() && ++tries <= 50 )
            waitFor(200);
        waitFor(1000);

        if (!isServerRunning())
            return false;

        m_env.remove("COPYQ_TEST_SETTINGS");
        return true;
    }

    bool stopServer()
    {
        if ( m_server.isNull() )
            return !isServerRunning();

        if ( run(Args("exit")) != 0 )
            return false;

        if ( !closeProcess(m_server.data()) )
            return false;

        return !isAnyServerRunning();
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

        if ( !closeProcess(&p) )
            return -3;

        if (stdoutData != NULL)
            *stdoutData = p.readAllStandardOutput();
        if (stderrData != NULL)
            *stderrData = p.readAllStandardError();

        return p.exitCode();
    }

    QByteArray runClient(const QStringList &arguments, const QByteArray &stdoutExpected)
    {
        QByteArray errors = readServerErrors();
        if (!errors.isEmpty())
            return errors;

        QByteArray stdoutActual;
        QByteArray stderrActual;
        const int exitCode = run(arguments, &stdoutActual, &stderrActual);

        if ( !testStderr(stderrActual) )
            return stderrActual;

        if (exitCode != 0) {
            return "Test failed: Exit code is " + QByteArray::number(exitCode)
                    + "\nError output is:\n" + stderrActual;
        }

        stdoutActual.replace('\r', "");
        if (stdoutActual != stdoutExpected) {
            return "Test failed: Unexpected output:\n" + stdoutActual
                    + "\nExpected output was:\n"
                    + stdoutExpected;
        }

        errors = readServerErrors();
        return errors;
    }

    void setClipboard(const QByteArray &bytes, const QString &mime)
    {
        if (m_monitor == NULL) {
            m_monitor.reset(new RemoteProcess);
            const QString name = "copyq_TEST";
            m_monitor->start( name, QStringList("monitor") << name );

            QElapsedTimer t;
            t.start();
            while( !m_monitor->isConnected() && t.elapsed() < 4000 )
                waitFor(200);
        }

        QVERIFY( m_monitor->isConnected() );

        const QVariantMap data = createDataMap(mime, bytes);
        QVERIFY( m_monitor->writeMessage(serializeData(data)) );

        waitUntilClipboardSet(bytes, mime);
        QCOMPARE( getClipboard(mime), bytes );
        waitFor(200);
    }

    QByteArray readServerErrors()
    {
        if (m_server) {
            const QByteArray output = m_server->readAllStandardError();
            return testStderr(output) ? QByteArray() : output;
        }

        if ( isServerRunning() )
            return "Test ERROR: Server is not running!";

        return QByteArray();
    }

    QByteArray cleanup()
    {
        QByteArray out = runClient(Args("eval") <<
            // Create tab.
            "tab('CLIPBOARD');"
            "add('');"

            // Remove test tabs.
            "tabs = tab().split('\\\\n');"
            "for (i in tabs) {"
            "  if (tabs[i] != 'CLIPBOARD' && tabs[i]) {"
            "    removetab(tabs[i]);"
            "  }"
            "}"

            // Clear items in first tab.
            "while (size() > 0) {"
            "  remove(0);"
            "}"

            "print(tab());"
            , "CLIPBOARD\n");

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
        if (!m_server && isAnyServerRunning() )
            return "Other test server is running!";

        if ( !isServerRunning() && !startServer() )
            return "Failed to start server!";

        QByteArray out = readServerErrors();
        if ( !out.isEmpty() )
            return out;

        // Always show main window first so that the results are consistent with desktop environments
        // where user cannot hide main window (tiling window managers without tray).
        out = show();
        if ( !out.isEmpty() )
            return out;

        // Enable clipboard monitoring.
        out = runClient(Args("enable"), "");

        return out;
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

    QScopedPointer<QProcess> m_server;
    QScopedPointer<RemoteProcess> m_monitor; /// Process to provide clipboard set by tests.
    QProcessEnvironment m_env;
    QVariantMap m_settings;
};

} // namespace

Tests::Tests(const TestInterfacePtr &test, QObject *parent)
    : QObject(parent)
    , m_test(test)
{
}

void Tests::initTestCase()
{
    TEST(m_test->init());
    cleanup();
}

void Tests::cleanupTestCase()
{
    QVERIFY( stopServer() );
}

void Tests::init()
{
    TEST(m_test->init());
}

void Tests::cleanup()
{
    const QByteArray out = m_test->cleanup();
    QVERIFY2( out.isEmpty(), out );
}

void Tests::moveAndDeleteItems()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab;
    RUN(Args(args) << "add" << "A" << "B" << "C", "");

    RUN(Args(args) << "read" << "0" << "1" << "2", "C\nB\nA");

    // focus test tab by deleting (Alt+1 may not work on some systems/desktops)
    RUN(Args(args) << "keys" << "RIGHT", "");
    RUN(Args(args) << "selectedtab", tab.toLocal8Bit() + "\n");

    // select first item
    RUN(Args(args) << "keys" << "HOME", "");
    RUN(Args(args) << "selectedtab", tab.toLocal8Bit() + "\n");
    RUN(Args(args) << "selecteditems", "0\n0\n");
    RUN(Args(args) << "selected", tab.toLocal8Bit() + "\n0\n0\n");

    // delete first item
    RUN(Args(args) << "keys" << m_test->shortcutToRemove(), "");
    RUN(Args(args) << "read" << "0" << "1", "B\nA");

    // move item one down
    RUN(Args(args) << "keys" << "CTRL+DOWN", "");
    RUN(Args(args) << "read" << "0", "A");
    RUN(Args(args) << "read" << "1", "B");

    // select first item
    RUN(Args("config") << "move" << "false", "");
    RUN(Args("config") << "activate_closes" << "false", "");
    RUN(Args("config") << "activate_focuses" << "false", "");
    RUN(Args("config") << "activate_pastes" << "false", "");
    RUN(Args(args) << "keys" << "UP" << "ENTER", "");
    QVERIFY( waitUntilClipboardSet("A") );
    RUN(Args(args) << "clipboard", "A");

    // select all and delete
    RUN(Args(args) << "keys" << "CTRL+A" << m_test->shortcutToRemove(), "");
    RUN(Args(args) << "size", "0\n");

    RUN(Args(args) << "add" << "ABC" << "DEF" << "GHI" << "JKL", "");

    // search and delete
    RUN(Args(args) << "keys" << ":[AG]", "");
    waitFor(waitMsSearch);
#ifdef Q_OS_MAC
    // "Down" doesn't leave the search box on OS X
    RUN(Args(args) << "keys" << "TAB" << "CTRL+A" << m_test->shortcutToRemove(), "");
#else
    RUN(Args(args) << "keys" << "DOWN" << "CTRL+A" << m_test->shortcutToRemove(), "");
#endif // Q_OS_MAC
    RUN(Args(args) << "read" << "0", "JKL");
    RUN(Args(args) << "read" << "1", "DEF");
    RUN(Args(args) << "size", "2\n");
    RUN(Args(args) << "keys" << "ESCAPE", "");

    // copy items to new tab
    RUN(Args(args) << "keys" << "CTRL+A" << QKeySequence(QKeySequence::Copy).toString(), "");
    RUN(Args(args) << "keys" << "CTRL+T", "");
    const QString tab2 = testTab(2);
    RUN(Args(args) << "keys" << ":" + tab2 << "ENTER", "");
    RUN(Args(args) << "keys" << QKeySequence(QKeySequence::Paste).toString(), "");
    const Args args2 = Args("tab") << tab2;
    RUN(Args(args2) << "read" << "0", "JKL");
    RUN(Args(args2) << "read" << "1", "DEF");
    RUN(Args(args2) << "size", "2\n");
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
        const QString help = QString::fromLocal8Bit(stdoutActual);
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

    const QString version = QString::fromLocal8Bit(stdoutActual);
    // Version contains application name and version.
    QVERIFY( version.contains(QRegExp("\\bCopyQ\\b.*v" + QRegExp::escape(COPYQ_VERSION))) );
    // Version contains Qt version.
    QVERIFY( version.contains(QRegExp("\\bQt\\s+\\d")) );
    // Version contains Qxt version.
    QVERIFY( version.contains(QRegExp("\\bLibQxt\\s+\\d")) );
}

void Tests::badCommand()
{
    QByteArray stdoutActual;
    QByteArray stderrActual;
    QCOMPARE( run(Args("xxx"), &stdoutActual, &stderrActual), 2 );
    QVERIFY( !stderrActual.isEmpty() );
    QVERIFY( stdoutActual.isEmpty() );
    QVERIFY( stderrActual.contains("xxx") );

    QCOMPARE( run(Args("tab") << testTab(1) << "yyy", &stdoutActual, &stderrActual), 2 );
    QVERIFY( !stderrActual.isEmpty() );
    QVERIFY( stdoutActual.isEmpty() );
    QVERIFY( stderrActual.contains("yyy") );

    // Bad command shoudn't create new tab.
    QCOMPARE( run(Args("tab"), &stdoutActual, &stderrActual), 0 );
    QVERIFY2( testStderr(stderrActual), stderrActual );
    QVERIFY( !QString::fromLocal8Bit(stdoutActual)
             .contains("^" + QRegExp::escape(testTab(1)) + "$") );
}

void Tests::copyCommand()
{
    const QByteArray data = "copyCommand";

    const QByteArray data1 = generateData(data);
    RUN( Args() << "copy" << QString::fromLocal8Bit(data1), "" );
    QVERIFY( waitUntilClipboardSet(data1) );
    RUN( Args("clipboard"), data1 );

    const QByteArray data2 = generateData(data);
    RUN( Args() << "copy" << "DATA" << QString::fromLocal8Bit(data2), "" );
    QVERIFY( waitUntilClipboardSet(data2, "DATA") );
    RUN( Args("clipboard") << "DATA", data2 );

    const QByteArray data3 = generateData(data);
    const QByteArray data4 = generateData(data);
    RUN( Args() << "copy"
         << "DATA3" << QString::fromLocal8Bit(data3)
         << "DATA4" << QString::fromLocal8Bit(data4)
         , "" );
    QVERIFY( waitUntilClipboardSet(data3, "DATA3") );
    QVERIFY( waitUntilClipboardSet(data4, "DATA4") );
    RUN( Args("clipboard") << "DATA3", data3 );
    RUN( Args("clipboard") << "DATA4", data4 );
}

void Tests::createAndCopyNewItem()
{
    RUN(Args() << "keys" << "CTRL+T", "");
    const QString tab = testTab(1);
    RUN(Args() << "keys" << ":" + tab << "ENTER", "");

    const QStringList itemTexts = QStringList()
            << "New item with random text and\n"
               "useless second line!"
            << "Second item with another random text and\n"
               "another useless second line!";

    RUN(Args() << "config" << "edit_ctrl_return" << "true", "");

    foreach (const QString &itemText, itemTexts) {
        RUN(Args() << "keys" << "CTRL+N", "");

        bool firstLine = true;
        foreach (const QString &itemLine, itemText.split('\n')) {
            if (firstLine)
                firstLine = false;
            else
                RUN(Args() << "keys" << "ENTER", "");
            RUN(Args() << "keys" << "CTRL+N" << ":" + itemLine, "");
        }

        RUN(Args() << "keys" << "F2", "");

        RUN(Args() << "tab" << tab << "read" << "0", itemText.toLocal8Bit());

        RUN(Args() << "keys" << QKeySequence(QKeySequence::Copy).toString(), "");
        QVERIFY( waitUntilClipboardSet(itemText.toLocal8Bit()) );
        RUN(Args("clipboard"), itemText.toLocal8Bit());
    }
}

void Tests::toggleClipboardMonitoring()
{
    const QByteArray data = "toggleClipboardMonitoring";
    setClipboard(data);
    RUN(Args("clipboard"), data);

    const QByteArray data1 = generateData(data);
    setClipboard(data1);
    RUN(Args("clipboard"), data1);
    RUN(Args("read") << "0", data1);

    RUN(Args("disable"), "");

    const QByteArray data2 = generateData(data);
    setClipboard(data2);
    RUN(Args("clipboard"), data2);
    RUN(Args("read") << "0", data1);

    RUN(Args("enable"), "");

    const QByteArray data3 = generateData(data);
    setClipboard(data3);
    RUN(Args("clipboard"), data3);
    RUN(Args("read") << "0", data3);
}

void Tests::clipboardToItem()
{
    setClipboard("TEST0");
    RUN(Args("clipboard"), "TEST0");

    setClipboard("TEST1");
    RUN(Args("clipboard"), "TEST1");
    RUN(Args("read") << "0", "TEST1");

    const QByteArray htmlBytes = "<b>TEST2</b>";
    setClipboard(htmlBytes, "text/html");
    RUN(Args("clipboard") << "text/html", htmlBytes.data());
    RUN(Args("read") << "text/html" << "0", htmlBytes.data());

    // Unicode test.
    const QString test = QString::fromUtf8(QByteArray("Zkouška s různými českými znaky!"));
    const QByteArray bytes = test.toUtf8();
    setClipboard(bytes);
    RUN(Args("clipboard"), bytes);
    RUN(Args("read") << "0", bytes);
}

void Tests::itemToClipboard()
{
    RUN(Args("add") << "TESTING1" << "TESTING2", "");
    RUN(Args("read") << "0", "TESTING2");
    RUN(Args("read") << "1", "TESTING1");

    RUN(Args("select") << "0", "");

    QVERIFY( waitUntilClipboardSet("TESTING2") );
    RUN(Args("clipboard"), "TESTING2");

    // select second item and move to top
    RUN(Args("config") << "move" << "true", "");
    RUN(Args("select") << "1", "");
    RUN(Args("read") << "0", "TESTING1");
    RUN(Args("read") << "1", "TESTING2");

    QVERIFY( waitUntilClipboardSet("TESTING1") );
    RUN(Args("clipboard"), "TESTING1");

    // select without moving
    RUN(Args("config") << "move" << "0", "");
    RUN(Args("select") << "1", "");
    RUN(Args("read") << "0", "TESTING1");
    RUN(Args("read") << "1", "TESTING2");

    QVERIFY( waitUntilClipboardSet("TESTING2") );
    RUN(Args("clipboard"), "TESTING2");
}

void Tests::tabAddRemove()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab;

    QVERIFY( !hasTab(tab) );
    RUN(args, "");
    RUN(Args(args) << "size", "0\n");
    RUN(Args(args) << "add" << "abc", "");
    QVERIFY( hasTab(tab) );

    RUN(Args(args) << "add" << "def" << "ghi", "");
    RUN(Args(args) << "size", "3\n");
    RUN(Args(args) << "read" << "0", "ghi");
    RUN(Args(args) << "read" << "1", "def");
    RUN(Args(args) << "read" << "2", "abc");
    RUN(Args(args) << "read" << "0" << "2" << "1", "ghi\nabc\ndef");

    // Restart server.
    QVERIFY( stopServer() );
    QVERIFY( startServer() );

    QVERIFY( hasTab(tab) );

    RUN(Args(args) << "read" << "0" << "2" << "1", "ghi\nabc\ndef");
    RUN(Args(args) << "size", "3\n");
}

void Tests::action()
{
    const Args args = Args("tab") << testTab(1);
    const Args argsAction = Args(args) << "action";
    const QString action = QString("%1 %2 %3").arg(QApplication::applicationFilePath())
                                              .arg(args.join(" "));

    QByteArray out;

    // action with size
    RUN(Args(argsAction) << action.arg("size") << "", "");
    WAIT_UNTIL(Args(args) << "size", out == "1\n", out);
    RUN(Args(args) << "read" << "0", "0\n");

    // action with size
    RUN(Args(argsAction) << action.arg("size") << "", "");
    WAIT_UNTIL(Args(args) << "size", out == "2\n", out);
    RUN(Args(args) << "read" << "0", "1\n");

    // action with eval print
    RUN(Args(argsAction) << action.arg("eval 'print(\"A,B,C\")'") << "", "");
    WAIT_UNTIL(Args(args) << "size", out == "3\n", out);
    RUN(Args(args) << "read" << "0", "A,B,C");

    // action with read and comma separator for new items
    RUN(Args(argsAction) << action.arg("read 0") << ",", "");
    WAIT_UNTIL(Args(args) << "size", out == "6\n", out);
    RUN(Args(args) << "read" << "0", "C");
    RUN(Args(args) << "read" << "1", "B");
    RUN(Args(args) << "read" << "2", "A");
}

void Tests::insertRemoveItems()
{
    const Args args = Args("tab") << testTab(1);

    RUN(Args(args) << "add" << "abc" << "ghi", "");
    RUN(Args(args) << "read" << "0", "ghi");
    RUN(Args(args) << "read" << "1", "abc");

    RUN(Args(args) << "insert" << "1" << "def", "");
    RUN(Args(args) << "read" << "0", "ghi");
    RUN(Args(args) << "read" << "1", "def");
    RUN(Args(args) << "read" << "2", "abc");

    RUN(Args(args) << "insert" << "0" << "012", "");
    RUN(Args(args) << "read" << "0", "012");
    RUN(Args(args) << "read" << "1", "ghi");
    RUN(Args(args) << "read" << "2", "def");
    RUN(Args(args) << "read" << "3", "abc");

    RUN(Args(args) << "remove" << "0" << "2", "");
    RUN(Args(args) << "read" << "0", "ghi");
    RUN(Args(args) << "read" << "1", "abc");

    QByteArray in("ABC");
    QCOMPARE( run(Args(args) << "insert" << "1" << "-", NULL, NULL, in), 0);
    RUN(Args(args) << "read" << "0", "ghi");
    RUN(Args(args) << "read" << "1", "ABC");
    RUN(Args(args) << "read" << "2", "abc");

    RUN(Args(args) << "read" << "3", "");
}

void Tests::renameTab()
{
    const QString tab1 = testTab(1);
    const QString tab2 = testTab(2);
    const QString tab3 = testTab(3);

    RUN(Args("tab") << tab1 << "add" << "abc" << "def" << "ghi", "");
    RUN(Args("tab") << tab1 << "size", "3\n");
    RUN(Args("tab") << tab1 << "read" << "0" << "1" << "2", "ghi\ndef\nabc");

    RUN(Args("renametab") << tab1 << tab2, "");
    RUN(Args("tab") << tab2 << "size", "3\n");
    RUN(Args("tab") << tab2 << "read" << "0" << "1" << "2", "ghi\ndef\nabc");
    QVERIFY( !hasTab(tab1) );

    QByteArray stderrData;
    // Rename non-existing tab.
    QCOMPARE( run(Args("renametab") << tab1 << tab2, NULL, &stderrData), 1 );
    QVERIFY( !stderrData.isEmpty() );
    // Rename to same name.
    QCOMPARE( run(Args("renametab") << tab2 << tab2, NULL, &stderrData), 1 );
    QVERIFY( !stderrData.isEmpty() );
    // Rename to empty name.
    QCOMPARE( run(Args("renametab") << tab2 << "", NULL, &stderrData), 1 );
    QVERIFY( !stderrData.isEmpty() );
    // Rename to existing tab.
    RUN(Args("tab") << tab3 << "add" << "xxx", "");
    QCOMPARE( run(Args("renametab") << tab2 << tab3, NULL, &stderrData), 1 );
    QVERIFY( !stderrData.isEmpty() );

    QVERIFY( !hasTab(tab1) );
    QVERIFY( hasTab(tab2) );
    QVERIFY( hasTab(tab3) );

    RUN(Args("renametab") << tab2 << tab1, "");
    RUN(Args("tab") << tab1 << "read" << "0" << "1" << "2", "ghi\ndef\nabc");

    QVERIFY( hasTab(tab1) );
    QVERIFY( !hasTab(tab2) );
    QVERIFY( hasTab(tab3) );
}

void Tests::importExportTab()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab;

    RUN(Args(args) << "add" << "abc" << "def" << "ghi", "");
    RUN(Args(args) << "read" << "0" << "1" << "2", "ghi\ndef\nabc");

    QTemporaryFile tmp;
    QVERIFY(tmp.open());
    RUN(Args(args) << "exporttab" << tmp.fileName(), "");

    RUN(Args("removetab") << tab, "");
    QVERIFY( !hasTab(tab) );

    RUN(Args(args) << "importtab" << tmp.fileName(), "");
    RUN(Args(args) << "read" << "0" << "1" << "2", "ghi\ndef\nabc");

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

    RUN(Args(args) << "add" << "jkl", "");
    RUN(Args(args) << "exporttab" << fileName, "");

    RUN(Args("removetab") << tab, "");
    QVERIFY( !hasTab(tab) );

    RUN(Args(args) << "importtab" << fileName, "");
    RUN(Args(args) << "read" << "0" << "1" << "2" << "3", "jkl\nghi\ndef\nabc");
}

void Tests::separator()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab;

    RUN(Args(args) << "add" << "abc" << "def" << "ghi", "");
    RUN(Args(args) << "read" << "0" << "1" << "2", "ghi\ndef\nabc");
    RUN(Args(args) << "separator" << "," << "read" << "0" << "1" << "2", "ghi,def,abc");
    RUN(Args(args) << "separator" << "---" << "read" << "0" << "1" << "2", "ghi---def---abc");
}

void Tests::eval()
{
    const QString tab1 = testTab(1);
    const QString tab2 = testTab(2);

    RUN(Args("eval") << "print('123')", "123");

    QByteArray stderrData;
    QCOMPARE( run(Args("eval") << "x", NULL, &stderrData), 1 );
    QVERIFY( !stderrData.isEmpty() );

    RUN(Args("eval") << QString("tab('%1');add('abc');tab('%2');add('def');").arg(tab1).arg(tab2), "");
    RUN(Args("eval") << QString("tab('%1');if (size() === 1) print('ok')").arg(tab1), "ok");
    RUN(Args("eval") << QString("tab('%1');if (size() === 1) print('ok')").arg(tab2), "ok");
    RUN(Args("eval") << QString("tab('%1');if (str(read(0)) === 'abc') print('ok')").arg(tab1), "ok");
    RUN(Args("eval") << QString("tab('%1');if (str(read(0)) === 'def') print('ok')").arg(tab2), "ok");
}

void Tests::rawData()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab;

    {
        QByteArray input("\x00\x01\x02\x03\x04", 5);
        QString arg1 = QString::fromLatin1("\x01\x02\x03\x04");
        QString arg2 = QString::fromLatin1("\x7f\x6f\x5f\x4f");
        QByteArray stderrData;
        QCOMPARE( run(Args(args) << "write"
                      << MIME_PREFIX "test1" << arg1
                      << MIME_PREFIX "test2" << "-"
                      << MIME_PREFIX "test3" << arg2 ,
                  NULL, &stderrData, input), 0);
        QVERIFY2(testStderr(stderrData), stderrData);
        RUN(Args(args) << "read" << MIME_PREFIX "test1" << "0", arg1.toLatin1());
        RUN(Args(args) << "read" << MIME_PREFIX "test2" << "0", input);
        RUN(Args(args) << "read" << MIME_PREFIX "test3" << "0", arg2.toLatin1());
    }
}

void Tests::nextPrevious()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab;
    RUN(Args(args)  << "add" << "A" << "B" << "C", "");

    // Select "C" in case tab is visible and selection stays on "A".
    RUN(Args(args)  << "previous", "");
    RUN(Args(args)  << "previous", "");

    RUN(Args(args)  << "next", "");
    QVERIFY( waitUntilClipboardSet("B") );
    RUN(Args(args)  << "read", "B");

    RUN(Args(args)  << "next", "");
    QVERIFY( waitUntilClipboardSet("A") );
    RUN(Args(args)  << "read", "A");

    RUN(Args(args)  << "previous", "");
    QVERIFY( waitUntilClipboardSet("B") );
    RUN(Args(args)  << "read", "B");

    RUN(Args(args)  << "previous", "");
    QVERIFY( waitUntilClipboardSet("C") );
    RUN(Args(args)  << "read", "C");
}

void Tests::options()
{
    QByteArray stdoutActual;
    QByteArray stderrActual;
    QCOMPARE( run(Args("config"), &stdoutActual, &stderrActual), 0 );
    QVERIFY2( testStderr(stderrActual), stderrActual );
    QVERIFY( !stdoutActual.isEmpty() );

    // invalid option
    QCOMPARE( run(Args("config") << "xxx", &stdoutActual, &stderrActual), 1 );
    QVERIFY( !testStderr(stderrActual) );
    QVERIFY( stdoutActual.isEmpty() );
    QVERIFY( stderrActual.contains("xxx") );

    QCOMPARE( run(Args("config") << "tab_tree", &stdoutActual, &stderrActual), 0 );
    QVERIFY( testStderr(stderrActual) );
    QVERIFY( stdoutActual == "true\n" || stdoutActual == "false\n" );

    RUN(Args("config") << "tab_tree" << "true", "");
    RUN(Args("config") << "tab_tree", "true\n");

    RUN(Args("config") << "tab_tree" << "false", "");
    RUN(Args("config") << "tab_tree", "false\n");

    RUN(Args("config") << "tab_tree" << "1", "");
    RUN(Args("config") << "tab_tree", "true\n");

    RUN(Args("config") << "tab_tree" << "0", "");
    RUN(Args("config") << "tab_tree", "false\n");
}

void Tests::editCommand()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab;

    RUN(Args("config") << "editor" << "", "");

    // Edit new item.
    const QByteArray data = "edit";
    const QByteArray data1 = generateData(data);
    RUN(Args(args) << "edit", "");
    RUN(Args(args) << "keys" << ":" + QString::fromLatin1(data1) << "F2", "");
    RUN(Args(args) << "read" << "0", data1);

    // Edit existing item.
    const QByteArray data2 = generateData(data);
    RUN(Args(args) << "edit" << "0", "");
    RUN(Args(args) << "keys" << "END" << ":" + QString::fromLatin1(data2) << "F2", "");
    RUN(Args(args) << "read" << "0", data1 + data2);
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
                "\"add(arguments[1]);while(str(read(0)) != '%3');\"")
            .arg(QApplication::applicationFilePath())
            .arg(editorTab)
            .arg(endEditor)
            + " %1";
    RUN(Args("config") << "editor" << cmd, "");

    // Set clipboard.
    const QByteArray data = "edit";
    const QByteArray data1 = generateData(data);
    setClipboard(data1);
    RUN(Args("clipboard"), data1);
    RUN(Args("read") << "0", data1);

    QByteArray out;

    // Edit clipboard.
    RUN(Args() << "edit" << "-1", "");

    // Get name of the filename to edit.
    WAIT_UNTIL(editorFileNameArgs, !out.isEmpty(), out);
    QFile file(out);
    QVERIFY( file.exists() );
    QVERIFY( file.open(QIODevice::ReadWrite) );
    QVERIFY( file.readAll() == data1 );

    // Modify clipboard.
    const QByteArray data2 = generateData(data);
    file.write(data2);
    file.close();

    // Close editor command.
    WAIT_UNTIL(editorEndArgs, !file.exists(), out);
    QCOMPARE(out.data(), "");
    RUN(Args(editorArgs) << "remove" << "0", "");

    // Check if clipboard changed.
    WAIT_UNTIL(Args("read") << "0", out == data1 + data2, out);
    RUN(Args("clipboard"), data1 + data2);

    // Edit existing item.
    const QString text =
            "Some text to edit,\n"
            "with second line!\n"
            + generateData(data);
    RUN(Args(args) << "add" << text, "");
    RUN(Args(args) << "edit" << "0", "");

    // Get name of the filename to edit.
    WAIT_UNTIL(editorFileNameArgs, !out.isEmpty(), out);
    file.setFileName(out);
    QVERIFY( file.exists() );
    QVERIFY( file.open(QIODevice::ReadWrite) );
    QVERIFY( file.readAll() == text.toLocal8Bit() );

    // Modify first item.
    const QByteArray data3 = generateData(data);
    file.write(data3);
    file.close();

    // Close editor command.
    WAIT_UNTIL(editorEndArgs, !file.exists(), out);
    QCOMPARE(out.data(), "");
    RUN(Args(editorArgs) << "remove" << "0", "");

    // Check first item.
    WAIT_UNTIL(Args(args) << "read" << "0", out == text.toLocal8Bit() + data3, out);

    // Edit new item.
    RUN(Args(args) << "edit", "");

    // Get name of the filename to edit.
    WAIT_UNTIL(editorFileNameArgs, !out.isEmpty(), out);
    file.setFileName(out);
    QVERIFY( file.exists() );
    QVERIFY( file.open(QIODevice::ReadWrite) );
    QCOMPARE( file.readAll().data(), "" );

    // Modify first item.
    const QByteArray data4 = generateData(data);
    file.write(data4);
    file.close();

    // Close editor command.
    WAIT_UNTIL(editorEndArgs, !file.exists(), out);
    QCOMPARE(out.data(), "");
    RUN(Args(editorArgs) << "remove" << "0", "");

    // Check first item.
    WAIT_UNTIL(Args(args) << "read" << "0", out == data4, out);
}

bool Tests::startServer()
{
    return m_test->startServer();
}

bool Tests::stopServer()
{
    return m_test->stopServer();
}

bool Tests::isServerRunning()
{
    return m_test->isServerRunning();
}

void Tests::setClipboard(const QByteArray &bytes, const QString &mime)
{
    m_test->setClipboard(bytes, mime);
}

int Tests::run(const QStringList &arguments, QByteArray *stdoutData, QByteArray *stderrData, const QByteArray &in)
{
    return m_test->run(arguments, stdoutData, stderrData, in);
}

bool Tests::hasTab(const QString &tabName)
{
    QByteArray out;
    run(Args("tab"), &out);
    return QString::fromLocal8Bit(out).split(QRegExp("\r\n|\n|\r")).contains(tabName);
}

int runTests(int argc, char *argv[])
{
    QRegExp onlyPlugins;

    if (argc > 1) {
        QString arg = argv[1];
        if (arg.startsWith("PLUGINS:")) {
            arg.remove(QRegExp("^PLUGINS:"));
            onlyPlugins.setPattern(arg);
            onlyPlugins.setCaseSensitivity(Qt::CaseInsensitive);
            --argc;
            ++argv;
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

    ItemFactory itemFactory;
    foreach( const ItemLoaderInterfacePtr &loader, itemFactory.loaders() ) {
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

    return exitCode;
}
