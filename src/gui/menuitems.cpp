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

void addMenuItem(
        MenuItems &items, Actions::Id id, const QString &text, const QString &settingsKey,
        const QString &shortcutText, const QString &iconName, ushort iconId = 0)
{
    const QKeySequence shortcut = QKeySequence(shortcutText, QKeySequence::NativeText);
    addMenuItem(items, id, text, settingsKey, shortcut, iconName, iconId);
}

} // namespace

MenuItems menuItems()
{
    MenuItems items;
    addMenuItem( items, Actions::File_New, QObject::tr("&New Item"), QStringLiteral("new"), QKeySequence::New,
                  QStringLiteral("document-new"), IconFileLines );
    addMenuItem( items, Actions::File_Import, QObject::tr("&Import..."), QStringLiteral("import"), QObject::tr("Ctrl+I"),
                  QStringLiteral("document-open"), IconFolderOpen );
    addMenuItem( items, Actions::File_Export, QObject::tr("&Export..."), QStringLiteral("export"), QKeySequence::Save,
                  QStringLiteral("document-save"), IconFloppyDisk );
    addMenuItem( items, Actions::File_Preferences, QObject::tr("&Preferences..."), QStringLiteral("preferences"), QObject::tr("Ctrl+P"),
                  QStringLiteral("preferences-other"), IconWrench );
    addMenuItem( items, Actions::File_Commands,
                  QObject::tr("C&ommands..."),
                  QStringLiteral("commands"), QObject::tr("F6"), QStringLiteral("system-run"), IconGear );

    addMenuItem( items, Actions::File_ShowClipboardContent, QObject::tr("Show &Clipboard Content"),
                  QStringLiteral("show_clipboard_content"), QObject::tr("Ctrl+Shift+C"), QStringLiteral("dialog-information"), IconPaste );
    addMenuItem( items, Actions::File_ShowPreview, QObject::tr("&Show Preview"),
                 QStringLiteral("show_item_preview"), QObject::tr("F7"), QStringLiteral("document-print-preview"), IconEye );
    addMenuItem( items, Actions::File_ToggleClipboardStoring, QObject::tr("&Toggle Clipboard Storing"),
                  QStringLiteral("toggle_clipboard_storing"), QObject::tr("Ctrl+Shift+X"), QStringLiteral(""), IconBan );
    addMenuItem( items, Actions::File_ProcessManager, QObject::tr("P&rocess Manager"),
                  QStringLiteral("process_manager"), QObject::tr("Ctrl+Shift+Z"), QStringLiteral("system-search"), IconGears );
    addMenuItem( items, Actions::File_Exit, QObject::tr("E&xit"), QStringLiteral("exit"), QObject::tr("Ctrl+Q"),
                  QStringLiteral("application-exit"), IconPowerOff );

    addMenuItem( items, Actions::Edit_SortSelectedItems, QObject::tr("&Sort Selected Items"),
                  QStringLiteral("sort_selected_items"), QObject::tr("Ctrl+Shift+S"),
                  QStringLiteral("view-sort-ascending"), IconArrowDownAZ );
    addMenuItem( items, Actions::Edit_ReverseSelectedItems, QObject::tr("&Reverse Selected Items"),
                  QStringLiteral("reverse_selected_items"), QObject::tr("Ctrl+Shift+R"),
                  QStringLiteral("view-sort-descending"), IconArrowUpAZ );
    addMenuItem( items, Actions::Edit_PasteItems, QObject::tr("&Paste Items"),
                  QStringLiteral("paste_selected_items"), QKeySequence::Paste, QStringLiteral("edit-paste"), IconPaste );
    addMenuItem( items, Actions::Edit_CopySelectedItems, QObject::tr("&Copy Selected Items"),
                  QStringLiteral("copy_selected_items"), QKeySequence::Copy, QStringLiteral("edit-copy"), IconCopy );
    addMenuItem( items, Actions::Edit_FindItems, QObject::tr("&Find"),
                  QStringLiteral("find_items"), QKeySequence::FindNext, QStringLiteral("edit-find"), IconMagnifyingGlass );

    addMenuItem(items, Actions::Editor_Save, QObject::tr("Save Item"),
                QStringLiteral("editor_save"), QObject::tr("F2", "Shortcut to save item editor changes"), QStringLiteral("document-save"), IconFloppyDisk);
    addMenuItem(items, Actions::Editor_Cancel, QObject::tr("Cancel Editing"),
                QStringLiteral("editor_cancel"), QObject::tr("Escape", "Shortcut to revert item editor changes"), QStringLiteral("document-revert"), IconTrash);
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
                  QStringLiteral("show_item_content"), QObject::tr("F4"), QStringLiteral("dialog-information"), IconCircleInfo );
    addMenuItem( items, Actions::Item_Remove, QObject::tr("&Remove"),
                  QStringLiteral("delete_item"),  shortcutToRemove(), QStringLiteral("list-remove"), IconTrash );
    addMenuItem( items, Actions::Item_Edit, QObject::tr("&Edit"), QStringLiteral("edit"), QObject::tr("F2"),
                  QStringLiteral("accessories-text-editor"), IconPenToSquare );
    addMenuItem( items, Actions::Item_EditNotes, QObject::tr("Edit &Notes"),
                  QStringLiteral("edit_notes"), QObject::tr("Shift+F2"), QStringLiteral("accessories-text-editor"), IconPenToSquare );
    addMenuItem( items, Actions::Item_EditWithEditor, QObject::tr("E&dit with Editor"),
                  QStringLiteral("editor"), QObject::tr("Ctrl+E"), QStringLiteral("accessories-text-editor"), IconPencil );
    addMenuItem( items, Actions::Item_Action, QObject::tr("&Action..."), QStringLiteral("system-run"), QObject::tr("F5"),
                  QStringLiteral("action"), IconBolt );

    addMenuItem( items, Actions::Item_MoveUp, QObject::tr("Move Up"),
                  QStringLiteral("move_up"),  QObject::tr("Ctrl+Up"), QStringLiteral("go-up"), IconAngleUp );
    addMenuItem( items, Actions::Item_MoveDown, QObject::tr("Move Down"),
                  QStringLiteral("move_down"),  QObject::tr("Ctrl+Down"), QStringLiteral("go-down"), IconAngleDown );
    addMenuItem( items, Actions::Item_MoveToTop, QObject::tr("Move to Top"),
                  QStringLiteral("move_to_top"),  QObject::tr("Ctrl+Home"), QStringLiteral("go-top"), IconAnglesUp );
    addMenuItem( items, Actions::Item_MoveToBottom, QObject::tr("Move to Bottom"),
                  QStringLiteral("move_to_bottom"),  QObject::tr("Ctrl+End"), QStringLiteral("go-bottom"), IconAnglesDown );

    addMenuItem( items, Actions::Tabs_NewTab, QObject::tr("&New Tab"),
                  QStringLiteral("new_tab"), QObject::tr("Ctrl+T"), QStringLiteral(":/images/tab_new") );
    addMenuItem( items, Actions::Tabs_RenameTab, QObject::tr("R&ename Tab"),
                  QStringLiteral("rename_tab"), QObject::tr("Ctrl+F2"), QStringLiteral(":/images/tab_rename") );
    addMenuItem( items, Actions::Tabs_RemoveTab, QObject::tr("Re&move Tab"),
                  QStringLiteral("remove_tab"), QObject::tr("Ctrl+W"), QStringLiteral(":/images/tab_remove") );
    addMenuItem( items, Actions::Tabs_ChangeTabIcon, QObject::tr("&Change Tab Icon"),
                  QStringLiteral("change_tab_icon"), QObject::tr("Ctrl+Shift+T"), QStringLiteral(":/images/tab_icon") );
    addMenuItem( items, Actions::Tabs_NextTab, QObject::tr("Ne&xt Tab"),
                  QStringLiteral("next_tab"), QObject::tr("Right", "Default shortcut to focus next tab"),
                  QStringLiteral("go-next"), IconArrowRight );
    addMenuItem( items, Actions::Tabs_PreviousTab, QObject::tr("&Previous Tab"),
                  QStringLiteral("previous_tab"), QObject::tr("Left", "Default shortcut to focus previous tab"),
                  QStringLiteral("go-previous"), IconArrowLeft );

    addMenuItem( items, Actions::Help_Help, QObject::tr("&Help"), QStringLiteral("help"), QKeySequence::HelpContents,
                  QStringLiteral("help-contents"), IconCircleQuestion );
    addMenuItem( items, Actions::Help_ShowLog, QObject::tr("&Show Log"), QStringLiteral("show-log"), QObject::tr("F12"),
                  QStringLiteral("help-about"), IconCircleExclamation );
    addMenuItem( items, Actions::Help_About, QObject::tr("&About"), QStringLiteral("about"), QKeySequence::WhatsThis,
                  QStringLiteral("help-about"), IconCircleInfo );

    addMenuItem( items, Actions::ItemMenu, QObject::tr("Open Item Context Menu"), QStringLiteral("item-menu"),
                 QObject::tr("Shift+F10", "Default shortcut to open item context menu"),
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
