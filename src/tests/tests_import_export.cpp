// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_utils.h"
#include "tests.h"

#include "common/temporaryfile.h"

#include <QRegularExpression>

namespace {

const QString exportFilePath(const QString &suffix = QStringLiteral("1"))
{
    return QStringLiteral("%1/copyq-test-%2.cpq")
        .arg(QString::fromUtf8(qgetenv("COPYQ_SETTINGS_PATH")), suffix);
}

} // namespace

void Tests::exportImport(int flags)
{
#ifdef Q_OS_MAC
    if (flags & (ExportSettings | ExportCommands))
        SKIP("Mnemonic for focusing shortcut button doesn't work on OS X");
#endif
    const auto tab = testTab(1);
    const auto args = Args("tab") << tab;

    if (flags & ExportTab) {
        RUN("show" << tab, "");

        RUN(args << "add" << "3" << "2" << "1", "");
        QVERIFY( hasTab(tab) );
        RUN(args << "size", "3\n");
        RUN(args << "read" << "0" << "1" << "2" << "3", "1\n2\n3\n");
    }

    RUN("setCommands([{name: 'test', cmd: 'copyq help'}])", "");
    RUN("commands()[0].name", "test\n");
    RUN("commands()[0].cmd", "copyq help\n");
    RUN("commands()[0].enable", "true\n");
    RUN("commands().length", "1\n");

    RUN("config('maxitems', 20)", "20\n");

    // Export
    KEYS("Ctrl+S" << exportDialogId);
    if (flags & ExportSettings)
        KEYS(exportDialogId << "ALT+N");  // Enable exporting configuration
    if (flags & ExportCommands)
        KEYS(exportDialogId << "ALT+M");  // Enable exporting commands
    KEYS("ENTER");

    // Export with password only if supported
    KEYS(passwordEntryExportId);
    if (flags & ExportWithPassword) {
        KEYS(":TEST1234" << "ENTER");
        KEYS(passwordEntryRetypeId << ":TEST1234");
    }
    KEYS("ENTER");

    KEYS(fileNameEditId << ":" + exportFilePath());
    KEYS("ENTER");
    KEYS(clipboardBrowserId);
    Q_ASSERT(QFile::exists(exportFilePath()));

    RUN("config('maxitems', 10)", "10\n");

    // Import
    KEYS("Ctrl+Shift+I" << fileNameEditId << ":" + exportFilePath());
    KEYS("ENTER");

    // Import with password only if supported
    if (flags & ExportWithPassword) {
        KEYS(passwordEntryImportId << ":TEST1234");
        KEYS("ENTER");
    }

    KEYS(importDialogId << "ENTER");
    KEYS(clipboardBrowserId);

    if (flags & ExportTab) {
        const auto tabCopy = QStringLiteral("%1 (1)").arg(tab);
        RUN("tab", QLatin1String(clipboardTabName) + "\n" + tab + "\n" + tabCopy + "\n");
        const auto args1 = Args("tab") << tabCopy;
        RUN(args1 << "size", "3\n");
        RUN(args1 << "read" << "0" << "1" << "2" << "3", "1\n2\n3\n");
    }

    if (flags & ExportSettings)
        RUN("config('maxitems')", "20\n");
    else
        RUN("config('maxitems')", "10\n");

    RUN("commands()[0].name", "test\n");
    RUN("commands()[0].cmd", "copyq help\n");
    RUN("commands()[0].enable", "true\n");
    if (flags & ExportCommands) {
        RUN("commands()[1].name", "test\n");
        RUN("commands()[1].cmd", "copyq help\n");
        RUN("commands()[1].enable", "true\n");
        RUN("commands().length", "2\n");
    } else {
        RUN("commands().length", "1\n");
    }
}

void Tests::exportImportNoPasswordTab() { exportImport(ExportTab); }
void Tests::exportImportNoPasswordSettingsOnly() { exportImport(ExportSettings); }
void Tests::exportImportNoPasswordCommandsOnly() { exportImport(ExportCommands); }
void Tests::exportImportPasswordTab() { exportImport(ExportWithPassword | ExportTab); }
void Tests::exportImportPasswordSettingsOnly() { exportImport(ExportWithPassword | ExportSettings); }
void Tests::exportImportPasswordCommandsOnly() { exportImport(ExportWithPassword | ExportCommands); }

