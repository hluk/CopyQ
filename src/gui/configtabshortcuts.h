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

#ifndef CONFIGTABSHORTCUTS_H
#define CONFIGTABSHORTCUTS_H

#include <QWidget>

namespace Ui {
class ConfigTabShortcuts;
}

class QSettings;

namespace Actions {

enum Id {
    File_New,
    File_ImportTab,
    File_ExportTab,
    File_Preferences,
    File_ShowClipboardContent,
    File_ToggleClipboardStoring,
    File_Exit,

    Edit_SortSelectedItems,
    Edit_ReverseSelectedItems,
    Edit_PasteItems,
    Edit_CopySelectedItems,

    Item_MoveToClipboard,
    Item_ShowContent,
    Item_Remove,
    Item_Edit,
    Item_EditNotes,
    Item_EditWithEditor,
    Item_Action,
    Item_NextToClipboard,
    Item_PreviousToClipboard,

    Tabs_NewTab,
    Tabs_RenameTab,
    Tabs_RemoveTab,

    Help_Help,

    Global_ToggleMainWindow,
    Global_ShowTray,
    Global_EditClipboard,
    Global_EditFirstItem,
    Global_CopySecondItem,
    Global_ShowActionDialog,
    Global_CreateItem,
    Global_CopyNextItem,
    Global_CopyPreviousItem,
    Global_PasteAsPlainText,
    Global_DisableClipboardStoring,
    Global_EnableClipboardStoring,
    Global_PasteAndCopyNext,
    Global_PasteAndCopyPrevious
};

} // Actions

class ConfigTabShortcuts : public QWidget
{
    Q_OBJECT

public:
    explicit ConfigTabShortcuts(QWidget *parent = NULL);

    ~ConfigTabShortcuts();

    /** Load shortcuts from settings file. */
    void loadShortcuts(QSettings &settings);
    /** Save shortcuts to settings file. */
    void saveShortcuts(QSettings &settings) const;
    /** Load system-wide shortcuts from settings file. */
    void loadGlobalShortcuts(QSettings &settings);
    /** Save system-wide shortcuts to settings file. */
    void saveGlobalShortcuts(QSettings &settings) const;

    QAction *action(Actions::Id id, QWidget *parent, Qt::ShortcutContext context);

    QList<QKeySequence> shortcuts(Actions::Id id) const;

    void updateIcons();

    void setDisabledShortcuts(const QList<QKeySequence> &shortcuts);

private:
    void initShortcuts();

    Ui::ConfigTabShortcuts *ui;
};

#endif // CONFIGTABSHORTCUTS_H
