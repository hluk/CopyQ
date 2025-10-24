// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_utils.h"
#include "tests.h"
#include "tests_common.h"

#include "common/mimetypes.h"

void Tests::createTabDialog()
{
    const auto tab1 = testTab(1);
    KEYS(
        clipboardBrowserId << "CTRL+T"
        << tabDialogLineEditId << ":" + tab1 << "ENTER");
    TEST_SELECTED(tab1 + "\n");
}

void Tests::showHideAboutDialog()
{
    const auto aboutShortcut = keyNameFor(QKeySequence::QKeySequence::WhatsThis);
    KEYS(clipboardBrowserId << aboutShortcut << aboutDialogId);
    KEYS(aboutDialogId << "ESCAPE" << clipboardBrowserId);
}

void Tests::showHideClipboardDialog()
{
    TEST( m_test->setClipboard("TEST", "test-format") );
    KEYS(clipboardBrowserId << "CTRL+SHIFT+C" << clipboardDialogId);

    KEYS(clipboardDialogId << "DOWN" << "HOME" << clipboardDialogFormatListId);

    KEYS(clipboardDialogFormatListId << keyNameFor(QKeySequence::Copy));
#ifdef Q_OS_WIN
    WAIT_FOR_CLIPBOARD("application/x-qt-windows-mime;value=\"test-format\"");
#else
    WAIT_FOR_CLIPBOARD("test-format");
#endif

    KEYS(clipboardDialogId << "ESCAPE" << clipboardBrowserId);
}

void Tests::showHideItemDialog()
{
    RUN("write" << "test-format" << "TEST", "");
    RUN("selectItems" << "0", "true\n");

    KEYS(clipboardBrowserId << "F4" << clipboardDialogId);

    KEYS(clipboardDialogId << "DOWN" << clipboardDialogFormatListId);
    KEYS(clipboardDialogFormatListId << keyNameFor(QKeySequence::Copy));
    WAIT_FOR_CLIPBOARD("test-format");

    KEYS(clipboardDialogFormatListId << "DOWN");
    KEYS(clipboardDialogFormatListId << keyNameFor(QKeySequence::Copy));
    WAIT_FOR_CLIPBOARD("test-format");

    KEYS(clipboardDialogId << "ESCAPE" << clipboardBrowserId);
}

void Tests::showHideLogDialog()
{
    KEYS(clipboardBrowserId << "F12" << logDialogId);

    KEYS(logDialogId << "CTRL+A" << "CTRL+C" << logDialogId);
    const QByteArray expectedLog = "Starting callback: onStart";
    TEST( m_test->verifyClipboard(expectedLog, mimeHtml, false) );

    KEYS(logDialogId << "ESCAPE" << clipboardBrowserId);
}

void Tests::showHideActionHandlerDialog()
{
    KEYS(clipboardBrowserId << "CTRL+SHIFT+Z" << actionHandlerDialogId);

    KEYS(actionHandlerFilterId << ":onstart" << "TAB" << actionHandlerTableId);

    KEYS(actionHandlerTableId << "RIGHT" << "CTRL+C");
    WAIT_FOR_CLIPBOARD("copyq onStart");

    KEYS(actionHandlerDialogId << "ESCAPE" << clipboardBrowserId);
}

void Tests::shortcutDialogAddShortcut()
{
#ifdef Q_OS_MAC
    SKIP("Mnemonic for focusing shortcut button doesn't work on OS X");
#endif

    RUN("setCommands([{name: 'test', inMenu: true, cmd: 'copyq add OK'}])", "");
    RUN("commands()[0].shortcuts", "");

    KEYS(clipboardBrowserId << "F6" << commandDialogId);
    KEYS(commandDialogId << "ALT+S" << shortcutButtonId);
    KEYS(shortcutButtonId << "Space" << shortcutDialogId);
    KEYS(shortcutDialogId << "CTRL+F1" << shortcutButtonId);

    KEYS(commandDialogId << "ESCAPE" << commandDialogSaveButtonId);
    KEYS(commandDialogSaveButtonId << "Enter" << clipboardBrowserId);
    RUN("commands()[0].shortcuts", "ctrl+f1\n");
}

void Tests::shortcutDialogAddTwoShortcuts()
{
#ifdef Q_OS_MAC
    SKIP("Mnemonic for focusing shortcut button doesn't work on OS X");
#endif

    RUN("setCommands([{name: 'test', inMenu: true, shortcuts: ['ctrl+f1'], cmd: 'copyq add OK'}])", "");
    RUN("commands()[0].shortcuts", "ctrl+f1\n");

    KEYS(clipboardBrowserId << "F6" << commandDialogId);
    KEYS(commandDialogId << "ALT+S" << shortcutButtonId);

    KEYS(shortcutButtonId << "TAB" << shortcutButtonId);
    KEYS(shortcutButtonId << "Space" << shortcutDialogId);
    KEYS(shortcutDialogId << "F1" << shortcutButtonId);

    KEYS(shortcutButtonId << "Space" << shortcutDialogId);
    KEYS(shortcutDialogId << "F2" << shortcutButtonId);

    KEYS(commandDialogId << "ESCAPE" << commandDialogSaveButtonId);
    KEYS(commandDialogSaveButtonId << "Enter" << clipboardBrowserId);
    RUN("commands()[0].shortcuts", "ctrl+f1\nf1\nf2\n");
}

