// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_utils.h"
#include "tests.h"
#include "tests_common.h"

#include "common/sleeptimer.h"

void Tests::shortcutCommand()
{
    RUN("setCommands([{name: 'test', inMenu: true, shortcuts: ['Ctrl+F1'], cmd: 'copyq add OK'}])", "");
    KEYS("CTRL+F1");
    WAIT_ON_OUTPUT("read" << "0", "OK");
}

void Tests::shortcutCommandOverrideEnter()
{
    RUN("setCommands([{name: 'test', inMenu: true, shortcuts: ['Enter'], cmd: 'copyq add OK'}])", "");
    KEYS("ENTER" << "ENTER");
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
    KEYS("CTRL+F1");
    WAIT_ON_OUTPUT("read" << "0", "test1");
    RUN("tab" << QString(clipboardTabName) << "size", "2\n");

    RUN("write" << "application/x-copyq-test2" << "", "");
    KEYS("CTRL+F1");
    WAIT_ON_OUTPUT("read" << "0", "test2");
    RUN("tab" << QString(clipboardTabName) << "size", "4\n");
}

void Tests::shortcutCommandMatchCmd()
{
    const auto tab = testTab(1);
    const Args args = Args("tab") << tab;

    // Activate only one of the three actions depending on exit code of command which matches input MIME format.
    const auto script = R"(
        function cmd(name) {
          var format = 'application/x-copyq-' + name
          return {
            name: name,
            inMenu: true,
            shortcuts: ['Ctrl+F1'],
            matchCmd: 'copyq: str(data("' + format + '")) || fail()',
            cmd: 'copyq tab )" + tab + R"( add ' + name
          }
        }
        setCommands([ cmd('test1'), cmd('test2') ])
        )";
    RUN(script, "");

    RUN("show" << tab, "");

    RUN(args << "write" << "application/x-copyq-test1" << "1", "");
    WAIT_ON_OUTPUT(args << "plugins.itemtests.keys('Ctrl+F1'); read(0)", "test1");

    RUN(args << "write" << "application/x-copyq-test2" << "2", "");
    WAIT_ON_OUTPUT(args << "plugins.itemtests.keys('Ctrl+F1'); read(0)", "test2");
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
    KEYS("CTRL+F1");
    WAIT_ON_OUTPUT("tab" << tab1 << "read" << "0", "C");
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
    KEYS("CTRL+F1");
    WAIT_ON_OUTPUT("read" << "2", "X");
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
    KEYS("CTRL+F1");
    WAIT_ON_OUTPUT("tab" << tab1 << "read" << "0", "B,C,");
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
    KEYS("CTRL+F1");
    WAIT_ON_OUTPUT("read" << "0" << "1" << "2", "A\nX\nY");
}

void Tests::shortcutCommandSelectedAndCurrent()
{
    const auto script = R"(
        setCommands([{
            name: 'Set Data for Second Selected Item',
            inMenu: true,
            shortcuts: ['Ctrl+F1'],
            output: 'text/plain',
            cmd: 'copyq: print(selectedItems() + "|" + currentItem() + "|" + selectedTab())'
        }])
        )";
    RUN(script, "");

    const auto tab1 = testTab(1);
    RUN("tab" << tab1 << "add" << "C" << "B" << "A", "");

    RUN("tab" << tab1 << "setCurrentTab" << tab1 << "selectItems" << "1" << "2", "true\n");
    KEYS("CTRL+F1");
    WAIT_ON_OUTPUT("tab" << tab1 << "read(0)", "1,2|2|" + tab1.toUtf8());
}

void Tests::shortcutCommandMoveSelected()
{
    const QString script = R"(
        setCommands([{
            name: 'Move Selected',
            inMenu: true,
            shortcuts: ['Ctrl+F1'],
            output: 'text/plain',
            cmd: 'copyq: move(%1); settings("done", 1)'
        }])
        )";
    RUN(script.arg(1), "");

    const Args args = Args("tab") << testTab(1) << "separator" << ",";
    RUN("setCurrentTab" << testTab(1), "");
    RUN(args << "add" << "4" << "3" << "2" << "1", "");

