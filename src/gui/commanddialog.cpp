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

#include "gui/commanddialog.h"
#include "ui_commanddialog.h"

#include "common/command.h"
#include "common/common.h"
#include "common/config.h"
#include "common/mimetypes.h"
#include "gui/commandwidget.h"
#include "gui/configurationmanager.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "item/encrypt.h"

#include <QFileDialog>
#include <QMenu>
#include <QMimeData>
#include <QSettings>
#include <QTemporaryFile>

namespace {

const QIcon iconLoadCommands(const QColor &color) { return getIcon("document-open", IconFolderOpen, color, color); }
const QIcon iconSaveCommands(const QColor &color) { return getIcon("document-save", IconSave, color, color); }

void loadCommand(QSettings *settings, bool onlyEnabled, CommandDialog::Commands *commands)
{
    Command c;
    c.enable = settings->value("Enable", true).toBool();

    if (onlyEnabled && !c.enable)
        return;

    c.name = settings->value("Name").toString();
    c.re   = QRegExp( settings->value("Match").toString() );
    c.wndre = QRegExp( settings->value("Window").toString() );
    c.matchCmd = settings->value("MatchCommand").toString();
    c.cmd = settings->value("Command").toString();
    c.sep = settings->value("Separator").toString();

    c.input = settings->value("Input").toString();
    if ( c.input == "false" || c.input == "true" )
        c.input = c.input == "true" ? QString(mimeText) : QString();

    c.output = settings->value("Output").toString();
    if ( c.output == "false" || c.output == "true" )
        c.output = c.output == "true" ? QString(mimeText) : QString();

    c.wait = settings->value("Wait").toBool();
    c.automatic = settings->value("Automatic").toBool();
    c.transform = settings->value("Transform").toBool();
    c.hideWindow = settings->value("HideWindow").toBool();
    c.icon = settings->value("Icon").toString();
    c.shortcuts = settings->value("Shortcut").toStringList();
    c.globalShortcuts = settings->value("GlobalShortcut").toStringList();
    c.tab = settings->value("Tab").toString();
    c.outputTab = settings->value("OutputTab").toString();
    c.inMenu = settings->value("InMenu").toBool();

    if (settings->value("Ignore").toBool()) {
        c.remove = c.automatic = true;
        settings->remove("Ignore");
        settings->setValue("Remove", c.remove);
        settings->setValue("Automatic", c.automatic);
    } else {
        c.remove = settings->value("Remove").toBool();
    }

    commands->append(c);
}

CommandDialog::Commands loadCommands(QSettings *settings, bool onlyEnabled = false)
{
    CommandDialog::Commands commands;

    const QStringList groups = settings->childGroups();

    if ( groups.contains("Command") ) {
        settings->beginGroup("Command");
        loadCommand(settings, onlyEnabled, &commands);
        settings->endGroup();
    }

    int size = settings->beginReadArray("Commands");

    for(int i=0; i<size; ++i) {
        settings->setArrayIndex(i);
        loadCommand(settings, onlyEnabled, &commands);
    }

    settings->endArray();

    return commands;
}

void saveValue(const char *key, const QRegExp &re, QSettings *settings)
{
    settings->setValue(key, re.pattern());
}

void saveValue(const char *key, const QVariant &value, QSettings *settings)
{
    settings->setValue(key, value);
}

/// Save only modified command properties.
template <typename Member>
void saveNewValue(const char *key, const Command &command, const Member &member, QSettings *settings)
{
    if (command.*member != Command().*member)
        saveValue(key, command.*member, settings);
}

void saveCommand(const Command &c, QSettings *settings)
{
    saveNewValue("Name", c, &Command::name, settings);
    saveNewValue("Match", c, &Command::re, settings);
    saveNewValue("Window", c, &Command::wndre, settings);
    saveNewValue("MatchCommand", c, &Command::matchCmd, settings);
    saveNewValue("Command", c, &Command::cmd, settings);
    saveNewValue("Input", c, &Command::input, settings);
    saveNewValue("Output", c, &Command::output, settings);

    // Separator for new command is set to '\n' for convenience.
    // But this value shouldn't be saved if output format is not set.
    if ( c.sep != "\\n" || !c.output.isEmpty() )
        saveNewValue("Separator", c, &Command::sep, settings);

    saveNewValue("Wait", c, &Command::wait, settings);
    saveNewValue("Automatic", c, &Command::automatic, settings);
    saveNewValue("InMenu", c, &Command::inMenu, settings);
    saveNewValue("Transform", c, &Command::transform, settings);
    saveNewValue("Remove", c, &Command::remove, settings);
    saveNewValue("HideWindow", c, &Command::hideWindow, settings);
    saveNewValue("Enable", c, &Command::enable, settings);
    saveNewValue("Icon", c, &Command::icon, settings);
    saveNewValue("Shortcut", c, &Command::shortcuts, settings);
    saveNewValue("GlobalShortcut", c, &Command::globalShortcuts, settings);
    saveNewValue("Tab", c, &Command::tab, settings);
    saveNewValue("OutputTab", c, &Command::outputTab, settings);
}

void saveCommands(const CommandDialog::Commands &commands, QSettings *settings)
{
    settings->remove("Commands");
    settings->remove("Command");

    if (commands.size() == 1) {
        settings->beginGroup("Command");
        saveCommand(commands[0], settings);
        settings->endGroup();
    } else {
        settings->beginWriteArray("Commands");
        int i = 0;
        foreach (const Command &c, commands) {
            settings->setArrayIndex(i++);
            saveCommand(c, settings);
        }
        settings->endArray();
    }
}

#ifndef NO_GLOBAL_SHORTCUTS
enum GlobalAction {
    GlobalActionToggleMainWindow,
    GlobalActionShowTray,
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

QString toPortableShortcutText(const QString &shortcutNativeText)
{
    return QKeySequence(shortcutNativeText, QKeySequence::NativeText)
            .toString(QKeySequence::PortableText);
}

void createGlobalShortcut(const QString &name, const QString &script, IconId icon,
                          const QStringList &s, Command *c)
{
    c->name = name;
    c->cmd = "copyq: " + script;
    c->icon = QString(QChar(icon));
    QString shortcutNativeText =
            ConfigurationManager::tr("Ctrl+Shift+1", "Global shortcut for some predefined commands");
    c->globalShortcuts = s.isEmpty() ? QStringList(toPortableShortcutText(shortcutNativeText)) : s;
}

void createGlobalShortcut(GlobalAction id, Command *c, const QStringList &s = QStringList())
{
    if (id == GlobalActionToggleMainWindow)
        createGlobalShortcut( ConfigurationManager::tr("Show/hide main window"), "toggle()", IconListAlt, s, c );
    else if (id == GlobalActionShowTray)
        createGlobalShortcut( ConfigurationManager::tr("Show the tray menu"), "menu()", IconInbox, s, c );
    else if (id == GlobalActionEditClipboard)
        createGlobalShortcut( ConfigurationManager::tr("Edit clipboard"), "edit(-1)", IconEdit, s, c );
    else if (id == GlobalActionEditFirstItem)
        createGlobalShortcut( ConfigurationManager::tr("Edit first item"), "edit(0)", IconEdit, s, c );
    else if (id == GlobalActionCopySecondItem)
        createGlobalShortcut( ConfigurationManager::tr("Copy second item"), "select(1)", IconCopy, s, c );
    else if (id == GlobalActionShowActionDialog)
        createGlobalShortcut( ConfigurationManager::tr("Show action dialog"), "action()", IconCog, s, c );
    else if (id == GlobalActionCreateItem)
        createGlobalShortcut( ConfigurationManager::tr("Create new item"), "edit()", IconAsterisk, s, c );
    else if (id == GlobalActionCopyNextItem)
        createGlobalShortcut( ConfigurationManager::tr("Copy next item"), "next()", IconArrowDown, s, c );
    else if (id == GlobalActionCopyPreviousItem)
        createGlobalShortcut( ConfigurationManager::tr("Copy previous item"), "previous()", IconArrowUp, s, c );
    else if (id == GlobalActionPasteAsPlainText)
        createGlobalShortcut( ConfigurationManager::tr("Paste clipboard as plain text"), "copy(clipboard()); paste()", IconPaste, s, c );
    else if (id == GlobalActionDisableClipboardStoring)
        createGlobalShortcut( ConfigurationManager::tr("Disable clipboard storing"), "disable()", IconEyeSlash, s, c );
    else if (id == GlobalActionEnableClipboardStoring)
        createGlobalShortcut( ConfigurationManager::tr("Enable clipboard storing"), "enable()", IconEyeOpen, s, c );
    else if (id == GlobalActionPasteAndCopyNext)
        createGlobalShortcut( ConfigurationManager::tr("Paste and copy next"), "paste(); next()", IconArrowCircleODown, s, c );
    else if (id == GlobalActionPasteAndCopyPrevious)
        createGlobalShortcut( ConfigurationManager::tr("Paste and copy previous"), "paste(); previous()", IconArrowCircleOUp, s, c );
    else
        Q_ASSERT(false);
}

void restoreGlobalAction(GlobalAction id, const QString &key, QSettings *settings, CommandDialog::Commands *commands)
{
    const QStringList shortcuts = settings->value(key).toStringList();
    if ( !shortcuts.isEmpty() ) {
        Command c;
        createGlobalShortcut(id, &c, shortcuts);
        commands->append(c);
        settings->remove(key);
    }
}

/**
 * Load deprecated global shortcut settings.
 * Removes the settings and stores as commands.
 */
void restoreGlobalActions()
{
    CommandDialog::Commands commands;
    QSettings settings;

    settings.beginGroup("Options");
    restoreGlobalAction( GlobalActionToggleMainWindow, "toggle_shortcut", &settings, &commands );
    restoreGlobalAction( GlobalActionShowTray, "menu_shortcut", &settings, &commands );
    restoreGlobalAction( GlobalActionEditClipboard, "edit_clipboard_shortcut", &settings, &commands );
    restoreGlobalAction( GlobalActionEditFirstItem, "edit_shortcut", &settings, &commands );
    restoreGlobalAction( GlobalActionCopySecondItem, "second_shortcut", &settings, &commands );
    restoreGlobalAction( GlobalActionShowActionDialog, "show_action_dialog", &settings, &commands );
    restoreGlobalAction( GlobalActionCreateItem, "new_item_shortcut", &settings, &commands );
    restoreGlobalAction( GlobalActionCopyNextItem, "next_item_shortcut", &settings, &commands );
    restoreGlobalAction( GlobalActionCopyPreviousItem, "previous_item_shortcut", &settings, &commands );
    restoreGlobalAction( GlobalActionPasteAsPlainText, "paste_as_plain_text", &settings, &commands );
    restoreGlobalAction( GlobalActionDisableClipboardStoring, "disable_monitoring_shortcut", &settings, &commands );
    restoreGlobalAction( GlobalActionEnableClipboardStoring, "enable_monitoring_shortcut", &settings, &commands );
    restoreGlobalAction( GlobalActionPasteAndCopyNext, "paste_and_copy_next_shortcut", &settings, &commands );
    restoreGlobalAction( GlobalActionPasteAndCopyPrevious, "paste_and_copy_previous_shortcut", &settings, &commands );
    settings.endGroup();

    if ( !commands.isEmpty() ) {
        commands.append( loadCommands(&settings, false) );
        saveCommands(commands, &settings);
    }
}

#endif

} // namespace

CommandDialog::CommandDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CommandDialog)
{
    restoreGlobalActions();

    ui->setupUi(this);
    ui->itemOrderListCommands->setFocus();

    foreach ( const Command &command, commands(false) )
        addCommandWithoutSave(command);

    Command cmd;
    QMenu *menu = new QMenu(this);
    IconFactory *iconFactory = ConfigurationManager::instance()->iconFactory();

    for ( int i = 1; defaultCommand(i, &cmd); ++i ) {
        menu->addAction( iconFactory->iconFromFile(cmd.icon), cmd.name.remove('&') )
                ->setProperty("COMMAND", i);
    }

    ui->itemOrderListCommands->setAddMenu(menu);

    QAction *act = new QAction(ui->itemOrderListCommands);
    ui->itemOrderListCommands->addAction(act);
    act->setShortcut(QKeySequence::Paste);
    connect(act, SIGNAL(triggered()), SLOT(tryPasteCommandFromClipboard()));

    act = new QAction(ui->itemOrderListCommands);
    ui->itemOrderListCommands->addAction(act);
    act->setShortcut(QKeySequence::Copy);
    connect(act, SIGNAL(triggered()), SLOT(copySelectedCommandsToClipboard()));

    ui->itemOrderListCommands->setDragAndDropValidator(QRegExp("\\[Commands?\\]"));
    connect( ui->itemOrderListCommands, SIGNAL(dropped(QString,int)),
             SLOT(onCommandDropped(QString,int)) );

    connect(this, SIGNAL(finished(int)), SLOT(onFinished(int)));

    restoreWindowGeometry(this, false);

    ConfigurationManager *cm = ConfigurationManager::instance();
    connect( cm, SIGNAL(configurationChanged()),
             this, SLOT(loadSettings()) );
    loadSettings();
}

