// SPDX-License-Identifier: GPL-3.0-or-later

#include "command.h"

#include <QDataStream>
#include <QLocale>

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
        && internalId == other.internalId
        && nameLocalization == other.nameLocalization;
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

QString Command::localizedName() const
{
    static const QLocale locale;
    static const QString code1 = locale.name();
    static const int i = code1.indexOf('_');
    static const QString code2 = i == -1 ? QString() : code1.mid(0, i);

    const auto value = nameLocalization.value(code1);
    if (!value.isEmpty())
        return value;

    if (code2.isEmpty())
        return name;

    return nameLocalization.value(code2, name);
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
        << command.internalId
        << command.nameLocalization;
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
       >> command.internalId
       >> command.nameLocalization;
    Q_ASSERT(in.status() == QDataStream::Ok);
    return in;
}
