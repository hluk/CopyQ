// SPDX-License-Identifier: GPL-3.0-or-later

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
    c->input = c->output = QString();
    c->wait = c->automatic = c->remove = false;
    c->sep = QLatin1String("\\n");

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Ignore items with no or single character");
    c->icon = QString(QChar(IconCircleExclamation));
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
    c->cmd  = QStringLiteral("copyq open %1");
    c->hideWindow = true;
    c->inMenu = true;

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Paste as Plain Text");
    c->input = mimeText;
    c->icon = QString(QChar(IconPaste));
    c->cmd  = QStringLiteral("copyq:") + pasteAsPlainTextScript("input()");
    c->hideWindow = true;
    c->inMenu = true;

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Autoplay videos");
    c->re   = QRegularExpression("^http://.*\\.(mp4|avi|mkv|wmv|flv|ogv)$");
    c->icon = QString(QChar(IconCirclePlay));
    c->cmd  = QStringLiteral("copyq open %1");
    c->automatic = true;
    c->hideWindow = true;
    c->inMenu = true;

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Copy URL (web address) to other tab");
    c->re   = reURL;
    c->icon = QString(QChar(IconCopy));
    c->tab  = QStringLiteral("&web");
    c->automatic = true;

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Create thumbnail (needs ImageMagick)");
    c->icon = QString(QChar(IconImage));
    c->cmd  = QStringLiteral("convert - -resize 92x92 png:-");
    c->input = QStringLiteral("image/png");
    c->output = QStringLiteral("image/png");
    c->inMenu = true;

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Create QR Code from URL (needs qrencode)");
    c->re   = reURL;
    c->icon = QString(QChar(IconQrcode));
    c->cmd  = QStringLiteral("qrencode -o - -t PNG -s 6");
    c->input = mimeText;
    c->output = QStringLiteral("image/png");
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
    c->remove = true;
    c->inMenu = true;

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Ignore copied files");
    c->re   = reNotURL;
    c->icon = QString(QChar(IconCircleExclamation));
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
        c->cmd = QStringLiteral("copyq ignore");
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
    c->cmd = QStringLiteral("copyq: ItemSelection(selectedTab()).selectRemovable().removeAll()");
    c->matchCmd = QStringLiteral("copyq: tab(selectedTab()); if (size() == 0) fail()");

    return commands;
}

