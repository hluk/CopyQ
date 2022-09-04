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

#include "predefinedcommands.h"

#include "common/command.h"
#include "common/globalshortcutcommands.h"
#include "common/mimetypes.h"
#include "common/shortcuts.h"
#include "common/textdata.h"
#include "gui/icons.h"
#include "platform/platformnativeinterface.h"

#include <QCoreApplication>

namespace {

Command *newCommand(QVector<Command> *commands)
{
    commands->append(Command());
    return &commands->last();
}

class AddCommandDialog final
{
    Q_DECLARE_TR_FUNCTIONS(AddCommandDialog)
};

} // namespace

QVector<Command> predefinedCommands()
{
    const QRegularExpression reURL("^(https?|ftps?|file)://");
    const QRegularExpression reNotURL("^(?!(http|ftp)s?://)");

    QVector<Command> commands = globalShortcutCommands();
    Command *c;

    commands.prepend(Command());
    c = &commands.first();
    c->name = AddCommandDialog::tr("New command");
    c->icon = QString(QChar(IconFile));
    c->input = c->output = "";
    c->wait = c->automatic = c->remove = false;
    c->sep = QLatin1String("\\n");

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Ignore items with no or single character");
    c->icon = QString(QChar(IconExclamationCircle));
    c->cmd  = R"(function hasEmptyOrSingleCharText() {
    if (dataFormats().includes(mimeText)) {
        var text = str(data(mimeText));
        if (text.match(/^\s*.?\s*$/)) {
            serverLog('Ignoring text with single or no character');
            return true;
        }
    }
    return false;
}

var onClipboardChanged_ = onClipboardChanged;
onClipboardChanged = function() {
    if (!hasEmptyOrSingleCharText()) {
        onClipboardChanged_();
    }
}

var synchronizeFromSelection_ = synchronizeFromSelection;
synchronizeFromSelection = function() {
    if (!hasEmptyOrSingleCharText()) {
        synchronizeFromSelection_();
    }
}

var synchronizeToSelection_ = synchronizeToSelection;
synchronizeToSelection = function() {
    if (!hasEmptyOrSingleCharText()) {
        synchronizeToSelection_();
    }
}
    )";
    c->isScript = true;

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

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Autoplay videos");
    c->re   = QRegularExpression("^http://.*\\.(mp4|avi|mkv|wmv|flv|ogv)$");
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

    if ( platformNativeInterface()->canGetWindowTitle() ) {
        c = newCommand(&commands);
        c->name = AddCommandDialog::tr("Ignore *\"Password\"* window");
        c->wndre = QRegularExpression(AddCommandDialog::tr("Password"));
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

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Clear Current Tab");
    c->icon = QString(QChar(IconBroom));
    c->inMenu = true;
    c->cmd = "copyq: ItemSelection(selectedTab()).selectRemovable().removeAll()";
    c->matchCmd = "copyq: tab(selectedTab()); if (size() == 0) fail()";

    return commands;
}

