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
#ifndef MENUITEMS_H
#define MENUITEMS_H

#include <QList>
#include <QKeySequence>
#include <QString>
#include <QSettings>

#include <array>

namespace Actions {

enum Id {
    File_New,
    File_Import,
    File_Export,
    File_Preferences,
    File_Commands,
    File_ShowClipboardContent,
    File_ShowPreview,
    File_ToggleClipboardStoring,
    File_ProcessManager,
    File_Exit,

    Edit_SortSelectedItems,
    Edit_ReverseSelectedItems,
    Edit_PasteItems,
    Edit_CopySelectedItems,
    Edit_FindItems,

    Editor_Save,
    Editor_Cancel,
    Editor_Undo,
    Editor_Redo,
    Editor_Font,
    Editor_Bold,
    Editor_Italic,
    Editor_Underline,
    Editor_Strikethrough,
    Editor_Foreground,
    Editor_Background,
    Editor_EraseStyle,
    Editor_Search,

    Item_MoveToClipboard,
    Item_ShowContent,
    Item_Remove,
    Item_Edit,
    Item_EditNotes,
    Item_EditWithEditor,
    Item_Action,

    Item_MoveUp,
    Item_MoveDown,
    Item_MoveToTop,
    Item_MoveToBottom,

    Tabs_NewTab,
    Tabs_RenameTab,
    Tabs_RemoveTab,
    Tabs_ChangeTabIcon,
    Tabs_NextTab,
    Tabs_PreviousTab,

    Help_Help,
    Help_ShowLog,
    Help_About,

    ItemMenu,

    Count
};

} // Actions

struct MenuItem {
    QString iconName;
    ushort iconId = 0;
    QString text;
    QString settingsKey;
    QKeySequence defaultShortcut;
    QList<QKeySequence> shortcuts;
};

using MenuItems = std::array<MenuItem, Actions::Count>;

MenuItems menuItems();

void loadShortcuts(MenuItems *items, const QSettings &settings);

#endif // MENUITEMS_H
