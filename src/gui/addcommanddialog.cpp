/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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

#include "addcommanddialog.h"
#include "ui_addcommanddialog.h"

#include "common/command.h"
#include "common/mimetypes.h"
#include "common/shortcuts.h"
#include "item/itemfactory.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "gui/windowgeometryguard.h"
#include "platform/platformnativeinterface.h"

#include <QAbstractListModel>
#include <QSortFilterProxyModel>

Q_DECLARE_METATYPE(Command)

namespace {

Command *newCommand(QList<Command> *commands)
{
    commands->append(Command());
    return &commands->last();
}

QString pasteAsPlainTextScript(const QString &what)
{
    return "\n"
           "var text = " + what + "\n"
           "copy(text)\n"
           "copySelection(text)\n"
           "paste()";
}

#ifndef NO_GLOBAL_SHORTCUTS
enum GlobalAction {
    GlobalActionToggleMainWindow,
    GlobalActionShowTray,
    GlobalActionShowMainWindowUnderMouseCursor,
    GlobalActionEditClipboard,
    GlobalActionEditFirstItem,
    GlobalActionCopySecondItem,
    GlobalActionShowActionDialog,
    GlobalActionCreateItem,
    GlobalActionCopyNextItem,
    GlobalActionCopyPreviousItem,
    GlobalActionPasteAsPlainText,
    GlobalActionDisableClipboardStoring,
    GlobalActionEnableClipboardStoring,
    GlobalActionPasteAndCopyNext,
    GlobalActionPasteAndCopyPrevious
};

void createGlobalShortcut(const QString &name, const QString &script, IconId icon,
                          const QStringList &s, Command *c)
{
    c->name = name;
    c->cmd = "copyq: " + script;
    c->icon = QString(QChar(icon));
    QString shortcutNativeText =
            AddCommandDialog::tr("Ctrl+Shift+1", "Global shortcut for some predefined commands");
    c->globalShortcuts = s.isEmpty() ? QStringList(toPortableShortcutText(shortcutNativeText)) : s;
}

void createGlobalShortcut(GlobalAction id, Command *c, const QStringList &s = QStringList())
{
    if (id == GlobalActionToggleMainWindow)
        createGlobalShortcut( AddCommandDialog::tr("Show/hide main window"), "toggle()", IconListAlt, s, c );
    else if (id == GlobalActionShowTray)
        createGlobalShortcut( AddCommandDialog::tr("Show the tray menu"), "menu()", IconInbox, s, c );
    else if (id == GlobalActionShowMainWindowUnderMouseCursor)
        createGlobalShortcut( AddCommandDialog::tr("Show main window under mouse cursor"), "showAt()", IconListAlt, s, c );
    else if (id == GlobalActionEditClipboard)
        createGlobalShortcut( AddCommandDialog::tr("Edit clipboard"), "edit(-1)", IconEdit, s, c );
    else if (id == GlobalActionEditFirstItem)
        createGlobalShortcut( AddCommandDialog::tr("Edit first item"), "edit(0)", IconEdit, s, c );
    else if (id == GlobalActionCopySecondItem)
        createGlobalShortcut( AddCommandDialog::tr("Copy second item"), "select(1)", IconCopy, s, c );
    else if (id == GlobalActionShowActionDialog)
        createGlobalShortcut( AddCommandDialog::tr("Show action dialog"), "action()", IconCog, s, c );
    else if (id == GlobalActionCreateItem)
        createGlobalShortcut( AddCommandDialog::tr("Create new item"), "edit()", IconAsterisk, s, c );
    else if (id == GlobalActionCopyNextItem)
        createGlobalShortcut( AddCommandDialog::tr("Copy next item"), "next()", IconArrowDown, s, c );
    else if (id == GlobalActionCopyPreviousItem)
        createGlobalShortcut( AddCommandDialog::tr("Copy previous item"), "previous()", IconArrowUp, s, c );
    else if (id == GlobalActionPasteAsPlainText)
        createGlobalShortcut( AddCommandDialog::tr("Paste clipboard as plain text"), pasteAsPlainTextScript("clipboard()"), IconPaste, s, c );
    else if (id == GlobalActionDisableClipboardStoring)
        createGlobalShortcut( AddCommandDialog::tr("Disable clipboard storing"), "disable()", IconEyeSlash, s, c );
    else if (id == GlobalActionEnableClipboardStoring)
        createGlobalShortcut( AddCommandDialog::tr("Enable clipboard storing"), "enable()", IconEyeOpen, s, c );
    else if (id == GlobalActionPasteAndCopyNext)
        createGlobalShortcut( AddCommandDialog::tr("Paste and copy next"), "paste(); next()", IconArrowCircleODown, s, c );
    else if (id == GlobalActionPasteAndCopyPrevious)
        createGlobalShortcut( AddCommandDialog::tr("Paste and copy previous"), "paste(); previous()", IconArrowCircleOUp, s, c );
    else
        Q_ASSERT(false);
}

void createGlobalShortcut(GlobalAction id, QList<Command> *commands)
{
    Command *c = newCommand(commands);
    createGlobalShortcut(id, c);
}
#endif // !NO_GLOBAL_SHORTCUTS

QList<Command> defaultCommands()
{
    const QRegExp reURL("^(https?|ftps?|file)://");
    const QRegExp reNotURL("^(?!(http|ftp)s?://)");

    QList<Command> commands;
    Command *c;

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("New command");
    c->icon = QString(QChar(IconFileAlt));
    c->input = c->output = "";
    c->wait = c->automatic = c->remove = false;
    c->sep = QString("\\n");

#ifndef NO_GLOBAL_SHORTCUTS
    createGlobalShortcut(GlobalActionToggleMainWindow, &commands);
    createGlobalShortcut(GlobalActionShowTray, &commands);
    createGlobalShortcut(GlobalActionShowMainWindowUnderMouseCursor, &commands);
    createGlobalShortcut(GlobalActionEditClipboard, &commands);
    createGlobalShortcut(GlobalActionEditFirstItem, &commands);
    createGlobalShortcut(GlobalActionCopySecondItem, &commands);
    createGlobalShortcut(GlobalActionShowActionDialog, &commands);
    createGlobalShortcut(GlobalActionCreateItem, &commands);
    createGlobalShortcut(GlobalActionCopyNextItem, &commands);
    createGlobalShortcut(GlobalActionCopyPreviousItem, &commands);
    createGlobalShortcut(GlobalActionPasteAsPlainText, &commands);
    createGlobalShortcut(GlobalActionDisableClipboardStoring, &commands);
    createGlobalShortcut(GlobalActionEnableClipboardStoring, &commands);
    createGlobalShortcut(GlobalActionPasteAndCopyNext, &commands);
    createGlobalShortcut(GlobalActionPasteAndCopyPrevious, &commands);
#endif

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Ignore items with no or single character");
    c->re   = QRegExp("^\\s*\\S?\\s*$");
    c->icon = QString(QChar(IconExclamationSign));
    c->remove = true;
    c->automatic = true;

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Open in &Browser");
    c->re   = reURL;
    c->icon = QString(QChar(IconGlobe));
    c->cmd  = "copyq open %1";
    c->hideWindow = true;
    c->inMenu = true;

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Paste as Plain Text");
    c->input = mimeText;
    c->icon = QString(QChar(IconPaste));
    c->cmd  = "copyq:" + pasteAsPlainTextScript("input()");
    c->hideWindow = true;
    c->inMenu = true;
    c->shortcuts.append( toPortableShortcutText(AddCommandDialog::tr("Shift+Return")) );

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Autoplay videos");
    c->re   = QRegExp("^http://.*\\.(mp4|avi|mkv|wmv|flv|ogv)$");
    c->icon = QString(QChar(IconPlayCircle));
    c->cmd  = "copyq open %1";
    c->automatic = true;
    c->hideWindow = true;
    c->inMenu = true;

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Copy URL (web address) to other tab");
    c->re   = reURL;
    c->icon = QString(QChar(IconCopy));
    c->tab  = "&web";
    c->automatic = true;

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Create thumbnail (needs ImageMagick)");
    c->icon = QString(QChar(IconPicture));
    c->cmd  = "convert - -resize 92x92 png:-";
    c->input = "image/png";
    c->output = "image/png";
    c->inMenu = true;

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Create QR Code from URL (needs qrencode)");
    c->re   = reURL;
    c->icon = QString(QChar(IconQRCode));
    c->cmd  = "qrencode -o - -t PNG -s 6";
    c->input = mimeText;
    c->output = "image/png";
    c->inMenu = true;

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Add to &TODO tab");
    c->icon = QString(QChar(IconShare));
    c->tab  = "TODO";
    c->input = mimeText;
    c->inMenu = true;

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Move to &TODO tab");
    c->icon = QString(QChar(IconShare));
    c->tab  = "TODO";
    c->input = mimeText;
    c->remove = true;
    c->inMenu = true;

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Ignore copied files");
    c->re   = reNotURL;
    c->icon = QString(QChar(IconExclamationSign));
    c->input = mimeUriList;
    c->remove = true;
    c->automatic = true;

    if ( createPlatformNativeInterface()->canGetWindowTitle() ) {
        c = newCommand(&commands);
        c->name = AddCommandDialog::tr("Ignore *\"Password\"* window");
        c->wndre = QRegExp(AddCommandDialog::tr("Password"));
        c->icon = QString(QChar(IconAsterisk));
        c->remove = true;
        c->automatic = true;
        c->cmd = "copyq ignore";
    }

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Move to Trash");
    c->icon = QString(QChar(IconTrash));
    c->inMenu = true;
    c->tab  = AddCommandDialog::tr("(trash)");
    c->remove = true;
    c->shortcuts.append( toPortableShortcutText(shortcutToRemove()) );

    return commands;
}

class CommandModel : public QAbstractListModel {
public:
    explicit CommandModel(const QList<Command> &commands, QObject *parent = nullptr)
        : QAbstractListModel(parent)
        , m_commands(commands)
    {
    }

