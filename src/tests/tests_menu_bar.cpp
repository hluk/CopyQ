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
    // while the menu bar is hidden.
    KEYS(clipboardBrowserId << "CTRL+M" << menuBarId);

    // Showing the menu bar clears the option, so the window cannot be left
    // with every way to the preferences hidden.
    RUN("config" << "hide_menu_bar", "false\n");
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
    KEYS(menuBarId);

    RUN("config" << "hide_menu_bar", "false\n");
}
