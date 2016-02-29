/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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

    connect( ui->pushButtonOpenCommandDialog, SIGNAL(clicked()),
             this, SIGNAL(openCommandDialogRequest()) );
}

ConfigTabShortcuts::~ConfigTabShortcuts()
{
    delete ui;
}

void ConfigTabShortcuts::loadShortcuts(QSettings &settings)
{
    ui->shortcutsWidgetGeneral->loadShortcuts(settings);

    const QIcon iconCommandDialog = getIcon("system-run", IconCog);
    ui->pushButtonOpenCommandDialog->setIcon(iconCommandDialog);
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

void ConfigTabShortcuts::setDisabledShortcuts(const QList<QKeySequence> &shortcuts)
{
    ui->shortcutsWidgetGeneral->setDisabledShortcuts(shortcuts);
}

const QList<QKeySequence> &ConfigTabShortcuts::disabledShortcuts() const
{
    return ui->shortcutsWidgetGeneral->disabledShortcuts();
}

void ConfigTabShortcuts::initShortcuts()
{
    ShortcutsWidget *w = ui->shortcutsWidgetGeneral;

    w->addAction( Actions::File_New, tr("&New Item"), "new", QKeySequence::New,
                  "document-new", IconFileAlt );
    w->addAction( Actions::File_ImportTab, tr("&Import Tab..."), "import_tab", tr("Ctrl+I"),
                  "document-open", IconFolderOpen );
    w->addAction( Actions::File_ExportTab, tr("&Export Tab..."), "export_tab", QKeySequence::Save,
                  "document-save", IconSave );
    w->addAction( Actions::File_Preferences, tr("&Preferences..."), "preferences", tr("Ctrl+P"),
                  "preferences-other", IconWrench );
    w->addAction( Actions::File_Commands,
#ifndef NO_GLOBAL_SHORTCUTS
                  tr("C&ommands/Global Shortcuts..."),
#else
                  tr("C&ommands..."),
#endif
                  "commands", tr("F6"), "system-run", IconCog );

    w->addAction( Actions::File_ShowClipboardContent, tr("Show &Clipboard Content"),
                  "show_clipboard_content", tr("Ctrl+Shift+C"), "dialog-information", IconPaste );
    w->addAction( Actions::File_ToggleClipboardStoring, tr("&Toggle Clipboard Storing"),
                  "toggle_clipboard_storing", tr("Ctrl+Shift+X"), "" );
    w->addAction( Actions::File_ProcessManager, tr("P&rocess Manager"),
                  "process_manager", tr("Ctrl+Shift+Z"), "system-search", IconCogs );
    w->addAction( Actions::File_Exit, tr("E&xit"), "exit", tr("Ctrl+Q"),
                  "application-exit", IconOff );

    w->addAction( Actions::Edit_SortSelectedItems, tr("&Sort Selected Items"),
                  "sort_selected_items", tr("Ctrl+Shift+S"),
                  "view-sort-ascending", IconSortByAlphabet );
    w->addAction( Actions::Edit_ReverseSelectedItems, tr("&Reverse Selected Items"),
                  "reverse_selected_items", tr("Ctrl+Shift+R"),
                  "view-sort-descending", IconSortByAlphabetAlt );
    w->addAction( Actions::Edit_PasteItems, tr("&Paste Items"),
                  "paste_selected_items", QKeySequence::Paste, "edit-paste", IconPaste );
    w->addAction( Actions::Edit_CopySelectedItems, tr("&Copy Selected Items"),
                  "copy_selected_items", QKeySequence::Copy, "edit-copy", IconCopy );
    w->addAction( Actions::Edit_FindItems, tr("&Find"),
                  "find_items", QKeySequence::FindNext, "edit-find", IconSearch );

    w->addAction( Actions::Item_MoveToClipboard, tr("Move to &Clipboard"),
                  "move_to_clipboard", QKeySequence(), "clipboard", IconPaste );
    w->addAction( Actions::Item_ShowContent, tr("&Show Content..."),
                  "show_item_content", tr("F4"), "dialog-information", IconInfoSign );
    w->addAction( Actions::Item_Edit, tr("&Edit"), "edit", tr("F2"),
                  "accessories-text-editor", IconEdit );
    w->addAction( Actions::Item_EditNotes, tr("Edit &Notes"),
                  "edit_notes", tr("Shift+F2"), "accessories-text-editor", IconEditSign );
    w->addAction( Actions::Item_EditWithEditor, tr("E&dit with editor"),
                  "editor", tr("Ctrl+E"), "accessories-text-editor", IconPencil );
    w->addAction( Actions::Item_Action, tr("&Action..."), "system-run", tr("F5"),
                  "action", IconCog );
    w->addAction( Actions::Item_NextFormat, tr("Next Format"), "format-next", tr("Ctrl+Right"),
                  "go-next", IconArrowRight );
    w->addAction( Actions::Item_PreviousFormat, tr("Previous Format"), "format-previous", tr("Ctrl+Left"),
                  "go-previous", IconArrowLeft );

    w->addAction( Actions::Item_Remove, tr("&Remove"),
                  "delete_item",  shortcutToRemove(), "list-remove", IconRemove );

    w->addAction( Actions::Item_MoveUp, tr("Move Up"),
                  "move_up",  tr("Ctrl+Up"), "go-up", IconAngleUp );
    w->addAction( Actions::Item_MoveDown, tr("Move Down"),
                  "move_down",  tr("Ctrl+Down"), "go-down", IconAngleDown );
    w->addAction( Actions::Item_MoveToTop, tr("Move to Top"),
                  "move_to_top",  tr("Ctrl+Home"), "go-top", IconAngleDoubleUp );
    w->addAction( Actions::Item_MoveToBottom, tr("Move to Bottom"),
                  "move_to_bottom",  tr("Ctrl+End"), "go-bottom", IconAngleDoubleDown );

    w->addAction( Actions::Tabs_NewTab, tr("&New Tab"),
                  "new_tab", tr("Ctrl+T"), ":/images/tab_new" );
    w->addAction( Actions::Tabs_RenameTab, tr("R&ename Tab"),
                  "rename_tab", tr("Ctrl+F2"), ":/images/tab_rename" );
    w->addAction( Actions::Tabs_RemoveTab, tr("Re&move Tab"),
                  "remove_tab", tr("Ctrl+W"), ":/images/tab_remove" );
    w->addAction( Actions::Tabs_ChangeTabIcon, tr("&Change Tab Icon"),
                  "change_tab_icon", tr("Ctrl+Shift+T"), ":/images/tab_icon" );
    w->addAction( Actions::Tabs_NextTab, tr("Ne&xt Tab"),
                  "next_tab", tr("Right"), "go-next", IconArrowRight );
    w->addAction( Actions::Tabs_PreviousTab, tr("&Previous Tab"),
                  "previous_tab", tr("Left"), "go-previous", IconArrowLeft );

    w->addAction( Actions::Help_ShowLog, tr("&Show Log"), "show-log", tr("F12"),
                  "help-about", IconExclamationSign );
    w->addAction( Actions::Help_Help, tr("&Help"), "help", QKeySequence::HelpContents,
                  "help-about", IconQuestionSign );
}
