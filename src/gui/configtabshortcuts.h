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
    File_Commands,
    File_ShowClipboardContent,
    File_ToggleClipboardStoring,
    File_ProcessManager,
    File_Exit,

    Edit_SortSelectedItems,
    Edit_ReverseSelectedItems,
    Edit_PasteItems,
    Edit_CopySelectedItems,
    Edit_FindItems,

    Item_MoveToClipboard,
    Item_ShowContent,
    Item_Remove,
    Item_Edit,
    Item_EditNotes,
    Item_EditWithEditor,
    Item_Action,

    Tabs_NewTab,
    Tabs_RenameTab,
    Tabs_RemoveTab,
    Tabs_ChangeTabIcon,

    Help_Help
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

    /** Return action with given @a id, set context and return the action. */
    QAction *action(Actions::Id id, QWidget *parent, Qt::ShortcutContext context);

    /** Return list of shortcuts defined for given @a id. */
    QList<QKeySequence> shortcuts(Actions::Id id) const;

    /** Disable shortcuts for all actions. */
    void setDisabledShortcuts(const QList<QKeySequence> &shortcuts);

    const QList<QKeySequence> &disabledShortcuts() const;

private:
    void initShortcuts();

    Ui::ConfigTabShortcuts *ui;
};

#endif // CONFIGTABSHORTCUTS_H
