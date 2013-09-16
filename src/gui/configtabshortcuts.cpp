/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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
#include "gui/configurationmanager.h"
#include "gui/iconfactory.h"

namespace {

const QIcon iconAction() { return getIcon("action", IconCog); }
const QIcon iconClipboard() { return getIcon("clipboard", IconPaste); }
const QIcon iconCopy() { return getIcon("edit-copy", IconCopy); }
const QIcon iconEditExternal() { return getIcon("accessories-text-editor", IconPencil); }
const QIcon iconEditNotes() { return getIcon("accessories-text-editor", IconEditSign); }
const QIcon iconEdit() { return getIcon("accessories-text-editor", IconEdit); }
const QIcon iconExit() { return getIcon("application-exit", IconOff); }
const QIcon iconHelp() { return getIcon("help-about", IconQuestionSign); }
const QIcon iconNew() { return getIcon("document-new", IconFile); }
const QIcon iconNextToClipboard() { return getIcon("go-down", IconArrowDown); }
const QIcon iconOpen() { return getIcon("document-open", IconFolderOpen); }
const QIcon iconPaste() { return getIcon("edit-paste", IconPaste); }
const QIcon iconPreferences() { return getIcon("preferences-other", IconWrench); }
const QIcon iconPreviousToClipboard() { return getIcon("go-up", IconArrowUp); }
const QIcon iconRemove() { return getIcon("list-remove", IconRemove); }
const QIcon iconReverse() { return getIcon("view-sort-descending", IconSortByAlphabetAlt); }
const QIcon iconSave() { return getIcon("document-save", IconSave); }
const QIcon iconShowContent() { return getIcon("dialog-information", IconInfoSign); }
const QIcon iconSort() { return getIcon("view-sort-ascending", IconSortByAlphabet); }

const QIcon &iconTabNew() { return getIconFromResources("tab_new"); }
const QIcon &iconTabRemove() { return getIconFromResources("tab_remove"); }
const QIcon &iconTabRename() { return getIconFromResources("tab_rename"); }

} // namespace

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

void ConfigTabShortcuts::loadGlobalShortcuts(QSettings &settings)
{
#ifdef NO_GLOBAL_SHORTCUTS
    Q_UNUSED(settings);
#else
    ui->shortcutsWidgetSystemWide->loadShortcuts(settings);
#endif
}

void ConfigTabShortcuts::saveGlobalShortcuts(QSettings &settings) const
{
#ifdef NO_GLOBAL_SHORTCUTS
    Q_UNUSED(settings);
#else
    ui->shortcutsWidgetSystemWide->saveShortcuts(settings);
#endif
}

QAction *ConfigTabShortcuts::action(Actions::Id id, QWidget *parent, Qt::ShortcutContext context)
{
    return ui->shortcutsWidgetGeneral->hasAction(id)
            ? ui->shortcutsWidgetGeneral->action(id, parent, context)
            : ui->shortcutsWidgetSystemWide->action(id, parent, context);
}

QList<QKeySequence> ConfigTabShortcuts::shortcuts(Actions::Id id) const
{
    return ui->shortcutsWidgetGeneral->hasAction(id)
            ? ui->shortcutsWidgetGeneral->shortcuts(id)
            : ui->shortcutsWidgetSystemWide->shortcuts(id);
}

