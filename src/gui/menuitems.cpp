// SPDX-License-Identifier: GPL-3.0-or-later

#include "menuitems.h"

#include "common/shortcuts.h"
#include "gui/icons.h"

#include <QStringList>
#include <QVector>

namespace {

void addMenuItem(
        MenuItems &items, Actions::Id id, const QString &text, const QString &settingsKey,
        const QKeySequence &shortcut, const QString &iconName, ushort iconId = 0)
{
    Q_ASSERT(items[id].text.isEmpty() && "Menu item index must be same as its ID.");
    Q_UNUSED(id)

    MenuItem &item = items[id];
    item.text = text;
    item.settingsKey = settingsKey;
    item.defaultShortcut = shortcut;
    item.shortcuts = QList<QKeySequence>() << shortcut;
    item.iconName = iconName;
    item.iconId = iconId;
}

// Keep shortcut identity independent of UI translations. Portable packages can
// contain CopyQ translations without the matching Qt key-name catalogs, making
// translated NativeText impossible for QKeySequence to parse.
void addMenuItem(
        MenuItems &items, Actions::Id id, const QString &text, const QString &settingsKey,
        const char *shortcutPortableText, const QString &iconName, ushort iconId = 0)
{
    const auto shortcut = QKeySequence(
        QString::fromLatin1(shortcutPortableText), QKeySequence::PortableText);
    addMenuItem(items, id, text, settingsKey, shortcut, iconName, iconId);
}

} // namespace

