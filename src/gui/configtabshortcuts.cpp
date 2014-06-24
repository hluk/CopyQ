/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

#include "gui/configtabshortcuts.h"
#include "ui_configtabshortcuts.h"

#include "common/client_server.h"
#include "common/common.h"
#include "gui/configurationmanager.h"
#include "gui/icons.h"

ConfigTabShortcuts::ConfigTabShortcuts(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ConfigTabShortcuts)
{
    ui->setupUi(this);
    initShortcuts();
}

ConfigTabShortcuts::~ConfigTabShortcuts()
{
    delete ui;
}

void ConfigTabShortcuts::loadShortcuts(QSettings &settings)
{
    ui->shortcutsWidgetGeneral->loadShortcuts(settings);
}

void ConfigTabShortcuts::saveShortcuts(QSettings &settings) const
{
    ui->shortcutsWidgetGeneral->saveShortcuts(settings);
}

QAction *ConfigTabShortcuts::action(Actions::Id id, QWidget *parent, Qt::ShortcutContext context)
{
    return ui->shortcutsWidgetGeneral->action(id, parent, context);
}

QList<QKeySequence> ConfigTabShortcuts::shortcuts(Actions::Id id) const
{
    return ui->shortcutsWidgetGeneral->shortcuts(id);
}

void ConfigTabShortcuts::updateIcons()
{
    ShortcutsWidget *w = ui->shortcutsWidgetGeneral;

    w->updateIcons( Actions::File_New, "document-new", IconFileAlt );
    w->updateIcons( Actions::File_ImportTab, "document-open", IconFolderOpen );
    w->updateIcons( Actions::File_ExportTab, "document-save", IconSave );
    w->updateIcons( Actions::File_Preferences, "preferences-other", IconWrench );
    w->updateIcons( Actions::File_Commands, "action", IconCog );
    w->updateIcons( Actions::File_ShowClipboardContent, "clipboard", IconPaste );
    w->updateIcons( Actions::File_ToggleClipboardStoring );
    w->updateIcons( Actions::File_ProcessManager, "", IconCogs );
    w->updateIcons( Actions::File_Exit, "application-exit", IconOff );

    w->updateIcons( Actions::Edit_SortSelectedItems, "view-sort-ascending", IconSortByAlphabet );
    w->updateIcons( Actions::Edit_ReverseSelectedItems, "view-sort-descending", IconSortByAlphabetAlt );
    w->updateIcons( Actions::Edit_PasteItems, "edit-paste", IconPaste );
    w->updateIcons( Actions::Edit_CopySelectedItems, "edit-copy", IconCopy );

    w->updateIcons( Actions::Item_MoveToClipboard, "clipboard", IconPaste );
    w->updateIcons( Actions::Item_ShowContent, "dialog-information", IconInfoSign );
    w->updateIcons( Actions::Item_Remove, "list-remove", IconRemove );
    w->updateIcons( Actions::Item_Edit, "accessories-text-editor", IconEdit );
    w->updateIcons( Actions::Item_EditNotes, "accessories-text-editor", IconEditSign );
    w->updateIcons( Actions::Item_EditWithEditor, "accessories-text-editor", IconPencil );
    w->updateIcons( Actions::Item_Action, "action", IconCog );
    w->updateIcons( Actions::Item_NextToClipboard, "go-down", IconArrowDown );
    w->updateIcons( Actions::Item_PreviousToClipboard, "go-up", IconArrowUp );

    w->updateIcons( Actions::Tabs_NewTab, "tab_new" );
    w->updateIcons( Actions::Tabs_RenameTab, "tab_rename" );
    w->updateIcons( Actions::Tabs_RemoveTab, "tab_remove" );

    w->updateIcons( Actions::Help_Help, "help-about", IconQuestionSign );
}

void ConfigTabShortcuts::setDisabledShortcuts(const QList<QKeySequence> &shortcuts)
{
    ui->shortcutsWidgetGeneral->setDisabledShortcuts(shortcuts);
}

void ConfigTabShortcuts::initShortcuts()
{
    ShortcutsWidget *w = ui->shortcutsWidgetGeneral;

    w->addAction( Actions::File_New, tr("&New Item"), "new", QKeySequence::New );
    w->addAction( Actions::File_ImportTab, tr("&Import Tab..."), "import_tab", tr("Ctrl+I") );
    w->addAction( Actions::File_ExportTab, tr("&Export Tab..."), "export_tab", QKeySequence::Save );
    w->addAction( Actions::File_Preferences, tr("&Preferences..."), "preferences", tr("Ctrl+P") );
    w->addAction( Actions::File_Commands, tr("C&ommands..."), "commands", tr("F6") );
    w->addAction( Actions::File_ShowClipboardContent, tr("Show &Clipboard Content"), "show_clipboard_content", tr("Ctrl+Shift+C") );
    w->addAction( Actions::File_ToggleClipboardStoring, tr("&Toggle Clipboard Storing"), "toggle_clipboard_storing", tr("Ctrl+Shift+X") );
    w->addAction( Actions::File_ProcessManager, tr("P&rocess Manager"), "process_manager", tr("Ctrl+Shift+Z") );
    w->addAction( Actions::File_Exit, tr("E&xit"), "exit", tr("Ctrl+Q") );

    w->addAction( Actions::Edit_SortSelectedItems, tr("&Sort Selected Items"), "sort_selected_items", tr("Ctrl+Shift+S") );
    w->addAction( Actions::Edit_ReverseSelectedItems, tr("&Reverse Selected Items"), "reverse_selected_items", tr("Ctrl+Shift+R") );
    w->addAction( Actions::Edit_PasteItems, tr("&Paste Items"), "paste_selected_items", QKeySequence::Paste );
    w->addAction( Actions::Edit_CopySelectedItems, tr("&Copy Selected Items"), "copy_selected_items", QKeySequence::Copy );

    w->addAction( Actions::Item_MoveToClipboard, tr("Move to &Clipboard"), "move_to_clipboard" );
    w->addAction( Actions::Item_ShowContent, tr("&Show Content..."), "show_item_content", tr("F4") );
    w->addAction( Actions::Item_Edit, tr("&Edit"), "edit", tr("F2") );
    w->addAction( Actions::Item_EditNotes, tr("Edit &Notes"), "edit_notes", tr("Shift+F2") );
    w->addAction( Actions::Item_EditWithEditor, tr("E&dit with editor"), "editor", tr("Ctrl+E") );
    w->addAction( Actions::Item_Action, tr("&Action..."), "action", tr("F5") );
    w->addAction( Actions::Item_NextToClipboard, tr("Ne&xt to Clipboard"), "next_to_clipboard", tr("Ctrl+Shift+N") );
    w->addAction( Actions::Item_PreviousToClipboard, tr("&Previous to Clipboard"), "previous_to_clipboard", tr("Ctrl+Shift+P") );

    w->addAction( Actions::Item_Remove, tr("&Remove"), "delete_item",  shortcutToRemove() );

    w->addAction( Actions::Tabs_NewTab, tr("&New Tab"), "new_tab", tr("Ctrl+T") );
    w->addAction( Actions::Tabs_RenameTab, tr("R&ename Tab"), "rename_tab", tr("Ctrl+F2") );
    w->addAction( Actions::Tabs_RemoveTab, tr("Re&move Tab"), "remove_tab", tr("Ctrl+W") );
    w->addAction( Actions::Tabs_ChangeTabIcon, tr("&Change Tab Icon"), "change_tab_icon", tr("Ctrl+Shift+T") );

    w->addAction( Actions::Help_Help, tr("&Help"), "help", QKeySequence::HelpContents );
}
