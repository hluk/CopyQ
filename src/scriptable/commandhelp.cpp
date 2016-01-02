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

#include "commandhelp.h"

#include "scriptable/scriptable.h"

CommandHelp::CommandHelp()
    : cmd()
    , desc()
    , args()
{
}

CommandHelp::CommandHelp(const char *command, const QString &description)
    : cmd(command)
    , desc(description)
    , args()
{
}

CommandHelp &CommandHelp::addArg(const QString &arg)
{
    args.append(' ');
    args.append(arg);
    return *this;
}

QString CommandHelp::toString() const
{
    if (cmd.isNull())
        return "\n";

    const int indent = 23;
    bool indentFirst = desc.startsWith('\n');
    return QString("    %1").arg(cmd + args, indentFirst ? 0 : -indent)
            + (indentFirst ? QString() : QString("  "))
            + QString(desc).replace('\n', "\n" + QString(' ')
                                    .repeated(4 + 2 + (indentFirst ? 0 : indent))) + "\n";
}

QList<CommandHelp> commandHelp()
{
    static QList<CommandHelp> help = QList<CommandHelp>()
            << CommandHelp("show",
                           Scriptable::tr("Show main window and optionally open tab with given name."))
               .addArg("[" + Scriptable::tr("NAME") + "]")
            << CommandHelp("hide",
                           Scriptable::tr("Hide main window."))
            << CommandHelp("toggle",
                           Scriptable::tr("Show or hide main window."))
            << CommandHelp("menu",
                           Scriptable::tr("Open context menu."))
            << CommandHelp("exit",
                           Scriptable::tr("Exit server."))
            << CommandHelp("disable, enable",
                           Scriptable::tr("Disable or enable clipboard content storing."))
            << CommandHelp()
            << CommandHelp("clipboard",
                           Scriptable::tr("Print clipboard content."))
               .addArg("[" + Scriptable::tr("MIME") + "]")
           #ifdef COPYQ_WS_X11
            << CommandHelp("selection",
                           Scriptable::tr("Print X11 selection content."))
               .addArg("[" + Scriptable::tr("MIME") + "]")
           #endif
            << CommandHelp("paste",
                           Scriptable::tr("Paste clipboard to current window\n"
                                          "(may not work with some applications)."))
            << CommandHelp("copy", Scriptable::tr("Set clipboard text."))
               .addArg(Scriptable::tr("TEXT"))
            << CommandHelp("copy", Scriptable::tr("\nSet clipboard content."))
               .addArg(Scriptable::tr("MIME"))
               .addArg(Scriptable::tr("DATA"))
               .addArg("[" + Scriptable::tr("MIME") + " " + Scriptable::tr("DATA") + "]...")
            << CommandHelp()
            << CommandHelp("count",
                           Scriptable::tr("Print amount of items in current tab."))
            << CommandHelp("select",
                           Scriptable::tr("Copy item in the row to clipboard."))
               .addArg("[" + Scriptable::tr("ROW") + "=0]")
            << CommandHelp("next",
                           Scriptable::tr("Copy next item from current tab to clipboard."))
            << CommandHelp("previous",
                           Scriptable::tr("Copy previous item from current tab to clipboard."))
            << CommandHelp("add",
                           Scriptable::tr("Add text into clipboard."))
               .addArg(Scriptable::tr("TEXT") + "...")
            << CommandHelp("insert",
                           Scriptable::tr("Insert text into given row."))
               .addArg(Scriptable::tr("ROW"))
               .addArg(Scriptable::tr("TEXT"))
            << CommandHelp("remove",
                           Scriptable::tr("Remove items in given rows."))
               .addArg("[" + Scriptable::tr("ROWS") + "=0...]")
            << CommandHelp("edit",
                           Scriptable::tr("Edit items or edit new one.\n"
                                          "Value -1 is for current text in clipboard."))
               .addArg("[" + Scriptable::tr("ROW") + "=-1...]")
            << CommandHelp()
            << CommandHelp("separator",
                           Scriptable::tr("Set separator for items on output."))
               .addArg(Scriptable::tr("SEPARATOR"))
            << CommandHelp("read",
                           Scriptable::tr("Print raw data of clipboard or item in row."))
               .addArg("[" + Scriptable::tr("MIME") + "|" + Scriptable::tr("ROW") + "]...")
            << CommandHelp("write", Scriptable::tr("\nWrite raw data to given row."))
               .addArg("[" + Scriptable::tr("ROW") + "=0]")
               .addArg(Scriptable::tr("MIME"))
               .addArg(Scriptable::tr("DATA"))
               .addArg("[" + Scriptable::tr("MIME") + " " + Scriptable::tr("DATA") + "]...")
            << CommandHelp()
            << CommandHelp("action",
                           Scriptable::tr("Show action dialog."))
               .addArg("[" + Scriptable::tr("ROWS") + "=0...]")
            << CommandHelp("action",
                           Scriptable::tr("\nRun PROGRAM on item text in the rows.\n"
                                          "Use %1 in PROGRAM to pass text as argument."))
               .addArg("[" + Scriptable::tr("ROWS") + "=0...]")
               .addArg("[" + Scriptable::tr("PROGRAM") + " [" + Scriptable::tr("SEPARATOR") + "=\\n]]")
            << CommandHelp("popup",
                           Scriptable::tr("\nShow tray popup message for TIME milliseconds."))
               .addArg(Scriptable::tr("TITLE"))
               .addArg(Scriptable::tr("MESSAGE"))
               .addArg("[" + Scriptable::tr("TIME") + "=8000]")
            << CommandHelp()
            << CommandHelp("tab",
                           Scriptable::tr("List available tab names."))
            << CommandHelp("tab",
                           Scriptable::tr("Run command on tab with given name.\n"
                                          "Tab is created if it doesn't exist.\n"
                                          "Default is the first tab."))
               .addArg(Scriptable::tr("NAME"))
               .addArg("[" + Scriptable::tr("COMMAND") + "]")
            << CommandHelp("removetab",
                           Scriptable::tr("Remove tab."))
               .addArg(Scriptable::tr("NAME"))
            << CommandHelp("renametab",
                           Scriptable::tr("Rename tab."))
               .addArg(Scriptable::tr("NAME"))
               .addArg(Scriptable::tr("NEW_NAME"))
            << CommandHelp()
            << CommandHelp("exporttab",
                           Scriptable::tr("Export items to file."))
               .addArg(Scriptable::tr("FILE_NAME"))
            << CommandHelp("importtab",
                           Scriptable::tr("Import items from file."))
               .addArg(Scriptable::tr("FILE_NAME"))
            << CommandHelp()
            << CommandHelp("config",
                           Scriptable::tr("List all options."))
            << CommandHelp("config",
                           Scriptable::tr("Get option value."))
               .addArg(Scriptable::tr("OPTION"))
            << CommandHelp("config",
                           Scriptable::tr("Set option value."))
               .addArg(Scriptable::tr("OPTION"))
               .addArg(Scriptable::tr("VALUE"))
            << CommandHelp()
            << CommandHelp("eval, -e",
                           Scriptable::tr("\nEvaluate ECMAScript program.\n"
                                          "Arguments are accessible using with \"arguments(0..N)\"."))
               .addArg("[" + Scriptable::tr("SCRIPT") + "]")
               .addArg("[" + Scriptable::tr("ARGUMENTS") + "]...")
            << CommandHelp("session, -s, --session",
                           Scriptable::tr("\nStarts or connects to application instance with given session name."))
               .addArg(Scriptable::tr("SESSION"))
            << CommandHelp("help, -h, --help",
                           Scriptable::tr("\nPrint help for COMMAND or all commands."))
               .addArg("[" + Scriptable::tr("COMMAND") + "]...")
            << CommandHelp("version, -v, --version",
                           Scriptable::tr("\nPrint version of program and libraries."))
#ifdef HAS_TESTS
            << CommandHelp("tests, --tests",
                           Scriptable::tr("Run application tests (append --help argument for more info)."))
#endif
               ;

    return help;
}