#define MOVE_SELECTED(EXPECTED_ITEMS) \
    RUN("settings" << "done" << "0" << "plugins.itemtests.keys" << "CTRL+F1", ""); \
    WAIT_ON_OUTPUT("settings" << "done", "1\n"); \
    RUN(args << "read(0,1,2,3,4)", EXPECTED_ITEMS)

    RUN(args << "selectItems" << "1" << "2", "true\n");
    MOVE_SELECTED("1,2,3,4,");

    RUN(args << "selectItems" << "2" << "3", "true\n");
    MOVE_SELECTED("1,3,4,2,");

    RUN(script.arg(5), "");
    MOVE_SELECTED("1,3,4,2,");

    RUN(script.arg(-1), "");
    MOVE_SELECTED("1,3,4,2,");

    RUN(script.arg(4), "");
    MOVE_SELECTED("1,2,3,4,");

    RUN(script.arg(0), "");
    MOVE_SELECTED("3,4,1,2,");

#undef MOVE_SELECTED
}

void Tests::automaticCommandIgnore()
{
    const auto script = R"(
        setCommands([
            { automatic: true, cmd: 'copyq ignore; copyq add OK' },
            { automatic: true, cmd: 'copyq add "SHOULD NOT BE EXECUTED"' }
        ])
        )";
    RUN(script, "");
    WAIT_ON_OUTPUT("commands().length", "2\n");

    TEST( m_test->setClipboard("SHOULD BE IGNORED 1") );
    WAIT_ON_OUTPUT("read" << "0", "OK");
    RUN("separator" << "," << "read" << "0" << "1" << "2", "OK,,");
    RUN("size", "1\n");

    TEST( m_test->setClipboard("SHOULD BE IGNORED 2") );
    WAIT_ON_OUTPUT("size", "2\n");

    RUN("separator" << "," << "read" << "0" << "1" << "2", "OK,OK,");
}

void Tests::automaticCommandRemove()
{
    const auto script = R"(
        setCommands([
            { automatic: true, remove: true, cmd: 'copyq add OK' },
            { automatic: true, cmd: 'copyq add "SHOULD NOT BE EXECUTED"' }
        ])
        )";
    RUN(script, "");
    WAIT_ON_OUTPUT("commands().length", "2\n");

    TEST( m_test->setClipboard("SHOULD BE IGNORED 1") );
    WAIT_ON_OUTPUT("read" << "0", "OK");

    TEST( m_test->setClipboard("SHOULD BE IGNORED 2") );
    WAIT_ON_OUTPUT("size", "2\n");

    RUN("separator" << "," << "read" << "0" << "1" << "2", "OK,OK,");
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
    WAIT_ON_OUTPUT("commands().length", "2\n");

    TEST( m_test->setClipboard("SHOULD BE CHANGED") );
    WAIT_ON_OUTPUT("read" << "0", "OK");
    RUN("read" << "DATA" << "0", "");
}

void Tests::automaticCommandRegExp()
{
    const auto script = R"(
        setCommands([
            { automatic: true, re: 'SHOULD BE (CHANGED)$', cmd: 'copyq: setData(mimeText, arguments[1])' },
            { automatic: true, cmd: 'copyq: setData("DATA", "DONE")' },
        ])
        )";
    RUN(script, "");
    WAIT_ON_OUTPUT("commands().length", "2\n");

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
    WAIT_ON_OUTPUT("commands().length", "1\n");

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
    WAIT_ON_OUTPUT("commands().length", "1\n");

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
    WAIT_ON_OUTPUT("commands().length", "1\n");

    TEST( m_test->setClipboard("TEST") );
    waitFor(1000);
    RUN("tab" << QString(clipboardTabName) << "size", "0\n");
}

