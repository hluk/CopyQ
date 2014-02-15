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
#include "item/serialize.h"

#include <QApplication>
#include <QClipboard>
#include <QElapsedTimer>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMimeData>
#include <QProcess>
#include <QTemporaryFile>
#include <QTest>

#define VERIFY_SERVER_OUTPUT() \
do {\
    QVERIFY( isServerRunning() ); \
    QByteArray output = m_server->readAllStandardError(); \
    QVERIFY2( testStderr(output), output ); \
} while (0)

#define RUN(arguments, stdoutExpected) \
do {\
    QVERIFY( isServerRunning() ); \
    QByteArray stdoutActual; \
    QByteArray stderrActual; \
    QCOMPARE( run(arguments, &stdoutActual, &stderrActual), 0 ); \
    stdoutActual.replace('\r', ""); \
    QCOMPARE( stdoutActual.data(), QByteArray(stdoutExpected).data() ); \
    QVERIFY2( testStderr(stderrActual), stderrActual ); \
    VERIFY_SERVER_OUTPUT(); \
} while (0)

namespace {

/// Interval to wait (in ms) until an action is completed and items from stdout are created.
const int waitMsAction = 200;

/// Interval to wait (in ms) until new clipboard content is propagated to items or monitor.
const int waitMsClipboard = 1000;

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
        QApplication::processEvents();
}

/// Naming scheme for test tabs in application.
QString testTab(int index)
{
    return QString("TEST_&%1").arg(index);
}

bool testStderr(const QByteArray &stderrData)
{
    return !stderrData.contains("warning: ") && !stderrData.contains("ERROR: ");
}

QByteArray getClipboard(const QString &mime = QString("text/plain"))
{
    QApplication::processEvents();
    const QMimeData *data = QApplication::clipboard()->mimeData();
    return (data != NULL) ? data->data(mime) : QByteArray();
}

void initTestProcess(QProcess *p)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("COPYQ_TESTING", "1");
    p->setProcessEnvironment(env);
}

int run(const Args &arguments = Args(), QByteArray *stdoutData = NULL, QByteArray *stderrData = NULL,
        const QByteArray &in = QByteArray())
{
    QProcess p;
    initTestProcess(&p);
    p.start( QApplication::applicationFilePath(), arguments );
    p.waitForStarted();

    p.write(in);
    p.closeWriteChannel();

    if ( !p.waitForFinished(100) ) {
        // Process events in case we own clipboard and the new process requests the contens.
        QApplication::processEvents();
        if ( !p.waitForFinished(200) ) {
            QApplication::processEvents();

            if ( !p.waitForFinished(6000) ) {
                qWarning() << "terminating process";
                p.terminate();

                if ( !p.waitForFinished(1000) ) {
                    qWarning() << "killing process";
                    p.kill();
                }

                return -1;
            }
        }
    }

    if (stdoutData != NULL)
        *stdoutData = p.readAllStandardOutput();
    if (stderrData != NULL)
        *stderrData = p.readAllStandardError();

    return p.exitCode();
}

bool isAnyServerRunning()
{
    return run(Args("size")) == 0;
}

bool hasTab(const QString &tabName)
{
    QByteArray out;
    run(Args("tab"), &out);
    return QString::fromLocal8Bit(out).split(QRegExp("\r\n|\n|\r")).contains(tabName);
}

} // namespace

Tests::Tests(QObject *parent)
    : QObject(parent)
    , m_server(NULL)
    , m_monitor(NULL)
{
}

void Tests::initTestCase()
{
    if ( isAnyServerRunning() ) {
        run(Args("exit"));

        // Wait until client/server communication is closed.
        int tries = 0;
        while( !startServer() && ++tries <= 100 )
            waitFor(100);

        QVERIFY( isServerRunning() );
    } else {
        QVERIFY( startServer() );
    }

    waitFor(1000);

    cleanup();
}

void Tests::cleanupTestCase()
{
    if (m_server != NULL) {
        QVERIFY( stopServer() );
        if ( m_server->state() != QProcess::NotRunning ) {
            m_server->terminate();
            if ( !m_server->waitForFinished(1000) )
                m_server->kill();
        }
    }

    delete m_monitor;
}

