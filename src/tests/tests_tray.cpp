// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_utils.h"
#include "tests.h"
#include "tests_common.h"

#include "common/sleeptimer.h"

// WORKAROUND: Checking clipboard right after closing menu gets stuck on OS X.
#define ACTIVATE_MENU_ITEM(MENU_ID, WIDGET_ID, CONTENT) \
    KEYS(MENU_ID << "ENTER"); \
    KEYS(WIDGET_ID); \
    WAIT_FOR_CLIPBOARD(CONTENT)

void Tests::tray()
{
    RUN("add" << "A", "");
    KEYS(clipboardBrowserId);
    RUN("menu", "");
    ACTIVATE_MENU_ITEM(trayMenuId, clipboardBrowserId, "A");
}

void Tests::menu()
{
    const auto tab = testTab(1);

    RUN("tab" << tab << "add" << "D" << "C" << "B" << "A", "");

    KEYS(clipboardBrowserId);
    RUN("menu" << tab, "");
    ACTIVATE_MENU_ITEM(menuId, clipboardBrowserId, "A");

    // Show menu with 2 items from the tab and select last one.
    KEYS(clipboardBrowserId);
    RUN("menu" << tab << "2", "");
    KEYS(menuId << "END");
    ACTIVATE_MENU_ITEM(menuId, clipboardBrowserId, "B");

#ifdef Q_OS_MAC
    SKIP("Number keys don't seem to work in the tray menu on macOS.");
#endif

    // Select item by row number.
    RUN("tab" << tab << "add(3,2,1,0)", "");
    RUN("menu" << tab, "");
    KEYS(menuId << "3" << clipboardBrowserId);
    WAIT_FOR_CLIPBOARD("2");

    // Select item by index.
    RUN("config" << "row_index_from_one" << "false", "false\n");
    RUN("tab" << tab << "add(3,2,1,0)", "");
    RUN("menu" << tab, "");
    KEYS(menuId << "3" << clipboardBrowserId);
    WAIT_FOR_CLIPBOARD("3");
}

void Tests::traySearch()
{
    RUN("add" << "C" << "B" << "A", "");

    KEYS(clipboardBrowserId);
    RUN("menu", "");
    KEYS(trayMenuId << "B");
    ACTIVATE_MENU_ITEM(trayMenuId, clipboardBrowserId, "B");
}

void Tests::trayPaste()
{
    RUN("config" << "tray_tab_is_current" << "false", "false\n");

    const auto tab1 = testTab(1);
    RUN("setCurrentTab" << tab1, "");
    KEYS(clipboardBrowserId << "CTRL+N" << editorId << ":NEW ");

    RUN("add" << "TEST", "");
    KEYS(editorId);
    RUN("menu", "");
    ACTIVATE_MENU_ITEM(trayMenuId, editorId, "TEST");
    waitFor(waitMsPasteClipboard);

    KEYS(editorId << "F2");
    RUN("tab" << tab1 << "read" << "0", "NEW TEST");

    KEYS(clipboardBrowserId << "CTRL+N" << editorId << ":NEW ");

    RUN("config" << "tray_item_paste" << "false", "false\n");
    RUN("add" << "TEST2", "");
    KEYS(editorId);
    RUN("menu", "");
    ACTIVATE_MENU_ITEM(trayMenuId, editorId, "TEST2");

    KEYS(editorId << "F2");
    RUN("tab" << tab1 << "read" << "0", "NEW ");
}

void Tests::configTrayTab()
{
    const auto tab1 = testTab(1);
    RUN("tab" << tab1 << "add" << "A", "");

    const auto tab2 = testTab(2);
    RUN("tab" << tab2 << "add" << "B", "");

    RUN("config" << "tray_tab_is_current" << "false", "false\n");

    RUN("config" << "tray_item_paste" << "false", "false\n");

    RUN("config" << "tray_tab" << tab1, tab1 + "\n");

    KEYS(clipboardBrowserId);
    RUN("menu", "");
    ACTIVATE_MENU_ITEM(trayMenuId, clipboardBrowserId, "A");

    RUN("config" << "tray_tab" << tab2, tab2 + "\n");

    KEYS(clipboardBrowserId);
    RUN("menu", "");
    ACTIVATE_MENU_ITEM(trayMenuId, clipboardBrowserId, "B");
}

void Tests::configMove()
{
    SKIP_ON_ENV("COPYQ_TESTS_SKIP_CONFIG_MOVE");

    RUN("add" << "B" << "A", "");

    RUN("config" << "tray_item_paste" << "false", "false\n");

    RUN("config" << "move" << "true", "true\n");

    KEYS(clipboardBrowserId);
    RUN("menu", "");
    KEYS(trayMenuId << "DOWN");
    ACTIVATE_MENU_ITEM(trayMenuId, clipboardBrowserId, "B");
    RUN("read" << "0" << "1", "B\nA");

    RUN("config" << "move" << "false", "false\n");

    KEYS(clipboardBrowserId);
    RUN("menu", "");
    KEYS(trayMenuId << "DOWN");
    ACTIVATE_MENU_ITEM(trayMenuId, clipboardBrowserId, "A");
    RUN("read" << "0" << "1", "B\nA");
}

void Tests::configTrayTabIsCurrent()
{
    const auto tab1 = testTab(1);
    RUN("tab" << tab1 << "add" << "A", "");

    const auto tab2 = testTab(2);
    RUN("tab" << tab2 << "add" << "B", "");

    RUN("config" << "tray_tab_is_current" << "true", "true\n");

    RUN("setCurrentTab" << tab1, "");
    KEYS(clipboardBrowserId);
    RUN("menu", "");
    ACTIVATE_MENU_ITEM(trayMenuId, clipboardBrowserId, "A");

    RUN("setCurrentTab" << tab2, "");
    KEYS(clipboardBrowserId);
    RUN("menu", "");
    ACTIVATE_MENU_ITEM(trayMenuId, clipboardBrowserId, "B");
}