CommandDialog::~CommandDialog()
{
    delete ui;
}

void CommandDialog::loadSettings()
{
    static const QColor color = getDefaultIconColor(*ui->pushButtonLoadCommands, QPalette::Window);
    ui->pushButtonLoadCommands->setIcon(iconLoadCommands(color));
    ui->pushButtonSaveCommands->setIcon(iconSaveCommands(color));

    ui->itemOrderListCommands->updateIcons();

    for (int i = 0; i < ui->itemOrderListCommands->itemCount(); ++i) {
        QWidget *w = ui->itemOrderListCommands->itemWidget(i);
        CommandWidget *cmdWidget = qobject_cast<CommandWidget *>(w);
        cmdWidget->updateIcons();
    }
}

CommandDialog::Commands CommandDialog::commands(bool onlyEnabled, bool onlySaved) const
{
    if (!onlySaved) {
        Commands commands;

        for (int i = 0; i < ui->itemOrderListCommands->itemCount(); ++i) {
            QWidget *w = ui->itemOrderListCommands->itemWidget(i);
            CommandWidget *cmdWidget = qobject_cast<CommandWidget *>(w);
            Command c = cmdWidget->command();
            c.enable = ui->itemOrderListCommands->isItemChecked(i);

            if (!onlyEnabled || c.enable)
                commands.append(c);
        }

        return commands;
    }

    return loadCommands(onlyEnabled);
}