void Tests::automaticCommandChaining()
{
    const auto script = R"(
        setCommands([
            {automatic: true, cmd: 'copyq: setData(mimeText, 1)'},
            {automatic: true, cmd: 'copyq: setData(mimeText, str(data(mimeText)) + 2)'},
            {automatic: true, cmd: 'copyq: setData(mimeText, str(data(mimeText)) + 3)'}
        ])
        )";
    RUN(script, "");
    WAIT_ON_OUTPUT("commands().length", "3\n");

    TEST( m_test->setClipboard("TEST") );
    WAIT_ON_OUTPUT("read" << "0", "123");
}

void Tests::automaticCommandCopyToTab()
{
    const auto tab1 = testTab(1);
    const auto script = R"(
        setCommands([{automatic: true, tab: ')" + tab1 + R"('}])
        )";
    RUN(script, "");
    WAIT_ON_OUTPUT("commands().length", "1\n");

    TEST( m_test->setClipboard("TEST") );
    WAIT_ON_OUTPUT("tab" << QString(clipboardTabName) << "read" << "0", "TEST");
    RUN("tab" << tab1 << "read" << "0", "TEST");
}

void Tests::automaticCommandStoreSpecialFormat()
{
    const auto script = R"(
        setCommands([
            { automatic: true, name: 'CMD1', input: 'test-format' }
        ])
        )";
    RUN(script, "");
    WAIT_ON_OUTPUT("commands().length", "1\n");

    TEST( m_test->setClipboard("DATA", "test-format") );
    WAIT_ON_OUTPUT("separator" << "," << "read" << "test-format" << "0" << "1", "DATA,");
}

void Tests::automaticCommandIgnoreSpecialFormat()
{
    const auto script = R"(
        setCommands([
            { automatic: true, name: 'CMD1', cmd: 'copyq add CMD1', input: 'test-format', remove: true },
            { automatic: true, name: 'CMD2', cmd: 'copyq add CMD2' },
            { automatic: true, name: 'CMD3', cmd: 'copyq add CMD3', input: 'test-format' }
        ])
        )";
    RUN(script, "");
    WAIT_ON_OUTPUT("commands().length", "3\n");

    TEST( m_test->setClipboard("SHOULD BE IGNORED", "test-format") );
    WAIT_ON_OUTPUT("separator" << "," << "read" << "0" << "1", "CMD1,");

    TEST( m_test->setClipboard("SHOULD NOT BE IGNORED") );
    WAIT_ON_OUTPUT("separator" << "," << "read" << "0" << "1" << "2" << "3", "SHOULD NOT BE IGNORED,CMD2,CMD1,");
}

void Tests::globalCommandInMenu()
{
    const auto script = R"(
        setCommands([
            { isGlobalShortcut: true, name: 'test', cmd: 'copyq add test' },
        ])
        )";
    RUN(script, "");
    WAIT_ON_OUTPUT("commands().length", "1\n");
    RUN("menu", "");
    KEYS(trayMenuId << "DOWN" << "ENTER");
    KEYS(clipboardBrowserId);
    WAIT_ON_OUTPUT("read(0)", "test");

    RUN("setCommands([])", "");
    WAIT_ON_OUTPUT("commands().length", "0\n");

    // Test sub-menus
    const auto script2 = R"(
        setCommands([
            { isGlobalShortcut: true, name: 'test|test1|test2', cmd: 'copyq add test2' },
        ])
        )";
    RUN(script2, "");
    WAIT_ON_OUTPUT("commands().length", "1\n");
    RUN("menu", "");
    KEYS(trayMenuId << "DOWN" << "DOWN" << "ENTER");
    waitFor(100);
    KEYS(trayMenuId << "ENTER");
    waitFor(100);
    KEYS(trayMenuId << "ENTER");
    KEYS(clipboardBrowserId);
    WAIT_ON_OUTPUT("read(0)", "test2");
}
