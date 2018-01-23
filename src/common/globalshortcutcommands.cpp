/*
    Copyright (c) 2018, Lukas Holecek <hluk@email.cz>

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

#include "globalshortcutcommands.h"

#include "common/command.h"
#include "common/shortcuts.h"
#include "gui/icons.h"

#include <QCoreApplication>
#include <QLocale>

#ifndef NO_GLOBAL_SHORTCUTS

namespace {

constexpr auto commandScreenshot = R"(
var imageData = screenshotSelect()
write('image/png', imageData)
copy('image/png', imageData)
)";

constexpr auto commandPasteDateTimeTemplate = R"(
// http://doc.qt.io/qt-5/qdatetime.html#toString
var format = '%1'
var dateTime = dateString(format)
copy(dateTime)
paste()
)";

class AddCommandDialog
{
    Q_DECLARE_TR_FUNCTIONS(AddCommandDialog)
};

QString commandPasteDateTime()
{
    const auto format = QLocale::system().dateTimeFormat(QLocale::LongFormat)
            .replace("\\", "\\\\")
            .replace("'", "\\'")
            .replace("\n", "\\n");

    return QString(commandPasteDateTimeTemplate).arg(format);
}

Command createGlobalShortcut(const QString &name, const QString &script, IconId icon)
{
    Command c;
    c.name = name;
    c.cmd = "copyq: " + script;
    c.icon = QString(QChar(icon));

    const auto shortcutNativeText =
            AddCommandDialog::tr("Ctrl+Shift+1", "Global shortcut for some predefined commands");
    c.globalShortcuts = QStringList(toPortableShortcutText(shortcutNativeText));

    return c;
}

} // namespace

QVector<Command> globalShortcutCommands()
{
    return QVector<Command>()
            << createGlobalShortcut( AddCommandDialog::tr("Show/hide main window"), "toggle()", IconListAlt )
            << createGlobalShortcut( AddCommandDialog::tr("Show the tray menu"), "menu()", IconInbox )
            << createGlobalShortcut( AddCommandDialog::tr("Show main window under mouse cursor"), "showAt()", IconListAlt )
            << createGlobalShortcut( AddCommandDialog::tr("Edit clipboard"), "edit(-1)", IconEdit )
            << createGlobalShortcut( AddCommandDialog::tr("Edit first item"), "edit(0)", IconEdit )
            << createGlobalShortcut( AddCommandDialog::tr("Copy second item"), "select(1)", IconCopy )
            << createGlobalShortcut( AddCommandDialog::tr("Show action dialog"), "action()", IconCog )
            << createGlobalShortcut( AddCommandDialog::tr("Create new item"), "edit()", IconAsterisk )
            << createGlobalShortcut( AddCommandDialog::tr("Copy next item"), "next()", IconArrowDown )
            << createGlobalShortcut( AddCommandDialog::tr("Copy previous item"), "previous()", IconArrowUp )
            << createGlobalShortcut( AddCommandDialog::tr("Paste clipboard as plain text"), pasteAsPlainTextScript("clipboard()"), IconPaste )
            << createGlobalShortcut( AddCommandDialog::tr("Disable clipboard storing"), "disable()", IconEyeSlash )
            << createGlobalShortcut( AddCommandDialog::tr("Enable clipboard storing"), "enable()", IconEye )
            << createGlobalShortcut( AddCommandDialog::tr("Paste and copy next"), "paste(); next()", IconArrowCircleDown )
            << createGlobalShortcut( AddCommandDialog::tr("Paste and copy previous"), "paste(); previous()", IconArrowCircleUp )
            << createGlobalShortcut( AddCommandDialog::tr("Take screenshot"), commandScreenshot, IconCamera )
            << createGlobalShortcut( AddCommandDialog::tr("Paste current date and time"), commandPasteDateTime(), IconClock );
}
#else
QVector<Command> globalShortcutCommands()
{
    return QVector<Command>();
}
#endif

QString pasteAsPlainTextScript(const QString &what)
{
    return "\n"
           "var text = " + what + "\n"
           "copy(text)\n"
           "copySelection(text)\n"
           "paste()";
}
