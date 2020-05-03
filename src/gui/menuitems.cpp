/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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
    Q_ASSERT(items.size() == id && "Menu item index must be same as its ID.");
    Q_UNUSED(id);

    MenuItem item;
    item.text = text;
    item.settingsKey = settingsKey;
    item.defaultShortcut = shortcut;
    item.shortcuts = QList<QKeySequence>() << shortcut;
    item.iconName = iconName;
    item.iconId = iconId;
    items.append(item);
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
                  "document-new", IconFileAlt );
    addMenuItem( items, Actions::File_Import, QObject::tr("&Import..."), "import", QObject::tr("Ctrl+I"),
                  "document-open", IconFolderOpen );
    addMenuItem( items, Actions::File_Export, QObject::tr("&Export..."), "export", QKeySequence::Save,
                  "document-save", IconSave );
    addMenuItem( items, Actions::File_Preferences, QObject::tr("&Preferences..."), "preferences", QObject::tr("Ctrl+P"),
                  "preferences-other", IconWrench );
    addMenuItem( items, Actions::File_Commands,
#ifndef NO_GLOBAL_SHORTCUTS
                  QObject::tr("C&ommands/Global Shortcuts..."),
#else
                  QObject::tr("C&ommands..."),
#endif
                  "commands", QObject::tr("F6"), "system-run", IconCog );

    addMenuItem( items, Actions::File_ShowClipboardContent, QObject::tr("Show &Clipboard Content"),
                  "show_clipboard_content", QObject::tr("Ctrl+Shift+C"), "dialog-information", IconPaste );
    addMenuItem( items, Actions::File_ShowPreview, QObject::tr("&Show Preview"),
                 "show_item_preview", QObject::tr("F7"), "document-print-preview", IconEye );
    addMenuItem( items, Actions::File_ToggleClipboardStoring, QObject::tr("&Toggle Clipboard Storing"),
                  "toggle_clipboard_storing", QObject::tr("Ctrl+Shift+X"), "" );
    addMenuItem( items, Actions::File_ProcessManager, QObject::tr("P&rocess Manager"),
                  "process_manager", QObject::tr("Ctrl+Shift+Z"), "system-search", IconCogs );
    addMenuItem( items, Actions::File_Exit, QObject::tr("E&xit"), "exit", QObject::tr("Ctrl+Q"),
                  "application-exit", IconPowerOff );

    addMenuItem( items, Actions::Edit_SortSelectedItems, QObject::tr("&Sort Selected Items"),
                  "sort_selected_items", QObject::tr("Ctrl+Shift+S"),
                  "view-sort-ascending", IconSortAlphaDown );
    addMenuItem( items, Actions::Edit_ReverseSelectedItems, QObject::tr("&Reverse Selected Items"),
                  "reverse_selected_items", QObject::tr("Ctrl+Shift+R"),
                  "view-sort-descending", IconSortAlphaUp );
    addMenuItem( items, Actions::Edit_PasteItems, QObject::tr("&Paste Items"),
                  "paste_selected_items", QKeySequence::Paste, "edit-paste", IconPaste );
    addMenuItem( items, Actions::Edit_CopySelectedItems, QObject::tr("&Copy Selected Items"),
                  "copy_selected_items", QKeySequence::Copy, "edit-copy", IconCopy );
    addMenuItem( items, Actions::Edit_FindItems, QObject::tr("&Find"),
                  "find_items", QKeySequence::FindNext, "edit-find", IconSearch );

    addMenuItem( items, Actions::Item_MoveToClipboard,
                  QObject::tr("Move to &Clipboard",
                              "copies selected items to clipboard and moves them to top (depending on settings)"),
                  "move_to_clipboard", QKeySequence(), "clipboard", IconPaste );
    addMenuItem( items, Actions::Item_ShowContent, QObject::tr("&Show Content..."),
                  "show_item_content", QObject::tr("F4"), "dialog-information", IconInfoCircle );
    addMenuItem( items, Actions::Item_Remove, QObject::tr("&Remove"),
                  "delete_item",  shortcutToRemove(), "list-remove", IconTrash );
    addMenuItem( items, Actions::Item_Edit, QObject::tr("&Edit"), "edit", QObject::tr("F2"),
                  "accessories-text-editor", IconEdit );
    addMenuItem( items, Actions::Item_EditNotes, QObject::tr("Edit &Notes"),
                  "edit_notes", QObject::tr("Shift+F2"), "accessories-text-editor", IconPenSquare );
    addMenuItem( items, Actions::Item_EditWithEditor, QObject::tr("E&dit with editor"),
                  "editor", QObject::tr("Ctrl+E"), "accessories-text-editor", IconPencilAlt );
    addMenuItem( items, Actions::Item_Action, QObject::tr("&Action..."), "system-run", QObject::tr("F5"),
                  "action", IconBolt );

    addMenuItem( items, Actions::Item_MoveUp, QObject::tr("Move Up"),
                  "move_up",  QObject::tr("Ctrl+Up"), "go-up", IconAngleUp );
    addMenuItem( items, Actions::Item_MoveDown, QObject::tr("Move Down"),
                  "move_down",  QObject::tr("Ctrl+Down"), "go-down", IconAngleDown );
    addMenuItem( items, Actions::Item_MoveToTop, QObject::tr("Move to Top"),
                  "move_to_top",  QObject::tr("Ctrl+Home"), "go-top", IconAngleDoubleUp );
    addMenuItem( items, Actions::Item_MoveToBottom, QObject::tr("Move to Bottom"),
                  "move_to_bottom",  QObject::tr("Ctrl+End"), "go-bottom", IconAngleDoubleDown );

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
                  "help-contents", IconQuestionCircle );
    addMenuItem( items, Actions::Help_ShowLog, QObject::tr("&Show Log"), "show-log", QObject::tr("F12"),
                  "help-about", IconExclamationCircle );
    addMenuItem( items, Actions::Help_About, QObject::tr("&About"), "about", QKeySequence::WhatsThis,
                  "help-about", IconInfoCircle );

    addMenuItem( items, Actions::ItemMenu, QObject::tr("Open Item Context Menu"), "item-menu",
                 QObject::tr("Shift+F10", "Default shortcut to open item context menu"),
                 "", IconListAlt );

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
