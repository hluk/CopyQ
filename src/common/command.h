/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef COMMAND_H
#define COMMAND_H

#include <QString>
#include <QStringList>
#include <QRegExp>

/**
 * Command for matched items.
 *
 * Executes an command for matched items.
 * Possible commands are (can be combined):
 * * execute program,
 * * copy item to other tab or
 * * don't add new item to list.
 */
struct Command {
    Command()
        : name()
        , re()
        , wndre()
        , matchCmd()
        , cmd()
        , sep()
        , input()
        , output()
        , wait(false)
        , automatic(false)
        , inMenu(false)
        , transform(false)
        , remove(false)
        , hideWindow(false)
        , enable(true)
        , icon()
        , shortcuts()
        , globalShortcuts()
        , tab()
        , outputTab()
        {}

    bool operator==(Command &other) const {
        return name == other.name
            && re == other.re
            && wndre == other.wndre
            && matchCmd == other.matchCmd
            && cmd == other.cmd
            && sep == other.sep
            && input == other.input
            && output == other.output
            && wait == other.wait
            && automatic == other.automatic
            && inMenu == other.inMenu
            && transform == other.transform
            && remove == other.remove
            && hideWindow == other.hideWindow
            && enable == other.enable
            && icon == other.icon
            && shortcuts == other.shortcuts
            && globalShortcuts == other.globalShortcuts
            && tab == other.tab
            && outputTab == other.outputTab;
    }

    bool operator!=(Command &other) const {
        return !(*this == other);
    }

    /** Name or short description. Used for menu item. */
    QString name;

    /** Regular expression to match items (empty matches all). */
    QRegExp re;

    /** Regular expression to match window titles (empty matches all). */
    QRegExp wndre;

    /**
     * Program to execute to match items.
     * Contains space separated list of arguments.
     * Item is passed to stdin of the program.
     */
    QString matchCmd;

    /**
     * Program to execute on matched items.
     * Contains space separated list of arguments.
     * Argument %1 stands for item text.
     */
    QString cmd;

    /** Separator for output items. */
    QString sep;

    /**
     *  If not empty send item text to program's standard input.
     *  Also match only items with this format (match all if empty).
     */
    QString input;

    /** MIME for new items created from program's stdout. */
    QString output;

    /** Open action dialog before executing program. */
    bool wait;

    /** If true run command automatically on new matched items. */
    bool automatic;

    /** If true show command in context menu on matching items. */
    bool inMenu;

    /** If true change item data, don't create any new items. */
    bool transform;

    /** If true remove matched items. */
    bool remove;

    /** If true close window after command is activated from menu. */
    bool hideWindow;

    /** If false command is disabled and should not be used. */
    bool enable;

    /** Icon for menu item. */
    QString icon;

    /** Shortcut for menu item. */
    QStringList shortcuts;

    /** Global/sytem shortcut. */
    QStringList globalShortcuts;

    /** Copy item to other tab (automatically on new matched item). */
    QString tab;

    /** Tab for output items. */
    QString outputTab;
};

#endif // COMMAND_H