    int rowCount(const QModelIndex &) const override
    {
        return m_commands.size();
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid())
            return QVariant();

        if (role == Qt::DecorationRole) {
            const QString &icon = m_commands[index.row()].icon;

            QVariant iconOrIconId;
            if (icon.size() == 1)
                iconOrIconId = static_cast<uint>(icon[0].unicode());
            else
                iconOrIconId = QIcon(icon);

            return getIcon(iconOrIconId);
        }
        if (role == Qt::DisplayRole)
            return m_commands[index.row()].name;
        if (role == Qt::UserRole)
            return QVariant::fromValue(m_commands[index.row()]);

        return QVariant();
    }

private:
    QList<Command> m_commands;
};

} // namespace

AddCommandDialog::AddCommandDialog(const QList<Command> &pluginCommands, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AddCommandDialog)
    , m_filterModel(new QSortFilterProxyModel(this))
{
    ui->setupUi(this);

    ui->filterLineEdit->loadSettings();

    QAbstractItemModel *model = new CommandModel(defaultCommands() + pluginCommands, m_filterModel);
    m_filterModel->setSourceModel(model);
    ui->listViewCommands->setModel(m_filterModel);
    ui->listViewCommands->setCurrentIndex(m_filterModel->index(0, 0));

    WindowGeometryGuard::create(this);
}

AddCommandDialog::~AddCommandDialog()
{
    delete ui;
}

void AddCommandDialog::accept()
{
    const QModelIndexList indexes =
            ui->listViewCommands->selectionModel()->selectedIndexes();

    if (!indexes.isEmpty()) {
        QList<Command> commands;

        for (const auto &index : indexes)
            commands.append( index.data(Qt::UserRole).value<Command>() );

        emit addCommands(commands);
    }

    QDialog::accept();
}

void AddCommandDialog::on_filterLineEdit_filterChanged(const QRegExp &re)
{
    m_filterModel->setFilterRegExp(re);
}

void AddCommandDialog::on_listViewCommands_activated()
{
    accept();
}