void CommandDialog::addCommand(const Command &command)
{
    addCommandWithoutSave(command);
    Commands cmds = commands(false, true);
    cmds.append(command);
}

void CommandDialog::apply()
{
    const Commands cmds = commands(false, false);
    saveCommands(cmds);
    emit commandsSaved();
}

void CommandDialog::tryPasteCommandFromClipboard()
{
    const QMimeData *data = clipboardData(QClipboard::Clipboard);
    if (data && data->hasText()) {
        const QString text = data->text().trimmed();
        if ( text.startsWith("[Command]") || text.startsWith("[Commands]") )
            onCommandDropped( text, ui->itemOrderListCommands->currentRow() );
    }
}

void CommandDialog::copySelectedCommandsToClipboard()
{
    QApplication::clipboard()->setText(serializeSelectedCommands());
}

void CommandDialog::onCommandDropped(const QString &text, int row)
{
    QTemporaryFile tmpfile;
    if ( !openTemporaryFile(&tmpfile) )
        return;

    if ( !tmpfile.open() )
        return;

    tmpfile.write(text.toUtf8());
    tmpfile.flush();

    loadCommandsFromFile( tmpfile.fileName(), row );
}

void CommandDialog::onCurrentCommandWidgetIconChanged(const QString &iconString)
{
    ui->itemOrderListCommands->setCurrentItemIcon( getCommandIcon(iconString) );
}

