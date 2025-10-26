// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_utils.h"
#include "tests.h"
#include "tests_common.h"

#include "common/commandstatus.h"
#include "common/config.h"
#include "common/log.h"
#include "common/version.h"
#include "common/settings.h"

#include <QRegularExpression>

void Tests::configPath()
{
    RUN("print(info('config'))", Settings().fileName());
}

void Tests::readLog()
{
    QByteArray stdoutActual;
    QByteArray stderrActual;
    QCOMPARE( run(Args("info") << "log", &stdoutActual, &stderrActual), 0 );
    QVERIFY2( testStderr(stderrActual), stderrActual );

    QVERIFY2( stdoutActual.endsWith("/tests-*.log*\n"), stdoutActual );

    const QString logFile = logFileName();
    QCOMPARE(
        logFile.section('-', 0, -3),
        QString::fromUtf8(stdoutActual).section('-', 0, -2) );

    const QByteArray log = readLogFile(maxReadLogSize);
    QVERIFY2(!log.isEmpty(), logFile.toUtf8());

    const QStringList lines = splitLines(readLogFile(maxReadLogSize));

    const auto configPattern = QStringLiteral(
        R"(^\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}\] DEBUG <Server-\d+>: Loading configuration$)");
    QVERIFY2(count(lines, configPattern), log);

    const auto monitorPattern = QStringLiteral(
        R"(^\[.*\] DEBUG <Server-\d+>: Starting monitor$)");
    QVERIFY2(count(lines, monitorPattern), log);
}

void Tests::rotateLog()
{
    QByteArray stdoutActual;
    QByteArray stderrActual;
    QCOMPARE( run(Args("info") << "log", &stdoutActual, &stderrActual), 0 );
    QVERIFY2( testStderr(stderrActual), stderrActual );
    const QString logDirPath = QString::fromUtf8(stdoutActual).section("/", 0, -2);
    QVERIFY( !logDirPath.isEmpty() );

    const QByteArray logData(logFileSize, '-');
    const QDir logDir(logDirPath);
    QVERIFY2( logDir.exists(), logDirPath.toUtf8() );
    const auto listLogFiles = [&](const int rotateNumber = -1) {
        const QString pattern = rotateNumber == -1
            ? QStringLiteral("tests*.log.*")
            : QStringLiteral("tests*.log.%1").arg(rotateNumber);
        return logDir.entryList({pattern}, QDir::Files, QDir::Name);
    };
    const auto logFilesMessage = [&](int i) {
        return QStringLiteral("%2 (rotateCount = %1):\n  %3").arg(i).arg(
            logDirPath.toUtf8(),
            listLogFiles().join("\n  ")).toUtf8();
    };

    for (int i = 1; i < logFileCount; ++i) {
        QCOMPARE( run(Args("serverLog") << "-", &stdoutActual, &stderrActual, logData), 0 );
        QVERIFY2( testStderr(stderrActual), stderrActual );
        QVERIFY2( listLogFiles().count() >= i, logFilesMessage(i) );
        QVERIFY2( !listLogFiles(i).isEmpty(), logFilesMessage(i) );
    }
    QCOMPARE( run(Args("serverLog") << "-", &stdoutActual, &stderrActual, logData), 0 );
    QVERIFY2( testStderr(stderrActual), stderrActual );
    const auto logFiles = listLogFiles(logFileCount);
    QVERIFY2( logFiles.isEmpty(), logFilesMessage(logFileCount) );
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
