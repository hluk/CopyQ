// SPDX-License-Identifier: GPL-3.0-or-later

#include "predefinedcommands.h"

#include "common/command.h"
#include "common/globalshortcutcommands.h"
#include "common/mimetypes.h"
#include "common/shortcuts.h"
#include "common/textdata.h"
#include "gui/icons.h"
#include "gui/fromiconid.h"
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
    c->icon = fromIconId(IconFile);
    c->input = c->output = QString();
    c->wait = c->automatic = c->remove = false;
    c->sep = QLatin1String("\\n");

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Ignore items with no or single character");
    c->icon = fromIconId(IconCircleExclamation);
    c->cmd  = R"(function hasEmptyOrSingleCharText() {
    if (dataFormats().includes(mimeText)) {
        const text = str(data(mimeText));
        if (/^\s*.?\s*$/.test(text)) {
            serverLog('Ignoring text with single or no character');
            return true;
        }
    }
    return false;
}

function overrideFunction(fn) {
    const oldFn = global[fn];
    global[fn] = function() {
        if (!hasEmptyOrSingleCharText()) {
            oldFn();
        }
    }
}

overrideFunction('onClipboardChanged');
overrideFunction('provideClipboard');
overrideFunction('provideSelection');
    )";
    c->isScript = true;

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Open in &Browser");
    c->re   = reURL;
    c->icon = fromIconId(IconGlobe);
    c->cmd  = QStringLiteral("copyq open %1");
    c->hideWindow = true;
    c->inMenu = true;

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Paste as Plain Text");
    c->input = mimeText;
    c->icon = fromIconId(IconPaste);
    c->cmd  = QStringLiteral("copyq:") + pasteAsPlainTextScript("input()");
    c->hideWindow = true;
    c->inMenu = true;

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Autoplay videos");
    c->re   = QRegularExpression("^http://.*\\.(mp4|avi|mkv|wmv|flv|ogv)$");
    c->icon = fromIconId(IconCirclePlay);
    c->cmd  = QStringLiteral("copyq open %1");
    c->automatic = true;
    c->hideWindow = true;
    c->inMenu = true;

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Copy URL (web address) to other tab");
    c->re   = reURL;
    c->icon = fromIconId(IconCopy);
    c->tab  = QStringLiteral("&web");
    c->automatic = true;

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Create thumbnail (needs ImageMagick)");
    c->icon = fromIconId(IconImage);
    c->cmd  = QStringLiteral("convert - -resize 92x92 png:-");
    c->input = QStringLiteral("image/png");
    c->output = QStringLiteral("image/png");
    c->inMenu = true;

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Create QR Code from URL (needs qrencode)");
    c->re   = reURL;
    c->icon = fromIconId(IconQrcode);
    c->cmd  = QStringLiteral("qrencode -o - -t PNG -s 6");
    c->input = mimeText;
    c->output = QStringLiteral("image/png");
    c->inMenu = true;

    const auto todoTab = AddCommandDialog::tr("Tasks", "Tab name for some predefined commands");
    const auto todoTabQuoted = quoteString(todoTab);
    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Add to %1 tab", "%1 is quoted Tasks tab name")
            .arg(todoTabQuoted);
    c->icon = fromIconId(IconShare);
    c->tab  = todoTab;
    c->input = mimeText;
    c->inMenu = true;

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Move to %1 tab", "%1 is quoted Tasks tab name")
            .arg(todoTabQuoted);
    c->icon = fromIconId(IconShare);
    c->tab  = todoTab;
    c->remove = true;
    c->inMenu = true;

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Ignore copied files");
    c->re   = reNotURL;
    c->icon = fromIconId(IconCircleExclamation);
    c->input = mimeUriList;
    c->remove = true;
    c->automatic = true;

    if ( platformNativeInterface()->canGetWindowTitle() ) {
        c = newCommand(&commands);
        c->name = AddCommandDialog::tr("Ignore *\"Password\"* window");
        c->wndre = QRegularExpression(AddCommandDialog::tr("Password"));
        c->icon = fromIconId(IconAsterisk);
        c->remove = true;
        c->automatic = true;
        c->cmd = QStringLiteral("copyq ignore");
    }

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Move to Trash");
    c->icon = fromIconId(IconTrash);
    c->inMenu = true;
    c->tab  = AddCommandDialog::tr("(trash)");
    c->remove = true;

    c = newCommand(&commands);
    c->name = AddCommandDialog::tr("Clear Current Tab");
    c->icon = fromIconId(IconBroom);
    c->inMenu = true;
    c->cmd = QStringLiteral("copyq: ItemSelection(selectedTab()).selectRemovable().removeAll()");
    c->matchCmd = QStringLiteral("copyq: tab(selectedTab()); if (size() == 0) fail()");

    return commands;
}