void CommandDialog::onCurrentCommandWidgetNameChanged(const QString &name)
{
    ui->itemOrderListCommands->setCurrentItemLabel(name);
}

void CommandDialog::onCurrentCommandWidgetAutomaticChanged(bool automatic)
{
    ui->itemOrderListCommands->setCurrentItemHighlight(automatic);
}

void CommandDialog::onFinished(int result)
{
    if (result == QDialog::Accepted)
        apply();

    saveWindowGeometry(this, false);
}

void CommandDialog::on_itemOrderListCommands_addButtonClicked(QAction *action)
{
    Command cmd;
    if ( !defaultCommand(action->property("COMMAND").toInt(), &cmd) )
        return;

    const int targetRow = qMax( 0, ui->itemOrderListCommands->currentRow() );
    addCommandWithoutSave(cmd, targetRow);
    ui->itemOrderListCommands->setCurrentItem(targetRow);
}

void CommandDialog::on_itemOrderListCommands_itemSelectionChanged()
{
    bool saveEnabled = !ui->itemOrderListCommands->selectedRows().isEmpty();
    ui->pushButtonSaveCommands->setEnabled(saveEnabled);
}

void CommandDialog::on_pushButtonLoadCommands_clicked()
{
    const QStringList fileNames =
            QFileDialog::getOpenFileNames(this, tr("Open Files with Commands"),
                                          QString(), tr("Commands (*.ini);; CopyQ Configuration (copyq.conf copyq-*.conf)"));

    foreach (const QString &fileName, fileNames)
        loadCommandsFromFile(fileName, -1);
}

