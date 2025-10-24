// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_utils.h"
#include "tests.h"

#include "common/commandstatus.h"
#include "common/mimetypes.h"
#include "common/sleeptimer.h"

void Tests::scriptCommandLoaded()
{
    const auto script = R"(
        setCommands([{
            isScript: true,
            cmd: 'add("LOADED")'
        }])
        )";
    RUN(script, "");
    RUN("read(0)", "LOADED");
}

void Tests::scriptCommandAddFunction()
{
    const auto script = R"(
        setCommands([{
            isScript: true,
            cmd: 'global.test = function() { return "TEST"; }'
        }])
        )";
    RUN(script, "");
    RUN("test", "TEST\n");
}

void Tests::scriptCommandOverrideFunction()
{
    const auto script = R"(
        setCommands([{
            isScript: true,
            cmd: 'popup = function(msg) { return msg; }'
        }])
        )";
    RUN(script, "");
    RUN("popup" << "test" << "xxx", "test");
}

void Tests::scriptCommandEnhanceFunction()
{
    const auto script = R"(
        setCommands([
            {
                isScript: true,
                cmd: 'var popup_ = popup; global.popup = function(msg) { popup_(msg); return msg + 1; }'
            },
            {
                isScript: true,
                cmd: 'var popup_ = popup; global.popup = function(msg) { return popup_(msg) + msg + 2; }'
            },
        ])
        )";
    RUN(script, "");
    RUN("popup" << "test", "test1test2\n");
}

void Tests::scriptCommandEndingWithComment()
{
    /*
    With Qml scripts in Qt 5, it's not possible to execute script in new context,
    only in the global one.

    Workaround is to wrap the script properly in a new function:

        function() {
            %1
        }()

    (Unfortunately, it's still possible to escape the new context with a script injection.)
    */

    const auto script = R"(
        setCommands([
            {
                isScript: true,
                cmd: 'global.popup = function(msg) { return msg + 1; } // TEST'
            },
        ])
        )";
    RUN(script, "");
    RUN("popup" << "test", "test1\n");
}

void Tests::scriptCommandWithError()
{
    const auto script = R"(
        setCommands([
            {
                isScript: true,
                name: 'bad_script',
                cmd: 'if (env("COPYQ_TEST_THROW") == "1") throw Error("BAD SCRIPT")'
            },
        ])
        )";
    RUN(script, "");
    m_test->setEnv("COPYQ_TEST_THROW", "1");
    RUN_EXPECT_ERROR_WITH_STDERR(
        "", CommandError,
        "ScriptError: BAD SCRIPT\n"
        "\n"
        "--- backtrace ---\n"
    );
    RUN_EXPECT_ERROR_WITH_STDERR(
        "", CommandError,
        "\neval:source@<bad_script>\n"
        "--- end backtrace ---\n"
    );
    m_test->setEnv("COPYQ_TEST_THROW", "0");
}

void Tests::scriptPaste()
{
    const auto script = R"(
        setCommands([
            {
                isScript: true,
                cmd: 'global.paste = function() { add("PASTE") }'
            },
        ])
        )";
    RUN(script, "");
    RUN("add(1)", "");
    KEYS(clipboardBrowserId << "ENTER");
    WAIT_ON_OUTPUT("read(0)", "PASTE");
}

void Tests::scriptOnTabSelected()
{
    const auto script = R"(
        setCommands([
            {
                isScript: true,
                cmd: 'global.onTabSelected = function() { add(selectedTab()) }'
            },
        ])
        )";
    RUN(script, "");

    const auto tab1 = testTab(1);
    const auto tab2 = testTab(2);
    RUN("show" << tab1, "");
    WAIT_ON_OUTPUT("tab" << tab1 << "read(0)", tab1);
    RUN("show" << tab2, "");
    WAIT_ON_OUTPUT("tab" << tab2 << "read(0)", tab2);
}

