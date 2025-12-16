// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_utils.h"
#include "tests.h"

namespace {

const QString exportFilePath()
{
    return QStringLiteral("%1/copyq-test.cpq")
        .arg(QString::fromUtf8(qgetenv("COPYQ_SETTINGS_PATH")));
}

} // namespace

void Tests::exportImport(int flags)
{
#ifdef Q_OS_MAC
    if (flags & (ExportSettings | ExportCommands))
        SKIP("Mnemonic for focusing shortcut button doesn't work on OS X");
#endif
#ifndef WITH_QCA_ENCRYPTION
    if (flags & ExportWithPassword)
        SKIP("Encryption support not built-in");
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
#ifdef WITH_QCA_ENCRYPTION
    KEYS(passwordEntryExportId);
    if (flags & ExportWithPassword) {
        KEYS(":TEST1234" << "ENTER");
        KEYS(passwordEntryRetypeId << ":TEST1234");
    }
    KEYS("ENTER");
#endif

    KEYS(fileNameEditId << ":" + exportFilePath());
    KEYS("ENTER");
    KEYS(clipboardBrowserId);
    Q_ASSERT(QFile::exists(exportFilePath()));

    RUN("config('maxitems', 10)", "10\n");

    // Import
    KEYS("Ctrl+Shift+I" << fileNameEditId << ":" + exportFilePath());
    KEYS("ENTER");

    // Import with password only if supported
#ifdef WITH_QCA_ENCRYPTION
    if (flags & ExportWithPassword) {
        KEYS(passwordEntryImportId << ":TEST1234");
        KEYS("ENTER");
    }
#endif

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