void Tests::exportImportErrors()
{
#ifdef Q_OS_WIN
    const auto fileNotFound = QStringLiteral("The system cannot find the path specified.");
#else
    const auto fileNotFound = QStringLiteral("No such file or directory");
#endif

    // Import non-existent file
    const auto invalidFile = QStringLiteral("/tmp/copyq-invalid/file.cpq");
    const auto openError = QStringLiteral(
        R"(Failed to open file for import: "%1" message: "%2")"
    ).arg(QRegularExpression::escape(invalidFile), fileNotFound);
    m_test->ignoreErrors(QRegularExpression(openError));
    const auto clientErrorTemplate = QStringLiteral("ScriptError: Failed to import file \"%1\"");
    const auto clientErrorInvalid = clientErrorTemplate.arg(invalidFile);
    RUN_EXPECT_ERROR_WITH_STDERR("importData" << invalidFile, 4, clientErrorInvalid);
    RUN_EXPECT_ERROR_WITH_STDERR("importTab" << invalidFile, 4, clientErrorInvalid);
    QVERIFY2( dropLogsToFileCountAndSize(0, 0), "Failed to remove log files" );

    // Export to non-existent directory
    const auto exportError = QStringLiteral(
        R"(Failed to open file for export( \(v2\))?: "%1" message: "%2")"
    ).arg(QRegularExpression::escape(invalidFile), fileNotFound);
    m_test->ignoreErrors(QRegularExpression(exportError));
    const auto clientErrorExport =
        QStringLiteral("ScriptError: Failed to export file \"%1\"").arg(invalidFile);
    RUN_EXPECT_ERROR_WITH_STDERR("exportData" << invalidFile, 4, clientErrorExport);
    RUN_EXPECT_ERROR_WITH_STDERR("exportTab" << invalidFile, 4, clientErrorExport);
    QVERIFY2( dropLogsToFileCountAndSize(0, 0), "Failed to remove log files" );

    // Import invalid file
    QFile tmp(exportFilePath(QStringLiteral("invalid")));
    QVERIFY(tmp.open(QFile::WriteOnly));
    tmp.write("TEST");
    tmp.close();
    const auto path = tmp.fileName();

    const auto headerError = QStringLiteral(
        R"(Failed to read file header for import - file: "%1" reason: 1)"
    ).arg(QRegularExpression::escape(path));
    m_test->ignoreErrors(QRegularExpression(headerError));
    const auto clientError = clientErrorTemplate.arg(path);
    RUN_EXPECT_ERROR_WITH_STDERR("importData" << path, 4, clientError);
    RUN_EXPECT_ERROR_WITH_STDERR("importTab" << path, 4, clientError);
    QVERIFY2( dropLogsToFileCountAndSize(0, 0), "Failed to remove log files" );

#ifndef Q_OS_WIN
    // Import file without permissions
    tmp.setPermissions(QFile::Permissions());
    const auto permissionError = QStringLiteral(
        R"(Failed to open file for import: "%1" message: "Permission denied")"
    ).arg(QRegularExpression::escape(path));
    m_test->ignoreErrors(QRegularExpression(permissionError));
    RUN_EXPECT_ERROR_WITH_STDERR("importData" << path, 4, clientError);
    RUN_EXPECT_ERROR_WITH_STDERR("importTab" << path, 4, clientError);
    QVERIFY2( dropLogsToFileCountAndSize(0, 0), "Failed to remove log files" );
#endif

    // Export incomplete file
    const auto pathPartial = exportFilePath(QStringLiteral("Partial"));
    m_test->ignoreErrors({});
    RUN("exportData" << pathPartial, "");

    QFile f(pathPartial);
    QVERIFY2( f.resize(f.size() - 1), f.errorString().toUtf8() );
    const auto importErrorV = QStringLiteral(R"(Import \(v%1\) failed - file: "%2" reason: 1)");
    const auto importErrorV4 = importErrorV.arg(4).arg(QRegularExpression::escape(pathPartial));
    m_test->ignoreErrors(QRegularExpression(importErrorV4));
    const auto clientErrorPartial = clientErrorTemplate.arg(pathPartial);
    RUN_EXPECT_ERROR_WITH_STDERR("importData" << pathPartial, 4, clientErrorPartial);
    QVERIFY2( dropLogsToFileCountAndSize(0, 0), "Failed to remove log files" );

    QVERIFY(f.open(QFile::WriteOnly));
    QDataStream in(&f);
    in.setVersion(QDataStream::Qt_4_7);
    in << QByteArray("CopyQ v3");
    f.close();
    const auto importErrorV3 = importErrorV.arg(3).arg(QRegularExpression::escape(pathPartial));
    m_test->ignoreErrors(QRegularExpression(importErrorV3));
    RUN_EXPECT_ERROR_WITH_STDERR("importData" << pathPartial, 4, clientErrorPartial);
    QVERIFY2( dropLogsToFileCountAndSize(0, 0), "Failed to remove log files" );

    QVERIFY(f.open(QFile::WriteOnly));
    in << QByteArray("CopyQ v4");
    f.close();
    m_test->ignoreErrors(QRegularExpression(importErrorV4));
    RUN_EXPECT_ERROR_WITH_STDERR("importData" << pathPartial, 4, clientErrorPartial);
    QVERIFY2( dropLogsToFileCountAndSize(0, 0), "Failed to remove log files" );

    QVERIFY(f.open(QFile::WriteOnly));
    in << QByteArray("CopyQ v2");
    f.close();
    const auto importErrorV2 = importErrorV.arg(2).arg(QRegularExpression::escape(pathPartial));
    m_test->ignoreErrors(QRegularExpression(importErrorV2));
    RUN_EXPECT_ERROR_WITH_STDERR("importData" << pathPartial, 4, clientErrorPartial);
}