void Tests::shortcutDialogChangeShortcut()
{
#ifdef Q_OS_MAC
    SKIP("Mnemonic for focusing shortcut button doesn't work on OS X");
#endif

    RUN("setCommands([{name: 'test', inMenu: true, shortcuts: ['f1','f2','f3'], cmd: 'copyq add OK'}])", "");
    RUN("commands()[0].shortcuts", "f1\nf2\nf3\n");

    KEYS(clipboardBrowserId << "F6" << commandDialogId);
    KEYS(commandDialogId << "ALT+S" << shortcutButtonId);
    KEYS(commandDialogId << "TAB" << shortcutButtonId);
    KEYS(shortcutButtonId << "Space" << shortcutDialogId);
    KEYS(shortcutDialogId << "F4" << shortcutButtonId);

    KEYS(commandDialogId << "ESCAPE" << commandDialogSaveButtonId);
    KEYS(commandDialogSaveButtonId << "Enter" << clipboardBrowserId);
    RUN("commands()[0].shortcuts", "f1\nf4\nf3\n");
}

void Tests::shortcutDialogSameShortcut()
{
#ifdef Q_OS_MAC
    SKIP("Mnemonic for focusing shortcut button doesn't work on OS X");
#endif

    RUN("setCommands([{name: 'test', inMenu: true, shortcuts: ['ctrl+f1'], cmd: 'copyq add OK'}])", "");
    RUN("commands()[0].shortcuts", "ctrl+f1\n");

    KEYS(clipboardBrowserId << "F6" << commandDialogId);
    KEYS(commandDialogId << "ALT+S" << shortcutButtonId);
    KEYS(shortcutButtonId << "TAB" << shortcutButtonId);
    KEYS(shortcutButtonId << "Space" << shortcutDialogId);
    KEYS(shortcutDialogId << "CTRL+F1" << shortcutButtonId);

    KEYS(commandDialogId << "ESCAPE" << clipboardBrowserId);
    RUN("commands()[0].shortcuts", "ctrl+f1\n");
}

void Tests::shortcutDialogCancel()
{
#ifdef Q_OS_MAC
    SKIP("Mnemonic for focusing shortcut button doesn't work on OS X");
#endif

    RUN("setCommands([{name: 'test', inMenu: true, shortcuts: ['ctrl+f1'], cmd: 'copyq add OK'}])", "");
    RUN("commands()[0].shortcuts", "ctrl+f1\n");

    KEYS(clipboardBrowserId << "F6" << commandDialogId);
    KEYS(commandDialogId << "ALT+S" << shortcutButtonId);
    KEYS(commandDialogId << "TAB" << shortcutButtonId);
    KEYS(shortcutButtonId << "Space" << shortcutDialogId);
    KEYS(shortcutDialogId << "TAB" << "focus:ShortcutDialog");
    KEYS("ESCAPE" << shortcutButtonId);

    KEYS(commandDialogId << "ESCAPE" << clipboardBrowserId);
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

    KEYS(clipboardBrowserId << "CTRL+F1" << actionDialogId);
    KEYS(actionDialogId << "ESCAPE" << clipboardBrowserId);
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

    KEYS(clipboardBrowserId << "CTRL+F1" << actionDialogId);
    // Can't focus configuration checkboxes on OS X
#ifdef Q_OS_MAC
    KEYS(actionDialogId << "BACKTAB" << "ENTER" << clipboardBrowserId);
#else
    KEYS(actionDialogId << "ENTER" << clipboardBrowserId);
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

    KEYS(clipboardBrowserId << "CTRL+F1" << actionDialogId);
    // Can't focus configuration checkboxes on OS X
#ifdef Q_OS_MAC
    KEYS(actionDialogId << "BACKTAB" << "ENTER" << clipboardBrowserId);
#else
    KEYS(actionDialogId << "ENTER" << clipboardBrowserId);
#endif
    WAIT_ON_OUTPUT("settings" << "test", "A\nC");
    WAIT_ON_OUTPUT(args << "read" << "0", "A\nC");
}

void Tests::exitConfirm()
{
    KEYS(clipboardBrowserId << "CTRL+Q" << confirmExitDialogId);
    KEYS(confirmExitDialogId << "ENTER");
    TEST( m_test->waitForServerToStop() );
}

void Tests::exitNoConfirm()
{
    RUN("config" << "confirm_exit" << "false", "false\n");
    KEYS(clipboardBrowserId << "CTRL+Q");
    TEST( m_test->waitForServerToStop() );
}

void Tests::exitStopCommands()
{
    RUN("config" << "confirm_exit" << "false", "false\n");
    RUN("action" << "copyq sleep 999999", "");
    KEYS(clipboardBrowserId << "CTRL+Q");
    KEYS(runningCommandsExitDialogId << "ENTER");
    TEST( m_test->waitForServerToStop() );
}