void CommandDialog::on_pushButtonSaveCommands_clicked()
{
    QString fileName =
            QFileDialog::getSaveFileName(this, tr("Save Selected Commands"),
                                          QString(), tr("Commands (*.ini)"));
    if ( !fileName.isEmpty() ) {
        if ( !fileName.endsWith(".ini") )
            fileName.append(".ini");

        QFile ini(fileName);
        ini.open(QIODevice::WriteOnly);
        ini.write(serializeSelectedCommands().toUtf8());
    }
}

void CommandDialog::on_lineEditFilterCommands_textChanged(const QString &text)
{
    for (int i = 0; i < ui->itemOrderListCommands->itemCount(); ++i) {
        QWidget *w = ui->itemOrderListCommands->itemWidget(i);
        CommandWidget *cmdWidget = qobject_cast<CommandWidget *>(w);
        const Command c = cmdWidget->command();
        bool show = text.isEmpty() || QString(c.name).remove('&').contains(text, Qt::CaseInsensitive)
                || c.cmd.contains(text, Qt::CaseInsensitive);
        ui->itemOrderListCommands->setItemWidgetVisible(i, show);
    }
}

void CommandDialog::on_buttonBox_clicked(QAbstractButton *button)
{
    switch( ui->buttonBox->buttonRole(button) ) {
    case QDialogButtonBox::ApplyRole:
        apply();
        break;
    case QDialogButtonBox::AcceptRole:
        accept();
        break;
    case QDialogButtonBox::RejectRole:
        reject();
        break;
    default:
        break;
    }
}

void CommandDialog::addCommandWithoutSave(const Command &command, int targetRow)
{
    CommandWidget *cmdWidget = new CommandWidget(this);

    cmdWidget->setCommand(command);

    connect( cmdWidget, SIGNAL(iconChanged(QString)),
             this, SLOT(onCurrentCommandWidgetIconChanged(QString)) );
    connect( cmdWidget, SIGNAL(nameChanged(QString)),
             this, SLOT(onCurrentCommandWidgetNameChanged(QString)) );
    connect( cmdWidget, SIGNAL(automaticChanged(bool)),
             this, SLOT(onCurrentCommandWidgetAutomaticChanged(bool)) );

    ui->itemOrderListCommands->insertItem(command.name, command.enable, command.automatic,
                                          getCommandIcon(cmdWidget->currentIcon()), cmdWidget,
                                          targetRow);
}

QIcon CommandDialog::getCommandIcon(const QString &iconString) const
{
    static const QColor color = getDefaultIconColor(*ui->itemOrderListCommands, QPalette::Base);
    IconFactory *iconFactory = ConfigurationManager::instance()->iconFactory();
    return iconFactory->iconFromFile(iconString, color);
}

