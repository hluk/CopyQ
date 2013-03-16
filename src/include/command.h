/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>
    
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
        , cmd()
        , sep()
        , input()
        , output()
        , wait(false)
        , automatic(false)
        , ignore(false)
        , enable(true)
        , icon()
        , shortcut()
        , tab()
        , outputTab()
        {}

    /** Name or short description. Used for menu item. */
    QString name;

    /** Regular expression to match items (empty matches all). */
    QRegExp re;

    /** Regular expression to match window titles (empty matches all). */
    QRegExp wndre;

    /**
     * Program to execute on matched items.
     * Contains space separated list of arguments.
     * Argument %1 stands for item text.
     */
    QString cmd;

    /** Separator for output items. */
    QString sep;

    /**
     *  If true send item text to program's standard input.
     *  Also match only items with this format (match all if empty).
     */
    QString input;

    /** If true items are created from program's standard output. */
    QString output;

    /** Open action dialog before executing program. */
    bool wait;

    /** If true run command automatically on new matched items. */
    bool automatic;

    /** If true don't add matched item to list. */
    bool ignore;

    /** If false command is disabled and should not be used. */
    bool enable;

    /** Icon for menu item. */
    QString icon;

    /** Shortcut for menu item. */
    QString shortcut;

    /** Copy item to other tab (automatically on new matched item). */
    QString tab;

    /** Tab for output items. */
    QString outputTab;
};

#endif // COMMAND_H
