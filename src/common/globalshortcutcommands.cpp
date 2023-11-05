// SPDX-License-Identifier: GPL-3.0-or-later

#include "globalshortcutcommands.h"

#include "common/command.h"

#include <QCoreApplication>
#include <QLocale>
#include <QVector>

#ifdef COPYQ_GLOBAL_SHORTCUTS
#   include "common/shortcuts.h"
#   include "gui/icons.h"

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
copySelection(dateTime)
paste()
)";

class AddCommandDialog final
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

Command createGlobalShortcut(const QString &name, const QString &script, IconId icon, const QString &internalId)
{
    Command c;
    c.internalId = internalId;
    c.name = name;
    c.cmd = "copyq: " + script;
    c.icon = QString(QChar(icon));
    c.isGlobalShortcut = true;
    return c;
}

} // namespace

QVector<Command> globalShortcutCommands()
{
    return {
        createGlobalShortcut( AddCommandDialog::tr("Show/hide main window"), "toggle()", IconRectangleList, "copyq_global_toggle"),
        createGlobalShortcut( AddCommandDialog::tr("Show the tray menu"), "menu()", IconInbox, "copyq_global_menu"),
        createGlobalShortcut( AddCommandDialog::tr("Show main window under mouse cursor"), "showAt()", IconRectangleList, "copyq_global_show_under_mouse"),
        createGlobalShortcut( AddCommandDialog::tr("Edit clipboard"), "edit(-1)", IconPenToSquare, "copyq_global_edit_clipboard"),
        createGlobalShortcut( AddCommandDialog::tr("Edit first item"), "edit(0)", IconPenToSquare, "copyq_global_edit_first_item"),
        createGlobalShortcut( AddCommandDialog::tr("Copy second item"), "select(1)", IconCopy, "copyq_global_copy_second_item"),
        createGlobalShortcut( AddCommandDialog::tr("Show action dialog"), "action()", IconGear, "copyq_global_show_action_dialog"),
        createGlobalShortcut( AddCommandDialog::tr("Create new item"), "edit()", IconAsterisk, "copyq_global_create_new_item"),
        createGlobalShortcut( AddCommandDialog::tr("Copy next item"), "next()", IconArrowDown, "copyq_global_copy_next"),
        createGlobalShortcut( AddCommandDialog::tr("Copy previous item"), "previous()", IconArrowUp, "copyq_global_copy_previous"),
        createGlobalShortcut( AddCommandDialog::tr("Paste clipboard as plain text"), pasteAsPlainTextScript("clipboard()"), IconPaste, "copyq_global_paste_clipboard_plain"),
        createGlobalShortcut( AddCommandDialog::tr("Disable clipboard storing"), "disable()", IconEyeSlash, "copyq_global_disable_clipboard_store"),
        createGlobalShortcut( AddCommandDialog::tr("Enable clipboard storing"), "enable()", IconEye, "copyq_global_enable_clipboard_store"),
        createGlobalShortcut( AddCommandDialog::tr("Paste and copy next"), "paste(); next()", IconCircleArrowDown, "copyq_global_paste_copy_next"),
        createGlobalShortcut( AddCommandDialog::tr("Paste and copy previous"), "paste(); previous()", IconCircleArrowUp, "copyq_global_paste_copy_previous"),
        createGlobalShortcut( AddCommandDialog::tr("Take screenshot"), commandScreenshot, IconCamera, "copyq_global_screenshot"),
        createGlobalShortcut( AddCommandDialog::tr("Paste current date and time"), commandPasteDateTime(), IconClock , "copyq_global_paste_datetime"),
    };
}
#else
QVector<Command> globalShortcutCommands()
{
    return {};
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
