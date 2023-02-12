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
    addMenuItem( items, Actions::File_New, QObject::tr("&New Item"), "new", QKeySequence::New,
                  "document-new", IconFileLines );
    addMenuItem( items, Actions::File_Import, QObject::tr("&Import..."), "import", QObject::tr("Ctrl+I"),
                  "document-open", IconFolderOpen );
    addMenuItem( items, Actions::File_Export, QObject::tr("&Export..."), "export", QKeySequence::Save,
                  "document-save", IconFloppyDisk );
    addMenuItem( items, Actions::File_Preferences, QObject::tr("&Preferences..."), "preferences", QObject::tr("Ctrl+P"),
                  "preferences-other", IconWrench );
    addMenuItem( items, Actions::File_Commands,
#ifndef NO_GLOBAL_SHORTCUTS
                  QObject::tr("C&ommands/Global Shortcuts..."),
#else
                  QObject::tr("C&ommands..."),
#endif
                  "commands", QObject::tr("F6"), "system-run", IconGear );

    addMenuItem( items, Actions::File_ShowClipboardContent, QObject::tr("Show &Clipboard Content"),
                  "show_clipboard_content", QObject::tr("Ctrl+Shift+C"), "dialog-information", IconPaste );
    addMenuItem( items, Actions::File_ShowPreview, QObject::tr("&Show Preview"),
                 "show_item_preview", QObject::tr("F7"), "document-print-preview", IconEye );
    addMenuItem( items, Actions::File_ToggleClipboardStoring, QObject::tr("&Toggle Clipboard Storing"),
                  "toggle_clipboard_storing", QObject::tr("Ctrl+Shift+X"), "", IconBan );
    addMenuItem( items, Actions::File_ProcessManager, QObject::tr("P&rocess Manager"),
                  "process_manager", QObject::tr("Ctrl+Shift+Z"), "system-search", IconGears );
    addMenuItem( items, Actions::File_Exit, QObject::tr("E&xit"), "exit", QObject::tr("Ctrl+Q"),
                  "application-exit", IconPowerOff );

    addMenuItem( items, Actions::Edit_SortSelectedItems, QObject::tr("&Sort Selected Items"),
                  "sort_selected_items", QObject::tr("Ctrl+Shift+S"),
                  "view-sort-ascending", IconArrowDownAZ );
    addMenuItem( items, Actions::Edit_ReverseSelectedItems, QObject::tr("&Reverse Selected Items"),
                  "reverse_selected_items", QObject::tr("Ctrl+Shift+R"),
                  "view-sort-descending", IconArrowUpAZ );
    addMenuItem( items, Actions::Edit_PasteItems, QObject::tr("&Paste Items"),
                  "paste_selected_items", QKeySequence::Paste, "edit-paste", IconPaste );
    addMenuItem( items, Actions::Edit_CopySelectedItems, QObject::tr("&Copy Selected Items"),
                  "copy_selected_items", QKeySequence::Copy, "edit-copy", IconCopy );
    addMenuItem( items, Actions::Edit_FindItems, QObject::tr("&Find"),
                  "find_items", QKeySequence::FindNext, "edit-find", IconMagnifyingGlass );

    addMenuItem(items, Actions::Editor_Save, QObject::tr("Save Item"),
                "editor_save", QObject::tr("F2", "Shortcut to save item editor changes"), "document-save", IconFloppyDisk);
    addMenuItem(items, Actions::Editor_Cancel, QObject::tr("Cancel Editing"),
                "editor_cancel", QObject::tr("Escape", "Shortcut to revert item editor changes"), "document-revert", IconTrash);
    addMenuItem(items, Actions::Editor_Undo, QObject::tr("Undo"),
                "editor_undo", QKeySequence::Undo, "edit-undo", IconRotateLeft);
    addMenuItem(items, Actions::Editor_Redo, QObject::tr("Redo"),
                "editor_redo", QKeySequence::Redo, "edit-redo", IconRotateRight);
    addMenuItem(items, Actions::Editor_Font, QObject::tr("Font"),
                "editor_font", QKeySequence(), "preferences-desktop-font", IconFont);
    addMenuItem(items, Actions::Editor_Bold, QObject::tr("Bold"),
                "editor_bold", QKeySequence::Bold, "format-text-bold", IconBold);
    addMenuItem(items, Actions::Editor_Italic, QObject::tr("Italic"),
                "editor_italic", QKeySequence::Italic, "format-text-italic", IconItalic);
    addMenuItem(items, Actions::Editor_Underline, QObject::tr("Underline"),
                "editor_underline", QKeySequence::Underline, "format-text-underline", IconUnderline);
    addMenuItem(items, Actions::Editor_Strikethrough, QObject::tr("Strikethrough"),
                "editor_strikethrough", QKeySequence(), "format-text-strikethrough", IconStrikethrough);
    addMenuItem(items, Actions::Editor_Foreground, QObject::tr("Foreground"),
                "editor_foreground", QKeySequence(), "", IconPaintbrush);
    addMenuItem(items, Actions::Editor_Background, QObject::tr("Background"),
                "editor_background", QKeySequence(), "", IconSquare);
    addMenuItem(items, Actions::Editor_EraseStyle, QObject::tr("Erase Style"),
                "editor_erase_style", QKeySequence(), "", IconEraser);
    addMenuItem(items, Actions::Editor_Search, QObject::tr("Search"),
                "editor_search", QKeySequence::Find, "edit-find", IconMagnifyingGlass);

    addMenuItem( items, Actions::Item_MoveToClipboard,
                  QObject::tr("A&ctivate Items",
                              "copies selected items to clipboard and moves them to top (depending on settings)"),
                  "move_to_clipboard", QKeySequence(), "clipboard", IconPaste );
    addMenuItem( items, Actions::Item_ShowContent, QObject::tr("&Show Content..."),
                  "show_item_content", QObject::tr("F4"), "dialog-information", IconCircleInfo );
    addMenuItem( items, Actions::Item_Remove, QObject::tr("&Remove"),
                  "delete_item",  shortcutToRemove(), "list-remove", IconTrash );
    addMenuItem( items, Actions::Item_Edit, QObject::tr("&Edit"), "edit", QObject::tr("F2"),
                  "accessories-text-editor", IconPenToSquare );
    addMenuItem( items, Actions::Item_EditNotes, QObject::tr("Edit &Notes"),
                  "edit_notes", QObject::tr("Shift+F2"), "accessories-text-editor", IconPenToSquare );
    addMenuItem( items, Actions::Item_EditWithEditor, QObject::tr("E&dit with Editor"),
                  "editor", QObject::tr("Ctrl+E"), "accessories-text-editor", IconPencil );
    addMenuItem( items, Actions::Item_Action, QObject::tr("&Action..."), "system-run", QObject::tr("F5"),
                  "action", IconBolt );

    addMenuItem( items, Actions::Item_MoveUp, QObject::tr("Move Up"),
                  "move_up",  QObject::tr("Ctrl+Up"), "go-up", IconAngleUp );
    addMenuItem( items, Actions::Item_MoveDown, QObject::tr("Move Down"),
                  "move_down",  QObject::tr("Ctrl+Down"), "go-down", IconAngleDown );
    addMenuItem( items, Actions::Item_MoveToTop, QObject::tr("Move to Top"),
                  "move_to_top",  QObject::tr("Ctrl+Home"), "go-top", IconAnglesUp );
    addMenuItem( items, Actions::Item_MoveToBottom, QObject::tr("Move to Bottom"),
                  "move_to_bottom",  QObject::tr("Ctrl+End"), "go-bottom", IconAnglesDown );

    addMenuItem( items, Actions::Tabs_NewTab, QObject::tr("&New Tab"),
                  "new_tab", QObject::tr("Ctrl+T"), ":/images/tab_new" );
    addMenuItem( items, Actions::Tabs_RenameTab, QObject::tr("R&ename Tab"),
                  "rename_tab", QObject::tr("Ctrl+F2"), ":/images/tab_rename" );
    addMenuItem( items, Actions::Tabs_RemoveTab, QObject::tr("Re&move Tab"),
                  "remove_tab", QObject::tr("Ctrl+W"), ":/images/tab_remove" );
    addMenuItem( items, Actions::Tabs_ChangeTabIcon, QObject::tr("&Change Tab Icon"),
                  "change_tab_icon", QObject::tr("Ctrl+Shift+T"), ":/images/tab_icon" );
    addMenuItem( items, Actions::Tabs_NextTab, QObject::tr("Ne&xt Tab"),
                  "next_tab", QObject::tr("Right", "Default shortcut to focus next tab"),
                  "go-next", IconArrowRight );
    addMenuItem( items, Actions::Tabs_PreviousTab, QObject::tr("&Previous Tab"),
                  "previous_tab", QObject::tr("Left", "Default shortcut to focus previous tab"),
                  "go-previous", IconArrowLeft );

    addMenuItem( items, Actions::Help_Help, QObject::tr("&Help"), "help", QKeySequence::HelpContents,
                  "help-contents", IconCircleQuestion );
    addMenuItem( items, Actions::Help_ShowLog, QObject::tr("&Show Log"), "show-log", QObject::tr("F12"),
                  "help-about", IconCircleExclamation );
    addMenuItem( items, Actions::Help_About, QObject::tr("&About"), "about", QKeySequence::WhatsThis,
                  "help-about", IconCircleInfo );

    addMenuItem( items, Actions::ItemMenu, QObject::tr("Open Item Context Menu"), "item-menu",
                 QObject::tr("Shift+F10", "Default shortcut to open item context menu"),
                 "", IconRectangleList );

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
