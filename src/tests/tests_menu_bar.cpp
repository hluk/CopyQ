// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_utils.h"
#include "tests.h"

namespace {

constexpr auto menuBarId = "focus:^menu_bar:QMenuBar";

} // namespace

void CoreTests::menuBarVisibleByDefault()
{
    RUN("config" << "hide_menu_bar", "false\n");
}

void CoreTests::menuBarShortcutShowsHiddenMenuBar()
{
    RUN("config" << "hide_menu_bar" << "true", "true\n");
    RUN("show", "");

    // The action is owned by the main window, so its shortcut keeps working
    // while the menu bar is hidden. It opens the first menu.
    KEYS(clipboardBrowserId << "CTRL+M" << menuId);

    // The menu bar is shown only until the menu closes; the option stays.
    KEYS(menuId << "ESCAPE" << clipboardBrowserId);
    RUN("config" << "hide_menu_bar", "true\n");
}

void CoreTests::menuBarMnemonicOpensMenuOfHiddenMenuBar()
{
    RUN("config" << "hide_menu_bar" << "true", "true\n");
    RUN("show", "");

    // Alt+F opens the File menu straight away, and the arrow keys move to
    // the next menu, as with a visible menu bar.
    KEYS(clipboardBrowserId << "ALT+F" << menuId);
    KEYS(menuId << "RIGHT" << menuId);
    KEYS(menuId << "ESCAPE" << clipboardBrowserId);
    RUN("config" << "hide_menu_bar", "true\n");
}

void CoreTests::menuBarContextMenuShowsMenuBar()
{
    RUN("config" << "hide_menu_bar" << "true", "true\n");
    RUN("add" << "ITEM", "");
    RUN("show", "");

    KEYS(clipboardBrowserId << "SHIFT+F10");

    // While the menu bar is hidden, the item context menu ends with the
    // replacements for it: Show Menu Bar, Preferences and Exit.
    KEYS(menuId << "UP" << "UP" << "UP" << "ENTER");
    KEYS(menuId << "ESCAPE" << clipboardBrowserId);

    RUN("config" << "hide_menu_bar", "true\n");
}

void CoreTests::menuBarContextMenuWithoutItems()
{
    RUN("config" << "hide_menu_bar" << "true", "true\n");
    RUN("show", "");

    // Without an item there is no item menu, so the context menu has only
    // the replacements for the menu bar.
    KEYS(clipboardBrowserId << "SHIFT+F10");
    KEYS(menuId << "UP" << "UP" << "ENTER");
    KEYS(configurationDialogId << "ESCAPE" << clipboardBrowserId);

    KEYS(clipboardBrowserId << "SHIFT+F10");
    KEYS(menuId << "UP" << "UP" << "UP" << "ENTER");
    KEYS(menuId << "ESCAPE" << clipboardBrowserId);
}
