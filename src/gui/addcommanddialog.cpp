/*
    Copyright (c) 2018, Lukas Holecek <hluk@email.cz>

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
#include "common/globalshortcutcommands.h"
#include "common/shortcuts.h"
#include "common/textdata.h"
#include "item/itemfactory.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "gui/windowgeometryguard.h"
#include "platform/platformnativeinterface.h"

#include <QAbstractListModel>
#include <QLocale>
#include <QSortFilterProxyModel>

Q_DECLARE_METATYPE(Command)

namespace {

Command *newCommand(QVector<Command> *commands)
{
    commands->append(Command());
    return &commands->last();
}

QVector<Command> defaultCommands()
{
    const QRegExp reURL("^(https?|ftps?|file)://");
    const QRegExp reNotURL("^(?!(http|ftp)s?://)");

    QVector<Command> commands = globalShortcutCommands();
    Command *c;

    commands.prepend(Command());
    c = &commands.first();
    c->name = AddCommandDialog::tr("New command");
    c->icon = QString(QChar(IconFileAlt));
    c->input = c->output = "";
    c->wait = c->automatic = c->remove = false;
    c->sep = QString("\\n");

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Ignore items with no or single character");
    c->input = mimeText;
    c->re   = QRegExp("^\\s*\\S?\\s*$");
    c->icon = QString(QChar(IconExclamationCircle));
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
    c->icon = QString(QChar(IconImage));
    c->cmd  = "convert - -resize 92x92 png:-";
    c->input = "image/png";
    c->output = "image/png";
    c->inMenu = true;

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Create QR Code from URL (needs qrencode)");
    c->re   = reURL;
    c->icon = QString(QChar(IconQrcode));
    c->cmd  = "qrencode -o - -t PNG -s 6";
    c->input = mimeText;
    c->output = "image/png";
    c->inMenu = true;

    const auto todoTab = AddCommandDialog::tr("Tasks", "Tab name for some predefined commands");
    const auto todoTabQuoted = quoteString(todoTab);
    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Add to %1 tab", "%1 is quoted Tasks tab name")
            .arg(todoTabQuoted);
    c->icon = QString(QChar(IconShare));
    c->tab  = todoTab;
    c->input = mimeText;
    c->inMenu = true;

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Move to %1 tab", "%1 is quoted Tasks tab name")
            .arg(todoTabQuoted);
    c->icon = QString(QChar(IconShare));
    c->tab  = todoTab;
    c->input = mimeText;
    c->remove = true;
    c->inMenu = true;

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Ignore copied files");
    c->re   = reNotURL;
    c->icon = QString(QChar(IconExclamationCircle));
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
    explicit CommandModel(const QVector<Command> &commands, QObject *parent = nullptr)
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
    QVector<Command> m_commands;
};

} // namespace

AddCommandDialog::AddCommandDialog(const QVector<Command> &pluginCommands, QWidget *parent)
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
        QVector<Command> commands;

        commands.reserve( indexes.size() );
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
