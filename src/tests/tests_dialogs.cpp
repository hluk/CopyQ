// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_utils.h"
#include "tests.h"
#include "tests_common.h"

#include "common/mimetypes.h"

void Tests::createTabDialog()
{
    const auto tab1 = testTab(1);
    RUN("keys"
        << clipboardBrowserId << "CTRL+T"
        << tabDialogLineEditId << ":" + tab1 << "ENTER", "");
    RUN("testSelected", tab1 + "\n");
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
    RUN("write" << "test-format" << "TEST", "");
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
    RUN("commands()[0].shortcuts", "");

    RUN("keys" << clipboardBrowserId << "F6" << commandDialogId, "");
    RUN("keys" << commandDialogId << "ALT+S" << shortcutButtonId, "");
    RUN("keys" << shortcutButtonId << "Space" << shortcutDialogId, "");
    RUN("keys" << shortcutDialogId << "CTRL+F1" << shortcutButtonId, "");

    RUN("keys" << commandDialogId << "ESCAPE" << commandDialogSaveButtonId, "");
    RUN("keys" << commandDialogSaveButtonId << "Enter" << clipboardBrowserId, "");
    RUN("commands()[0].shortcuts", "ctrl+f1\n");
}

void Tests::shortcutDialogAddTwoShortcuts()
{
#ifdef Q_OS_MAC
    SKIP("Mnemonic for focusing shortcut button doesn't work on OS X");
#endif

    RUN("setCommands([{name: 'test', inMenu: true, shortcuts: ['ctrl+f1'], cmd: 'copyq add OK'}])", "");
    RUN("commands()[0].shortcuts", "ctrl+f1\n");

    RUN("keys" << clipboardBrowserId << "F6" << commandDialogId, "");
    RUN("keys" << commandDialogId << "ALT+S" << shortcutButtonId, "");

    RUN("keys" << shortcutButtonId << "TAB" << shortcutButtonId, "");
    RUN("keys" << shortcutButtonId << "Space" << shortcutDialogId, "");
    RUN("keys" << shortcutDialogId << "F1" << shortcutButtonId, "");

    RUN("keys" << shortcutButtonId << "Space" << shortcutDialogId, "");
    RUN("keys" << shortcutDialogId << "F2" << shortcutButtonId, "");

    RUN("keys" << commandDialogId << "ESCAPE" << commandDialogSaveButtonId, "");
    RUN("keys" << commandDialogSaveButtonId << "Enter" << clipboardBrowserId, "");
    RUN("commands()[0].shortcuts", "ctrl+f1\nf1\nf2\n");
}

void Tests::shortcutDialogChangeShortcut()
{
#ifdef Q_OS_MAC
    SKIP("Mnemonic for focusing shortcut button doesn't work on OS X");
#endif

    RUN("setCommands([{name: 'test', inMenu: true, shortcuts: ['f1','f2','f3'], cmd: 'copyq add OK'}])", "");
    RUN("commands()[0].shortcuts", "f1\nf2\nf3\n");

    RUN("keys" << clipboardBrowserId << "F6" << commandDialogId, "");
    RUN("keys" << commandDialogId << "ALT+S" << shortcutButtonId, "");
    RUN("keys" << commandDialogId << "TAB" << shortcutButtonId, "");
    RUN("keys" << shortcutButtonId << "Space" << shortcutDialogId, "");
    RUN("keys" << shortcutDialogId << "F4" << shortcutButtonId, "");

    RUN("keys" << commandDialogId << "ESCAPE" << commandDialogSaveButtonId, "");
    RUN("keys" << commandDialogSaveButtonId << "Enter" << clipboardBrowserId, "");
    RUN("commands()[0].shortcuts", "f1\nf4\nf3\n");
}

void Tests::shortcutDialogSameShortcut()
{
#ifdef Q_OS_MAC
    SKIP("Mnemonic for focusing shortcut button doesn't work on OS X");
#endif

    RUN("setCommands([{name: 'test', inMenu: true, shortcuts: ['ctrl+f1'], cmd: 'copyq add OK'}])", "");
    RUN("commands()[0].shortcuts", "ctrl+f1\n");

    RUN("keys" << clipboardBrowserId << "F6" << commandDialogId, "");
    RUN("keys" << commandDialogId << "ALT+S" << shortcutButtonId, "");
    RUN("keys" << shortcutButtonId << "TAB" << shortcutButtonId, "");
    RUN("keys" << shortcutButtonId << "Space" << shortcutDialogId, "");
    RUN("keys" << shortcutDialogId << "CTRL+F1" << shortcutButtonId, "");

    RUN("keys" << commandDialogId << "ESCAPE" << clipboardBrowserId, "");
    RUN("commands()[0].shortcuts", "ctrl+f1\n");
}

void Tests::shortcutDialogCancel()
{
#ifdef Q_OS_MAC
    SKIP("Mnemonic for focusing shortcut button doesn't work on OS X");
#endif

    RUN("setCommands([{name: 'test', inMenu: true, shortcuts: ['ctrl+f1'], cmd: 'copyq add OK'}])", "");
    RUN("commands()[0].shortcuts", "ctrl+f1\n");

    RUN("keys" << clipboardBrowserId << "F6" << commandDialogId, "");
    RUN("keys" << commandDialogId << "ALT+S" << shortcutButtonId, "");
    RUN("keys" << commandDialogId << "TAB" << shortcutButtonId, "");
    RUN("keys" << shortcutButtonId << "Space" << shortcutDialogId, "");
    RUN("keys" << shortcutDialogId << "TAB" << "focus:ShortcutDialog", "");
    RUN("keys" << "ESCAPE" << shortcutButtonId, "");

    RUN("keys" << commandDialogId << "ESCAPE" << clipboardBrowserId, "");
    RUN("commands()[0].shortcuts", "ctrl+f1\n");
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
    // Can't focus configuration checkboxes on OS X
#ifdef Q_OS_MAC
    RUN("keys" << actionDialogId << "BACKTAB" << "ENTER" << clipboardBrowserId, "");
#else
    RUN("keys" << actionDialogId << "ENTER" << clipboardBrowserId, "");
#endif
    WAIT_ON_OUTPUT("settings" << "test", "SHOULD_BE_SET");
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
            cmd: `
                copyq settings test %1
                copyq input
            `
        }])
        )";
    RUN(script, "");

    const auto tab = testTab(1);
    const auto args = Args("tab") << tab;
    RUN(args << "add" << "C" << "B" << "A", "");
    RUN("setCurrentTab" << tab, "");
    RUN(args << "selectItems" << "0" << "2", "true\n");

    RUN("keys" << clipboardBrowserId << "CTRL+F1" << actionDialogId, "");
    // Can't focus configuration checkboxes on OS X
#ifdef Q_OS_MAC
    RUN("keys" << actionDialogId << "BACKTAB" << "ENTER" << clipboardBrowserId, "");
#else
    RUN("keys" << actionDialogId << "ENTER" << clipboardBrowserId, "");
#endif
    WAIT_ON_OUTPUT("settings" << "test", "A\nC");
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
    RUN("config" << "confirm_exit" << "false", "false\n");
    RUN("keys" << clipboardBrowserId << "CTRL+Q", "");
    TEST( m_test->waitForServerToStop() );
}

void Tests::exitStopCommands()
{
    RUN("config" << "confirm_exit" << "false", "false\n");
    RUN("action" << "copyq sleep 999999", "");
    RUN("keys" << clipboardBrowserId << "CTRL+Q", "");
    RUN("keys" << runningCommandsExitDialogId << "ENTER", "");
    TEST( m_test->waitForServerToStop() );
}