MenuItems menuItems()
{
    MenuItems items;
    addMenuItem( items, Actions::File_New, QObject::tr("&New Item"), QStringLiteral("new"), QKeySequence::New,
                  QStringLiteral("document-new"), IconFileLines );
    addMenuItem( items, Actions::File_Import, QObject::tr("&Import..."), QStringLiteral("import"), "Ctrl+Shift+I",
                  QStringLiteral("document-open"), IconFolderOpen );
    addMenuItem( items, Actions::File_Export, QObject::tr("&Export..."), QStringLiteral("export"), QKeySequence::Save,
                  QStringLiteral("document-save"), IconFloppyDisk );
    addMenuItem( items, Actions::File_Preferences, QObject::tr("&Preferences..."), QStringLiteral("preferences"), "Ctrl+P",
                  QStringLiteral("preferences-other"), IconWrench );
    addMenuItem( items, Actions::File_Commands,
                  QObject::tr("C&ommands..."),
                  QStringLiteral("commands"), "F6", QStringLiteral("system-run"), IconGear );

    addMenuItem( items, Actions::File_ShowClipboardContent, QObject::tr("Show &Clipboard Content"),
                  QStringLiteral("show_clipboard_content"), "Ctrl+Shift+C", QStringLiteral("dialog-information"), IconPaste );
    addMenuItem( items, Actions::File_ShowPreview, QObject::tr("&Show Preview"),
                 QStringLiteral("show_item_preview"), "F7", QStringLiteral("document-print-preview"), IconEye );
    addMenuItem( items, Actions::File_ToggleClipboardStoring, QObject::tr("&Toggle Clipboard Storing"),
                  QStringLiteral("toggle_clipboard_storing"), "Ctrl+Shift+X", QStringLiteral(""), IconBan );
    addMenuItem( items, Actions::File_ProcessManager, QObject::tr("P&rocess Manager"),
                  QStringLiteral("process_manager"), "Ctrl+Shift+Z", QStringLiteral("system-search"), IconGears );
    addMenuItem( items, Actions::File_Exit, QObject::tr("E&xit"), QStringLiteral("exit"), "Ctrl+Q",
                  QStringLiteral("application-exit"), IconPowerOff );

    addMenuItem( items, Actions::Edit_SortSelectedItems, QObject::tr("&Sort Selected Items"),
                  QStringLiteral("sort_selected_items"), "Ctrl+Shift+S",
                  QStringLiteral("view-sort-ascending"), IconArrowDownAZ );
    addMenuItem( items, Actions::Edit_ReverseSelectedItems, QObject::tr("&Reverse Selected Items"),
                  QStringLiteral("reverse_selected_items"), "Ctrl+Shift+R",
                  QStringLiteral("view-sort-descending"), IconArrowUpAZ );
    addMenuItem( items, Actions::Edit_PasteItems, QObject::tr("&Paste Items"),
                  QStringLiteral("paste_selected_items"), QKeySequence::Paste, QStringLiteral("edit-paste"), IconPaste );
    addMenuItem( items, Actions::Edit_CopySelectedItems, QObject::tr("&Copy Selected Items"),
                  QStringLiteral("copy_selected_items"), QKeySequence::Copy, QStringLiteral("edit-copy"), IconCopy );
    addMenuItem( items, Actions::Edit_FindItems, QObject::tr("&Find"),
                  QStringLiteral("find_items"), QKeySequence::FindNext, QStringLiteral("edit-find"), IconMagnifyingGlass );

    addMenuItem(items, Actions::Editor_Save, QObject::tr("Save Item"),
                QStringLiteral("editor_save"), "F2", QStringLiteral("document-save"), IconFloppyDisk);
    addMenuItem(items, Actions::Editor_Cancel, QObject::tr("Cancel Editing"),
                QStringLiteral("editor_cancel"), "Escape", QStringLiteral("document-revert"), IconTrash);
    addMenuItem(items, Actions::Editor_Undo, QObject::tr("Undo"),
                QStringLiteral("editor_undo"), QKeySequence::Undo, QStringLiteral("edit-undo"), IconRotateLeft);
    addMenuItem(items, Actions::Editor_Redo, QObject::tr("Redo"),
                QStringLiteral("editor_redo"), QKeySequence::Redo, QStringLiteral("edit-redo"), IconRotateRight);
    addMenuItem(items, Actions::Editor_Font, QObject::tr("Font"),
                QStringLiteral("editor_font"), QKeySequence(), QStringLiteral("preferences-desktop-font"), IconFont);
    addMenuItem(items, Actions::Editor_Bold, QObject::tr("Bold"),
                QStringLiteral("editor_bold"), QKeySequence::Bold, QStringLiteral("format-text-bold"), IconBold);
    addMenuItem(items, Actions::Editor_Italic, QObject::tr("Italic"),
                QStringLiteral("editor_italic"), QKeySequence::Italic, QStringLiteral("format-text-italic"), IconItalic);
    addMenuItem(items, Actions::Editor_Underline, QObject::tr("Underline"),
                QStringLiteral("editor_underline"), QKeySequence::Underline, QStringLiteral("format-text-underline"), IconUnderline);
    addMenuItem(items, Actions::Editor_Strikethrough, QObject::tr("Strikethrough"),
                QStringLiteral("editor_strikethrough"), QKeySequence(), QStringLiteral("format-text-strikethrough"), IconStrikethrough);
    addMenuItem(items, Actions::Editor_Foreground, QObject::tr("Foreground"),
                QStringLiteral("editor_foreground"), QKeySequence(), QStringLiteral(""), IconPaintbrush);
    addMenuItem(items, Actions::Editor_Background, QObject::tr("Background"),
                QStringLiteral("editor_background"), QKeySequence(), QStringLiteral(""), IconSquare);
    addMenuItem(items, Actions::Editor_EraseStyle, QObject::tr("Erase Style"),
                QStringLiteral("editor_erase_style"), QKeySequence(), QStringLiteral(""), IconEraser);
    addMenuItem(items, Actions::Editor_Search, QObject::tr("Search"),
                QStringLiteral("editor_search"), QKeySequence::Find, QStringLiteral("edit-find"), IconMagnifyingGlass);

    addMenuItem( items, Actions::Item_MoveToClipboard,
                  QObject::tr("A&ctivate Items",
                              "copies selected items to clipboard and moves them to top (depending on settings)"),
                  QStringLiteral("move_to_clipboard"), QKeySequence(), QStringLiteral("clipboard"), IconPaste );
    addMenuItem( items, Actions::Item_ShowContent, QObject::tr("&Show Content..."),
                  QStringLiteral("show_item_content"), "F4", QStringLiteral("dialog-information"), IconCircleInfo );
    addMenuItem( items, Actions::Item_Remove, QObject::tr("&Remove"),
                  QStringLiteral("delete_item"),
                  QKeySequence(shortcutToRemove(), QKeySequence::PortableText), QStringLiteral("list-remove"), IconTrash );
    addMenuItem( items, Actions::Item_Edit, QObject::tr("&Edit"), QStringLiteral("edit"), "F2",
                  QStringLiteral("accessories-text-editor"), IconPenToSquare );
    addMenuItem( items, Actions::Item_EditNotes, QObject::tr("Edit &Notes"),
                  QStringLiteral("edit_notes"), "Shift+F2", QStringLiteral("accessories-text-editor"), IconPenToSquare );
    addMenuItem( items, Actions::Item_EditWithEditor, QObject::tr("E&dit with Editor"),
                  QStringLiteral("editor"), "Ctrl+E", QStringLiteral("accessories-text-editor"), IconPencil );
    addMenuItem( items, Actions::Item_Action, QObject::tr("&Action..."), QStringLiteral("system-run"), "F5",
                  QStringLiteral("action"), IconBolt );

    addMenuItem( items, Actions::Item_MoveUp, QObject::tr("Move Up"),
                  QStringLiteral("move_up"),  "Ctrl+Up", QStringLiteral("go-up"), IconAngleUp );
    addMenuItem( items, Actions::Item_MoveDown, QObject::tr("Move Down"),
                  QStringLiteral("move_down"),  "Ctrl+Down", QStringLiteral("go-down"), IconAngleDown );
    addMenuItem( items, Actions::Item_MoveToTop, QObject::tr("Move to Top"),
                  QStringLiteral("move_to_top"),  "Ctrl+Home", QStringLiteral("go-top"), IconAnglesUp );
    addMenuItem( items, Actions::Item_MoveToBottom, QObject::tr("Move to Bottom"),
                  QStringLiteral("move_to_bottom"),  "Ctrl+End", QStringLiteral("go-bottom"), IconAnglesDown );

    addMenuItem( items, Actions::Tabs_NewTab, QObject::tr("&New Tab"),
                  QStringLiteral("new_tab"), "Ctrl+T", QStringLiteral(":/images/tab_new") );
    addMenuItem( items, Actions::Tabs_RenameTab, QObject::tr("R&ename Tab"),
                  QStringLiteral("rename_tab"), "Ctrl+F2", QStringLiteral(":/images/tab_rename") );
    addMenuItem( items, Actions::Tabs_RemoveTab, QObject::tr("Re&move Tab"),
                  QStringLiteral("remove_tab"), "Ctrl+W", QStringLiteral(":/images/tab_remove") );
    addMenuItem( items, Actions::Tabs_ChangeTabIcon, QObject::tr("&Change Tab Icon"),
                  QStringLiteral("change_tab_icon"), "Ctrl+Shift+T", QStringLiteral(":/images/tab_icon") );
    addMenuItem( items, Actions::Tabs_NextTab, QObject::tr("Ne&xt Tab"),
                  QStringLiteral("next_tab"), "Right",
                  QStringLiteral("go-next"), IconArrowRight );
    addMenuItem( items, Actions::Tabs_PreviousTab, QObject::tr("&Previous Tab"),
                  QStringLiteral("previous_tab"), "Left",
                  QStringLiteral("go-previous"), IconArrowLeft );

    addMenuItem( items, Actions::Help_Help, QObject::tr("&Help"), QStringLiteral("help"), QKeySequence::HelpContents,
                  QStringLiteral("help-contents"), IconCircleQuestion );
    addMenuItem( items, Actions::Help_ShowLog, QObject::tr("&Show Log"), QStringLiteral("show-log"), "F12",
                  QStringLiteral("help-about"), IconCircleExclamation );
    addMenuItem( items, Actions::Help_About, QObject::tr("&About"), QStringLiteral("about"), QKeySequence::WhatsThis,
                  QStringLiteral("help-about"), IconCircleInfo );

    addMenuItem( items, Actions::ItemMenu, QObject::tr("Open Item Context Menu"), QStringLiteral("item-menu"),
                 "Shift+F10",
                 QStringLiteral(""), IconRectangleList );

    return items;
}

void loadShortcuts(MenuItems *items, const QSettings &settings)
{
    for (auto &item : *items) {
        if ( !item.settingsKey.isEmpty() ) {
            const QVariant shortcutNames = settings.value(item.settingsKey);
            if ( shortcutNames.isValid() ) {
                item.shortcuts.clear();
                for ( const auto &shortcut : shortcutNames.toStringList() )
                    item.shortcuts.append(shortcut);
            }
        }
    }
}
