/*
    Copyright (c) 2021, Lukas Holecek <hluk@email.cz>

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

#include "command.h"

#include <QDataStream>

bool Command::operator==(const Command &other) const
{
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
        && display == other.display
        && inMenu == other.inMenu
        && isGlobalShortcut == other.isGlobalShortcut
        && isScript == other.isScript
        && transform == other.transform
        && remove == other.remove
        && hideWindow == other.hideWindow
        && enable == other.enable
        && icon == other.icon
        && shortcuts == other.shortcuts
        && globalShortcuts == other.globalShortcuts
        && tab == other.tab
        && outputTab == other.outputTab
        && internalId == other.internalId;
}

bool Command::operator!=(const Command &other) const {
    return !(*this == other);
}

int Command::type() const
{
    auto type =
           (automatic ? CommandType::Automatic : 0)
         | (display ? CommandType::Display : 0)
         | (isGlobalShortcut ? CommandType::GlobalShortcut : 0)
         | (inMenu && !name.isEmpty() ? CommandType::Menu : 0);

    // Scripts cannot be used in other types of commands.
    if (isScript)
        type = CommandType::Script;

    if (type == CommandType::None)
        type = CommandType::Invalid;

    if (!enable)
        type |= CommandType::Disabled;

    return type;
}

QDataStream &operator<<(QDataStream &out, const Command &command)
{
    out << command.name
        << command.re
        << command.wndre
        << command.matchCmd
        << command.cmd
        << command.sep
        << command.input
        << command.output
        << command.wait
        << command.automatic
        << command.display
        << command.inMenu
        << command.isGlobalShortcut
        << command.isScript
        << command.transform
        << command.remove
        << command.hideWindow
        << command.enable
        << command.icon
        << command.shortcuts
        << command.globalShortcuts
        << command.tab
        << command.outputTab
        << command.internalId;
    Q_ASSERT(out.status() == QDataStream::Ok);
    return out;
}

QDataStream &operator>>(QDataStream &in, Command &command)
{
    in >> command.name
       >> command.re
       >> command.wndre
       >> command.matchCmd
       >> command.cmd
       >> command.sep
       >> command.input
       >> command.output
       >> command.wait
       >> command.automatic
       >> command.display
       >> command.inMenu
       >> command.isGlobalShortcut
       >> command.isScript
       >> command.transform
       >> command.remove
       >> command.hideWindow
       >> command.enable
       >> command.icon
       >> command.shortcuts
       >> command.globalShortcuts
       >> command.tab
       >> command.outputTab
       >> command.internalId;
    Q_ASSERT(in.status() == QDataStream::Ok);
    return in;
}