void Tests::init()
{
    QVERIFY( isAnyServerRunning() );
    QVERIFY( isServerRunning() );
    VERIFY_SERVER_OUTPUT();

    // Always show main window first so that the results are consistent with desktop environments
    // where user cannot hide main window (tiling window managers without tray).
    RUN(Args("show"), "");
    waitFor(waitMsShow);
}

void Tests::cleanup()
{
    // Remove test tabs
    for (int i = 0; i < 10; ++i) {
        QString tab = testTab(i);
        if ( hasTab(tab.toLatin1()) )
            RUN(Args("removetab") << tab, "");
    }
}

void Tests::moveAndDeleteItems()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab;
    RUN(Args(args) << "add" << "A" << "B" << "C", "");

    RUN(Args(args) << "read" << "0", "C");
    // focus test tab
#ifdef Q_OS_MAC
    // Alt-1 doesn't work on OS X
    RUN(Args(args) << "keys" << "ALT+LEFT", "");
#else
    RUN(Args(args) << "keys" << "ALT+1", "");
#endif // Q_OS_MAC
    // delete first item
    RUN(Args(args) << "keys" << "Home", "");
    RUN(Args(args) << "keys" << shortcutToRemove(), "");
    RUN(Args(args) << "read" << "0", "B");

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
    waitFor(waitMsClipboard);
    RUN(Args(args) << "clipboard", "A");

    // select all and delete
    RUN(Args(args) << "keys" << "CTRL+A" << shortcutToRemove(), "");
    RUN(Args(args) << "size", "0\n");

    RUN(Args(args) << "add" << "ABC" << "DEF" << "GHI" << "JKL", "");

    // search and delete
    RUN(Args(args) << "keys" << ":[AG]", "");
    waitFor(waitMsSearch);
#ifdef Q_OS_MAC
    // "Down" doesn't leave the search box on OS X
    RUN(Args(args) << "keys" << "TAB" << "CTRL+A" << shortcutToRemove(), "");
#else
    RUN(Args(args) << "keys" << "DOWN" << "CTRL+A" << shortcutToRemove(), "");
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
}

void Tests::clipboardToItem()
{
    RUN(Args("show"), "");
    waitFor(waitMsShow);

    setClipboard("TEST0");
    QCOMPARE( getClipboard().data(), "TEST0" );
    RUN(Args("clipboard"), "TEST0");

    setClipboard("TEST1");
    QCOMPARE( getClipboard().data(), "TEST1" );
    RUN(Args("clipboard"), "TEST1");
    RUN(Args("read") << "0", "TEST1");

    const QByteArray htmlBytes = "<b>TEST2</b>";
    setClipboard(htmlBytes, "text/html");
    QCOMPARE( getClipboard("text/html").data(), htmlBytes.data() );
    RUN(Args("clipboard") << "text/html", htmlBytes.data());
    RUN(Args("read") << "text/html" << "0", htmlBytes.data());

    // Unicode test.
    const QString test = QString::fromUtf8(QByteArray("Zkouška s různými českými znaky!"));
    const QByteArray bytes = test.toUtf8();
    setClipboard(bytes);
    QCOMPARE( getClipboard(), bytes );
    RUN(Args("clipboard"), bytes);
    RUN(Args("read") << "0", bytes);
}