void CommandDialog::loadCommandsFromFile(const QString &fileName, int targetRow)
{
    QSettings commandsSettings(fileName, QSettings::IniFormat);
    QList<int> rowsToSelect;

    const int count = ui->itemOrderListCommands->rowCount();
    int row = targetRow >= 0 ? targetRow : count;

    foreach ( Command command, loadCommands(&commandsSettings) ) {
        rowsToSelect.append(row);

        if (command.cmd.startsWith("\n    ")) {
            command.cmd.remove(0, 5);
            command.cmd.replace("\n    ", "\n");
        }

        addCommandWithoutSave(command, row);
        row++;
    }

    ui->itemOrderListCommands->setSelectedRows(rowsToSelect);
}

CommandDialog::Commands CommandDialog::selectedCommands() const
{
    QList<int> rows = ui->itemOrderListCommands->selectedRows();
    const Commands allCommands = commands(false, false);
    Commands commandsToSave;
    foreach (int row, rows) {
        Q_ASSERT(row < allCommands.size());
        commandsToSave.append(allCommands.value(row));
    }

    return commandsToSave;
}

bool CommandDialog::defaultCommand(int index, Command *c) const
{
    static const QRegExp reURL("^(https?|ftps?|file)://");

    *c = Command();
    int i = 0;
    if (index == ++i) {
        c->name = tr("New command");
        c->input = c->output = "";
        c->wait = c->automatic = c->remove = false;
        c->sep = QString("\\n");
#ifndef NO_GLOBAL_SHORTCUTS
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionToggleMainWindow, c);
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionShowTray, c);
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionEditClipboard, c);
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionEditFirstItem, c);
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionCopySecondItem, c);
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionShowActionDialog, c);
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionCreateItem, c);
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionCopyNextItem, c);
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionCopyPreviousItem, c);
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionPasteAsPlainText, c);
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionDisableClipboardStoring, c);
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionEnableClipboardStoring, c);
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionPasteAndCopyNext, c);
    } else if (index == ++i) {
        createGlobalShortcut(GlobalActionPasteAndCopyPrevious, c);
#endif
    } else if (index == ++i) {
        c->name = tr("Ignore items with no or single character");
        c->re   = QRegExp("^\\s*\\S?\\s*$");
        c->icon = QString(QChar(IconExclamationSign));
        c->remove = true;
        c->automatic = true;
    } else if (index == ++i) {
        c->name = tr("Open in &Browser");
        c->re   = reURL;
        c->icon = QString(QChar(IconGlobe));
        c->cmd  = "firefox %1";
        c->hideWindow = true;
        c->inMenu = true;
    } else if (index == ++i) {
        c->name = tr("Paste as Plain Text");
        c->input = mimeText;
        c->icon = QString(QChar(IconPaste));
        c->cmd  = "copyq:\ncopy(input())\npaste()";
        c->hideWindow = true;
        c->inMenu = true;
        c->shortcuts.append( toPortableShortcutText(tr("Shift+Return")) );
    } else if (index == ++i) {
        c->name = tr("Autoplay videos");
        c->re   = QRegExp("^http://.*\\.(mp4|avi|mkv|wmv|flv|ogv)$");
        c->icon = QString(QChar(IconPlayCircle));
        c->cmd  = "vlc %1";
        c->automatic = true;
        c->hideWindow = true;
        c->inMenu = true;
    } else if (index == ++i) {
        c->name = tr("Copy URL (web address) to other tab");
        c->re   = reURL;
        c->icon = QString(QChar(IconCopy));
        c->tab  = "&web";
        c->automatic = true;
    } else if (index == ++i) {
        c->name = tr("Create thumbnail (needs ImageMagick)");
        c->icon = QString(QChar(IconPicture));
        c->cmd  = "convert - -resize 92x92 png:-";
        c->input = "image/png";
        c->output = "image/png";
        c->inMenu = true;
    } else if (index == ++i) {
        c->name = tr("Create QR Code from URL (needs qrencode)");
        c->re   = reURL;
        c->icon = QString(QChar(IconQRCode));
        c->cmd  = "qrencode -o - -t PNG -s 6";
        c->input = mimeText;
        c->output = "image/png";
        c->inMenu = true;
    } else if (index == ++i) {
        c->name = tr("Add to &TODO tab");
        c->icon = QString(QChar(IconShare));
        c->tab  = "TODO";
        c->input = mimeText;
        c->inMenu = true;
    } else if (index == ++i) {
        c->name = tr("Move to &TODO tab");
        c->icon = QString(QChar(IconShare));
        c->tab  = "TODO";
        c->input = mimeText;
        c->remove = true;
        c->inMenu = true;
    } else if (index == ++i) {
        c->name = tr("Ignore copied files");
        c->icon = QString(QChar(IconExclamationSign));
        c->input = mimeUriList;
        c->remove = true;
        c->automatic = true;
#if defined(COPYQ_WS_X11) || defined(Q_OS_WIN) || defined(Q_OS_MAC)
    } else if (index == ++i) {
        c->name = tr("Ignore *\"Password\"* window");
        c->wndre = QRegExp(tr("Password"));
        c->icon = QString(QChar(IconAsterisk));
        c->remove = true;
        c->automatic = true;
        c->cmd = "copyq ignore";
#endif
    } else if (index == ++i) {
        c->name = tr("Encrypt (needs GnuPG)");
        c->icon = QString(QChar(IconLock));
        c->input = mimeItems;
        c->output = mimeEncryptedData;
        c->inMenu = true;
        c->transform = true;
        c->cmd = getEncryptCommand() + " --encrypt";
        c->shortcuts.append( toPortableShortcutText(tr("Ctrl+L")) );
    } else if (index == ++i) {
        c->name = tr("Decrypt");
        c->icon = QString(QChar(IconUnlock));
        c->input = mimeEncryptedData;
        c->output = mimeItems;
        c->inMenu = true;
        c->transform = true;
        c->cmd = getEncryptCommand() + " --decrypt";
        c->shortcuts.append( toPortableShortcutText(tr("Ctrl+L")) );
    } else if (index == ++i) {
        c->name = tr("Decrypt and Copy");
        c->icon = QString(QChar(IconUnlockAlt));
        c->input = mimeEncryptedData;
        c->inMenu = true;
        c->cmd = getEncryptCommand() + " --decrypt | copyq copy " + mimeItems + " -";
        c->shortcuts.append( toPortableShortcutText(tr("Ctrl+Shift+L")) );
    } else if (index == ++i) {
        c->name = tr("Move to Trash");
        c->icon = QString(QChar(IconTrash));
        c->inMenu = true;
        c->tab  = tr("(trash)");
        c->remove = true;
        c->shortcuts.append( toPortableShortcutText(shortcutToRemove()) );
    } else {
        return false;
    }

    return true;
}

