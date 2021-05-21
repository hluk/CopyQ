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

#ifndef COMMAND_H
#define COMMAND_H

#include <QString>
#include <QStringList>
#include <QRegularExpression>

namespace CommandType {
enum CommandType {
    None = 0,
    Invalid = 1,
    Automatic = 1 << 1,
    GlobalShortcut = 1 << 2,
    Menu = 1 << 3,
    Script = 1 << 4,
    Display = 1 << 5,
    Disabled = 1 << 6
};
} // namespace CommandType

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
    bool operator==(const Command &other) const;

    bool operator!=(const Command &other) const;

    int type() const;

    /** Name or short description. Used for menu item. */
    QString name;

    /** Regular expression to match items (empty matches all). */
    QRegularExpression re;

    /** Regular expression to match window titles (empty matches all). */
    QRegularExpression wndre;

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
    bool wait = false;

    /** If true run command automatically on new matched items. */
    bool automatic = false;

    /** If true, run command automatically when displaying matched items. */
    bool display = false;

    /** If true show command in context menu on matching items. */
    bool inMenu = false;

    /** If true command can be triggered by global shortcut. */
    bool isGlobalShortcut = false;

    /** If true, overrides scripting functionality. */
    bool isScript = false;

    /** If true change item data, don't create any new items. */
    bool transform = false;

    /** If true remove matched items. */
    bool remove = false;

    /** If true close window after command is activated from menu. */
    bool hideWindow = false;

    /** If false command is disabled and should not be used. */
    bool enable = true;

    /** Icon for menu item. */
    QString icon;

    /** Shortcut for menu item. */
    QStringList shortcuts;

    /** Global/system shortcut. */
    QStringList globalShortcuts;

    /** Copy item to other tab (automatically on new matched item). */
    QString tab;

    /** Tab for output items. */
    QString outputTab;

    QString internalId;
};

QDataStream &operator<<(QDataStream &out, const Command &command);

QDataStream &operator>>(QDataStream &in, Command &command);

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
constexpr bool _testCommandMetaType()
{
    return QTypeTraits::has_stream_operator_v<QDataStream, Command>;
}
constexpr const bool _testCommandMetaTypeOk = _testCommandMetaType();
static_assert(_testCommandMetaTypeOk, "");
#endif

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
Q_DECLARE_METATYPE(Command)
#endif

#endif // COMMAND_H
