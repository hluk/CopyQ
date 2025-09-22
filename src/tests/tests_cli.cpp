// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_utils.h"
#include "tests.h"
#include "tests_common.h"

#include "common/commandstatus.h"
#include "common/log.h"
#include "common/version.h"

#include <QRegularExpression>

void Tests::readLog()
{
    QByteArray stdoutActual;
    QByteArray stderrActual;
    QCOMPARE( run(Args("info") << "log", &stdoutActual, &stderrActual), 0 );
    QVERIFY2( testStderr(stderrActual), stderrActual );

    const QString logFile =
        QFileInfo(logFileName()).absoluteDir().filePath("copyq.log");
    QCOMPARE( logFile + "\n", QString::fromUtf8(stdoutActual) );

    const QByteArray log = readLogFile(maxReadLogSize);
    QVERIFY2(!log.isEmpty(), log);

    const QStringList lines = splitLines(readLogFile(maxReadLogSize));

    const auto configPattern = QStringLiteral(
        R"(^\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}\] DEBUG <Server-\d+>: Loading configuration$)");
    QVERIFY2(count(lines, configPattern), log);

    const auto monitorPattern = QStringLiteral(
        R"(^\[.*\] DEBUG <Server-\d+>: Starting monitor$)");
    QVERIFY2(count(lines, monitorPattern), log);
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
    QVERIFY( version.contains(QRegularExpression("\\bCopyQ\\b.*" + QRegularExpression::escape(versionString))) );
    // Version contains Qt version.
    QVERIFY( version.contains(QRegularExpression("\\bQt:\\s+\\d")) );
}

void Tests::badCommand()
{
    RUN_EXPECT_ERROR_WITH_STDERR("xxx", CommandException, "xxx");
    RUN_EXPECT_ERROR_WITH_STDERR("tab" << testTab(1) << "yyy", CommandException, "yyy");

    // Bad command shouldn't create new tab.
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

void Tests::commandCatchExceptions()
{
    RUN("try { removeTab('MISSING') } catch(e) { print(e) }",
        "Error: Tab with given name doesn't exist!");
}