QString CommandDialog::serializeSelectedCommands()
{
    const Commands commands = selectedCommands();
    if ( commands.isEmpty() )
        return QString();

    QTemporaryFile tmpfile;
    if ( !openTemporaryFile(&tmpfile) )
        return QString();

    if ( !tmpfile.open() )
        return QString();

    QSettings commandsSettings(tmpfile.fileName(), QSettings::IniFormat);
    saveCommands(commands, &commandsSettings);
    commandsSettings.sync();

    // Replace ugly '\n' with indented lines.
    const QString data = QString::fromUtf8(tmpfile.readAll());
    QString commandData;
    commandData.reserve(data.size());
    QRegExp re("^(\\d+\\\\)?Command=\"");

    foreach (const QString &line, data.split('\n')) {
        if (line.contains(re)) {
            int i = re.matchedLength();
            commandData.append(line.left(i) + "\n    ");
            bool escape = false;

            for (; i < line.size(); ++i) {
                const QChar c = line[i];

                if (escape) {
                    escape = false;

                    if (c == 'n') {
                        commandData.append("\n    ");
                    } else {
                        commandData.append('\\');
                        commandData.append(c);
                    }
                } else if (c == '\\') {
                    escape = !escape;
                } else {
                    commandData.append(c);
                }
            }
        } else {
            commandData.append(line);
        }

        commandData.append('\n');
    }

    return commandData;
}

CommandDialog::Commands loadCommands(bool onlyEnabled)
{
    QSettings settings;
    return loadCommands(&settings, onlyEnabled);
}

void saveCommands(const CommandDialog::Commands &commands)
{
    QSettings settings;
    saveCommands(commands, &settings);
}