void Tests::itemToClipboard()
{
    RUN(Args("add") << "TESTING1" << "TESTING2", "");
    RUN(Args("read") << "0", "TESTING2");
    RUN(Args("read") << "1", "TESTING1");

    RUN(Args("select") << "0", "");

    waitFor(waitMsClipboard);
    RUN(Args("clipboard"), "TESTING2");
    QCOMPARE( getClipboard().data(), "TESTING2" );

    // select second item and move to top
    RUN(Args("config") << "move" << "true", "");
    RUN(Args("select") << "1", "");
    RUN(Args("read") << "0", "TESTING1");
    RUN(Args("read") << "1", "TESTING2");

    waitFor(waitMsClipboard);
    RUN(Args("clipboard"), "TESTING1");
    QCOMPARE( getClipboard().data(), "TESTING1" );

    // select without moving
    RUN(Args("config") << "move" << "0", "");
    RUN(Args("select") << "1", "");
    RUN(Args("read") << "0", "TESTING1");
    RUN(Args("read") << "1", "TESTING2");

    waitFor(waitMsClipboard);
    RUN(Args("clipboard"), "TESTING2");
    QCOMPARE( getClipboard().data(), "TESTING2" );
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

    // action with size
    RUN(Args(argsAction) << action.arg("size") << "", "");
    waitFor(waitMsAction);
    RUN(Args(args) << "size", "1\n");
    RUN(Args(args) << "read" << "0", "0\n");

    // action with size
    RUN(Args(argsAction) << action.arg("size") << "", "");
    waitFor(waitMsAction);
    RUN(Args(args) << "size", "2\n");
    RUN(Args(args) << "read" << "0", "1\n");

    // action with eval print
    RUN(Args(argsAction) << action.arg("eval 'print(\"A,B,C\")'") << "", "");
    waitFor(waitMsAction);
    RUN(Args(args) << "size", "3\n");
    RUN(Args(args) << "read" << "0", "A,B,C");

    // action with read and comma separator for new items
    RUN(Args(argsAction) << action.arg("read 0") << ",", "");
    waitFor(waitMsAction);
    RUN(Args(args) << "size", "6\n");
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
        QByteArray in("\x00\x01\x02\x03\x04", 5);
        QByteArray stderrData;
        QCOMPARE( run(Args(args) << "add" << "-", NULL, &stderrData, in), 0);
        QVERIFY2(testStderr(stderrData), stderrData);
        RUN(Args(args) << "read" << "0", in);
    }

    {
        QByteArray in("\x00\x01\x02\x03\x04", 5);
        QString arg1 = QString::fromLatin1("\x01\x02\x03\x04");
        QString arg2 = QString::fromLatin1("\x7f\x6f\x5f\x4f");
        QByteArray stderrData;
        QCOMPARE( run(Args(args) << "write"
                      << MIME_PREFIX "test1" << arg1
                      << MIME_PREFIX "test2" << "-"
                      << MIME_PREFIX "test3" << arg2 ,
                  NULL, &stderrData, in), 0);
        QVERIFY2(testStderr(stderrData), stderrData);
        RUN(Args(args) << "read" << MIME_PREFIX "test1" << "0", arg1.toLatin1());
        RUN(Args(args) << "read" << MIME_PREFIX "test2" << "0", in);
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
    waitFor(waitMsClipboard);
    RUN(Args(args)  << "read", "B");

    RUN(Args(args)  << "next", "");
    waitFor(waitMsClipboard);
    RUN(Args(args)  << "read", "A");

    RUN(Args(args)  << "previous", "");
    waitFor(waitMsClipboard);
    RUN(Args(args)  << "read", "B");

    RUN(Args(args)  << "previous", "");
    waitFor(waitMsClipboard);
    RUN(Args(args)  << "read", "C");
}

bool Tests::startServer()
{
    if (m_server != NULL)
        m_server->deleteLater();
    m_server = new QProcess(this);

    initTestProcess(m_server);

    m_server->start( QApplication::applicationFilePath(), QStringList(), QIODevice::ReadOnly );
    m_server->waitForStarted();

    if (m_server->state() != QProcess::Running) {
        log( QString("Failed to launch \"%1\": %2")
             .arg(QApplication::applicationFilePath())
             .arg(m_server->errorString()),
             LogError );
        return false;
    }

    // Wait for client/server communication is established.
    int tries = 0;
    while( !isServerRunning() && ++tries <= 50 )
        waitFor(100);

    return isServerRunning();
}

bool Tests::stopServer()
{
    if (m_server == NULL)
        return !isServerRunning();

    run(Args("exit"));
    m_server->waitForFinished(5000);

    return !isAnyServerRunning();
}

bool Tests::isServerRunning()
{
    return m_server != NULL && m_server->state() == QProcess::Running && isAnyServerRunning();
}

void Tests::setClipboard(const QByteArray &bytes, const QString &mime)
{
    if (m_monitor == NULL) {
        m_monitor = new RemoteProcess();
        const QString name = "copyq_TEST";
        m_monitor->start( name, QStringList("monitor") << name );
        waitFor(1000);
    }

    QVERIFY( m_monitor->isConnected() );

    const QVariantMap data = createDataMap(mime, bytes);
    QVERIFY( m_monitor->writeMessage(serializeData(data)) );

    waitFor(waitMsClipboard);

    QVERIFY( m_monitor->isConnected() );
}