void Tests::scriptOnItemsRemoved()
{
    const auto script = R"(
        setCommands([
            {
                isScript: true,
                cmd: `
                  global.onItemsRemoved = function() {
                    items = ItemSelection().current().items();
                    tab(tab()[0]);
                    add("R0:" + str(items[0][mimeText]));
                    add("R1:" + str(items[1][mimeText]));
                  }
                `
            },
        ])
        )";
    RUN(script, "");
    const auto tab1 = testTab(1);
    RUN("tab" << tab1 << "add(3,2,1,0)", "");
    RUN("tab" << tab1 << "remove(1,2)", "");
    WAIT_ON_OUTPUT("separator" << "," << "read(0,1,2,)", "R1:2,R0:1,");

    // Cancel item removal
    const auto script2 = R"(
        setCommands([
            {
                isScript: true,
                cmd: "global.onItemsRemoved = global.fail",
            },
        ])
        )";
    RUN(script2, "");
    const auto tab2 = testTab(2);
    RUN("tab" << tab2 << "add(3,2,1,0)", "");
    RUN("tab" << tab2 << "remove(1,2)", "");
    waitFor(1000);
    RUN("tab" << tab2 << "separator" << "," << "read(0,1,2,3,4)", "0,1,2,3,");

    // Avoid crash if the tab itself is removed while removing items
    const auto script3 = R"(
        setCommands([
            {
                isScript: true,
                cmd: `
                  global.onItemsRemoved = function() {
                    removeTab(selectedTab())
                  }
                `
            },
        ])
        )";
    RUN(script3, "");
    const auto tab3 = testTab(3);
    RUN("tab" << tab3 << "add(3,2,1,0)", "");
    RUN("tab" << tab3 << "remove(1,2)", "");
    waitFor(1000);
    RUN("tab" << tab3 << "separator" << "," << "read(0,1,2,3,4)", ",,,,");
}

void Tests::scriptOnItemsAdded()
{
    const auto script = R"(
        setCommands([
            {
                isScript: true,
                cmd: `
                  global.onItemsAdded = function() {
                    sel = ItemSelection().current();
                    items = sel.items();
                    for (i = 0; i < items.length; ++i)
                        items[i][mimeText] = "A:" + str(items[i][mimeText])
                    sel.setItems(items);
                  }
                `
            },
        ])
        )";
    RUN(script, "");
    const auto tab1 = testTab(1);
    RUN("tab" << tab1 << "add(1,0)", "");
    WAIT_ON_OUTPUT("tab" << tab1 << "separator" << "," << "read(0,1,2)", "A:0,A:1,");
}

void Tests::scriptOnItemsChanged()
{
    const auto script = R"(
        setCommands([
            {
                isScript: true,
                cmd: `
                  global.onItemsChanged = function() {
                    if (selectedTab() == tab()[0]) abort();
                    items = ItemSelection().current().items();
                    tab(tab()[0]);
                    add("C:" + str(items[0][mimeText]));
                  }
                `
            },
        ])
        )";
    RUN(script, "");
    const auto tab1 = testTab(1);
    RUN("tab" << tab1 << "add(0)", "");
    RUN("tab" << tab1 << "change(0, mimeText, 'A')", "");
    WAIT_ON_OUTPUT("separator" << "," << "read(0,1,2)", "C:A,,");
    RUN("tab" << tab1 << "change(0, mimeText, 'B')", "");
    WAIT_ON_OUTPUT("separator" << "," << "read(0,1,2)", "C:B,C:A,");
}

void Tests::scriptOnItemsLoaded()
{
    const auto script = R"(
        setCommands([
            {
                isScript: true,
                cmd: `
                  global.onItemsLoaded = function() {
                    if (selectedTab() == tab()[0]) abort();
                    tab(tab()[0]);
                    add(selectedTab());
                  }
                `
            },
        ])
        )";
    RUN(script, "");

    const auto tab1 = testTab(1);
    RUN("show" << tab1, "");
    WAIT_ON_OUTPUT("separator" << "," << "read(0,1,2)", tab1 + ",,");

    const auto tab2 = testTab(2);
    RUN("show" << tab2, "");
    WAIT_ON_OUTPUT("separator" << "," << "read(0,1,2)", tab2 + "," + tab1 + ",");
}