void ConfigTabShortcuts::updateIcons()
{
    ShortcutsWidget *w = ui->shortcutsWidgetGeneral;

    w->updateIcons( Actions::File_New, iconNew() );
    w->updateIcons( Actions::File_ImportTab, iconOpen() );
    w->updateIcons( Actions::File_ExportTab, iconSave() );
    w->updateIcons( Actions::File_Preferences, iconPreferences() );
    w->updateIcons( Actions::File_ShowClipboardContent, iconClipboard() );
    w->updateIcons( Actions::File_ToggleClipboardStoring );
    w->updateIcons( Actions::File_Exit, iconExit() );

    w->updateIcons( Actions::Edit_SortSelectedItems, iconSort() );
    w->updateIcons( Actions::Edit_ReverseSelectedItems, iconReverse() );
    w->updateIcons( Actions::Edit_PasteItems, iconPaste() );
    w->updateIcons( Actions::Edit_CopySelectedItems, iconCopy() );

    w->updateIcons( Actions::Item_MoveToClipboard, iconClipboard() );
    w->updateIcons( Actions::Item_ShowContent, iconShowContent() );
    w->updateIcons( Actions::Item_Remove, iconRemove() );
    w->updateIcons( Actions::Item_Edit, iconEdit() );
    w->updateIcons( Actions::Item_EditNotes, iconEditNotes() );
    w->updateIcons( Actions::Item_EditWithEditor, iconEditExternal() );
    w->updateIcons( Actions::Item_Action, iconAction() );
    w->updateIcons( Actions::Item_NextToClipboard, iconNextToClipboard() );
    w->updateIcons( Actions::Item_PreviousToClipboard, iconPreviousToClipboard() );

    w->updateIcons( Actions::Tabs_NewTab, iconTabNew() );
    w->updateIcons( Actions::Tabs_RenameTab, iconTabRename() );
    w->updateIcons( Actions::Tabs_RemoveTab, iconTabRemove() );

    w->updateIcons( Actions::Help_Help, iconHelp() );

#ifndef NO_GLOBAL_SHORTCUTS
    w = ui->shortcutsWidgetSystemWide;

    w->updateIcons( Actions::Global_ToggleMainWindow );
    w->updateIcons( Actions::Global_ShowTray );
    w->updateIcons( Actions::Global_EditClipboard );
    w->updateIcons( Actions::Global_EditFirstItem );
    w->updateIcons( Actions::Global_CopySecondItem );
    w->updateIcons( Actions::Global_ShowActionDialog );
    w->updateIcons( Actions::Global_CreateItem );
    w->updateIcons( Actions::Global_CopyNextItem );
    w->updateIcons( Actions::Global_CopyPreviousItem );
    w->updateIcons( Actions::Global_PasteAsPlainText );
    w->updateIcons( Actions::Global_DisableClipboardStoring );
    w->updateIcons( Actions::Global_EnableClipboardStoring );
    w->updateIcons( Actions::Global_PasteAndCopyNext );
    w->updateIcons( Actions::Global_PasteAndCopyPrevious );
#endif
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
    w->addAction( Actions::File_ShowClipboardContent, tr("Show &Clipboard Content"), "show_clipboard_content", tr("Ctrl+Shift+C") );
    w->addAction( Actions::File_ToggleClipboardStoring, tr("&Enable Clipboard Storing"), "toggle_clipboard_storing", tr("Ctrl+Shift+X") );
    w->addAction( Actions::File_Exit, tr("E&xit"), "exit", tr("Ctrl+Q") );

    w->addAction( Actions::Edit_SortSelectedItems, tr("&Sort Selected Items"), "sort_selected_items", tr("Ctrl+Shift+S") );
    w->addAction( Actions::Edit_ReverseSelectedItems, tr("&Reverse Selected Items"), "reverse_selected_items", tr("Ctrl+Shift+R") );
    w->addAction( Actions::Edit_PasteItems, tr("&Paste Items"), "paste_selected_items", QKeySequence::Paste );
    w->addAction( Actions::Edit_CopySelectedItems, tr("&Copy Selected Items"), "copy_selected_items", QKeySequence::Copy );

    w->addAction( Actions::Item_MoveToClipboard, tr("Move to &Clipboard"), "move_to_clipboard" );
    w->addAction( Actions::Item_ShowContent, tr("&Show Content..."), "show_item_content", tr("F4") );
    w->addAction( Actions::Item_Remove, tr("&Remove"), "delete_item", tr("Delete") );
    w->addAction( Actions::Item_Edit, tr("&Edit"), "edit", tr("F2") );
    w->addAction( Actions::Item_EditNotes, tr("&Edit Notes"), "edit_notes", tr("Shift+F2") );
    w->addAction( Actions::Item_EditWithEditor, tr("E&dit with editor"), "editor", tr("Ctrl+E") );
    w->addAction( Actions::Item_Action, tr("&Action..."), "action", tr("F5") );
    w->addAction( Actions::Item_NextToClipboard, tr("&Next to Clipboard"), "next_to_clipboard", tr("Ctrl+Shift+N") );
    w->addAction( Actions::Item_PreviousToClipboard, tr("&Previous to Clipboard"), "previous_to_clipboard", tr("Ctrl+Shift+P") );

    w->addAction( Actions::Tabs_NewTab, tr("&New tab"), "new_tab", tr("Ctrl+T") );
    w->addAction( Actions::Tabs_RenameTab, tr("Re&name tab"), "rename_tab", tr("Ctrl+F2") );
    w->addAction( Actions::Tabs_RemoveTab, tr("Re&move tab"), "remove_tab", tr("Ctrl+W") );

    w->addAction( Actions::Help_Help, tr("&Help"), "help", QKeySequence::HelpContents );

#ifdef NO_GLOBAL_SHORTCUTS
    ui->shortcutsWidgetGeneral->setParent(this);
    layout()->addWidget( ui->shortcutsWidgetGeneral );
    ui->tabWidget->hide();
#else
    w = ui->shortcutsWidgetSystemWide;

    w->addAction( Actions::Global_ToggleMainWindow, tr("Sh&ow/hide main window"), "toggle_shortcut" );
    w->addAction( Actions::Global_ShowTray, tr("Show the tray &menu"), "menu_shortcut" );
    w->addAction( Actions::Global_EditClipboard, tr("&Edit clipboard"), "edit_clipboard_shortcut" );
    w->addAction( Actions::Global_EditFirstItem, tr("Edit &first item"), "edit_shortcut" );
    w->addAction( Actions::Global_CopySecondItem, tr("Copy &second item"), "second_shortcut" );
    w->addAction( Actions::Global_ShowActionDialog, tr("Show &action dialog"), "show_action_dialog" );
    w->addAction( Actions::Global_CreateItem, tr("Create &new item"), "new_item_shortcut" );
    w->addAction( Actions::Global_CopyNextItem, tr("Copy n&ext item"), "next_item_shortcut" );
    w->addAction( Actions::Global_CopyPreviousItem, tr("Copy &previous item"), "previous_item_shortcut" );
    w->addAction( Actions::Global_PasteAsPlainText, tr("Paste as pla&in text"), "paste_as_plain_text" );
    w->addAction( Actions::Global_DisableClipboardStoring, tr("Disable clipboard storing"), "disable_monitoring_shortcut" );
    w->addAction( Actions::Global_EnableClipboardStoring, tr("Enable clipboard storing"), "enable_monitoring_shortcut" );
    w->addAction( Actions::Global_PasteAndCopyNext, tr("Paste and copy next"), "paste_and_copy_next_shortcut" );
    w->addAction( Actions::Global_PasteAndCopyPrevious, tr("Paste and copy previous"), "paste_and_copy_previous_shortcut" );
#endif
}