void Tests::scriptEventMaxRecursion()
{
    const auto script = R"(
        setCommands([
            {
                isScript: true,
                cmd: `global.onItemsRemoved = function() {
                    const toRemove = str(selectedItemData(0)[mimeText]);
                    const newItem = (toRemove == "X") ? "A" : ("WRONG:" + toRemove);
                    add(newItem);
                    remove(size()-1);
                }`
            },
        ])
        )";
    RUN(script, "");
    RUN("add('X'); remove(0)", "");
    WAIT_ON_OUTPUT("separator" << "," << "read(0,1,2,3,4,5,6,7,8,9,10)", "A,A,A,A,A,A,A,A,A,A,");
    waitFor(200);
    RUN("separator" << "," << "read(0,1,2,3,4,5,6,7,8,9,10)", "A,A,A,A,A,A,A,A,A,A,");
}

void Tests::scriptSlowCollectOverrides()
{
    const auto script = R"(
        setCommands([
            {
                isScript: true,
                cmd: 'global.onTabSelected = function() { add(selectedTab()) }'
            },
            {
                isScript: true,
                cmd: `
                  var collectOverrides_ = global.collectOverrides;
                  global.collectOverrides = function() { sleep(1000); collectOverrides_() }
                `
            },
        ])
        )";
    RUN(script, "");

    const auto tab1 = testTab(1);
    RUN("show" << tab1, "");
    WAIT_ON_OUTPUT("tab" << tab1 << "read(0)", tab1);
}

void Tests::displayCommand()
{
    const auto testMime = COPYQ_MIME_PREFIX "test";
    const auto script = QString(R"(
        setCommands([{
            display: true,
            input: '%1',
            cmd: 'copyq:'
               + 'text = str(data(mimeText));'
               + 'currentTab = str(data(mimeCurrentTab));'
               + 'add(currentTab + "/" + text);'
        }])
        )").arg(testMime);

    RUN(script, "");

    RUN("write" << "0" << testMime << "" << mimeText << "a", "");
    WAIT_ON_OUTPUT(
                "read(0,1,2)",
                QString::fromLatin1("%1/a\na\n")
                .arg(clipboardTabName)
                .toUtf8() );

    RUN("write" << "0" << testMime << "" << mimeText << "b", "");
    WAIT_ON_OUTPUT(
                "read(0,1,2,3,4)",
                QString::fromLatin1("%1/b\nb\n%1/a\na\n")
                .arg(clipboardTabName)
                .toUtf8() );
}

void Tests::displayCommandForMenu()
{
    const auto tab = testTab(1);
    const auto args = Args("tab") << tab << "separator" << ",";
    const auto script = QString(R"(
        setCommands([{
            display: true,
            cmd: 'copyq:'
               + 'currentTab = str(data(mimeCurrentTab));'
               + 'inMenu = str(data(mimeDisplayItemInMenu));'
               + 'if (inMenu != "1" || currentTab != "%1") abort();'
               + 'text = str(data(mimeText));'
               + 'setData(mimeText, "display:" + text);'
               + 'setData(mimeIcon, String.fromCharCode(0xF328));'
               + 'setData("application/x-copyq-item-tag", "TAG");'
               + 'tab(tab()[0]);'
               + 'old = str(read(0));'
               + 'add(old + "|" + text);'
        }])
        )").arg(tab);

    RUN("config" << "tray_tab" << tab, tab + "\n");
    RUN("config" << "tray_tab_is_current" << "false", "false\n");
    RUN(script, "");

    RUN(args << "add(1,2,3,4,5)", "");
    RUN("menu", "");
    WAIT_ON_OUTPUT("read(0)", "|5|4|3|2|1");
}
