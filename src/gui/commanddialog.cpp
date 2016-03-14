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

#include "gui/commanddialog.h"
#include "ui_commanddialog.h"

#include "common/command.h"
#include "common/common.h"
#include "common/config.h"
#include "common/mimetypes.h"
#include "common/settings.h"
#include "gui/addcommanddialog.h"
#include "gui/commandwidget.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "platform/platformnativeinterface.h"

#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QSettings>
#include <QTemporaryFile>

namespace {

const QIcon iconLoadCommands() { return getIcon("document-open", IconFolderOpen); }
const QIcon iconSaveCommands() { return getIcon("document-save", IconSave); }
const QIcon iconCopyCommands() { return getIcon("edit-copy", IconCopy); }
const QIcon iconPasteCommands() { return getIcon("edit-paste", IconPaste); }

class CommandItem : public ItemOrderList::Item {
public:
    CommandItem(const Command &command, const QStringList &formats, CommandDialog *cmdDialog)
        : m_command(command)
        , m_cmdDialog(cmdDialog)
        , m_formats(formats)
    {
    }

    QVariant data() const { return QVariant::fromValue(m_command); }

private:
    QWidget *createWidget(QWidget *parent) const
    {
        CommandWidget *cmdWidget = new CommandWidget(parent);
        cmdWidget->setFormats(m_formats);
        cmdWidget->setCommand(m_command);

        QObject::connect( cmdWidget, SIGNAL(iconChanged(QString)),
                          m_cmdDialog, SLOT(onCurrentCommandWidgetIconChanged(QString)) );
        QObject::connect( cmdWidget, SIGNAL(nameChanged(QString)),
                          m_cmdDialog, SLOT(onCurrentCommandWidgetNameChanged(QString)) );
        QObject::connect( cmdWidget, SIGNAL(automaticChanged(bool)),
                          m_cmdDialog, SLOT(onCurrentCommandWidgetAutomaticChanged(bool)) );

        return cmdWidget;
    }

    Command m_command;
    CommandDialog *m_cmdDialog;
    QStringList m_formats;
};

void loadCommand(const QSettings &settings, bool onlyEnabled, CommandDialog::Commands *commands)
{
    Command c;
    c.enable = settings.value("Enable", true).toBool();

    if (onlyEnabled && !c.enable)
        return;

    c.name = settings.value("Name").toString();
    c.re   = QRegExp( settings.value("Match").toString() );
    c.wndre = QRegExp( settings.value("Window").toString() );
    c.matchCmd = settings.value("MatchCommand").toString();
    c.cmd = settings.value("Command").toString();
    c.sep = settings.value("Separator").toString();

    c.input = settings.value("Input").toString();
    if ( c.input == "false" || c.input == "true" )
        c.input = c.input == "true" ? QString(mimeText) : QString();

    c.output = settings.value("Output").toString();
    if ( c.output == "false" || c.output == "true" )
        c.output = c.output == "true" ? QString(mimeText) : QString();

    c.wait = settings.value("Wait").toBool();
    c.automatic = settings.value("Automatic").toBool();
    c.transform = settings.value("Transform").toBool();
    c.hideWindow = settings.value("HideWindow").toBool();
    c.icon = settings.value("Icon").toString();
    c.shortcuts = settings.value("Shortcut").toStringList();
    c.globalShortcuts = settings.value("GlobalShortcut").toStringList();
    c.tab = settings.value("Tab").toString();
    c.outputTab = settings.value("OutputTab").toString();
    c.inMenu = settings.value("InMenu").toBool();

    if (c.globalShortcuts.size() == 1 && c.globalShortcuts[0] == "DISABLED")
        c.globalShortcuts = QStringList();

    if (settings.value("Ignore").toBool())
        c.remove = c.automatic = true;
    else
        c.remove = settings.value("Remove").toBool();

    commands->append(c);
}

CommandDialog::Commands loadCommands(QSettings *settings, bool onlyEnabled = false)
{
    CommandDialog::Commands commands;

    const QStringList groups = settings->childGroups();

    if ( groups.contains("Command") ) {
        settings->beginGroup("Command");
        loadCommand(*settings, onlyEnabled, &commands);
        settings->endGroup();
    }

    int size = settings->beginReadArray("Commands");

    for(int i=0; i<size; ++i) {
        settings->setArrayIndex(i);
        loadCommand(*settings, onlyEnabled, &commands);
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

QIcon getCommandIcon(const QString &iconString)
{
    return iconFromFile(iconString);
}

QString commandsToPaste()
{
    const QMimeData *data = clipboardData(QClipboard::Clipboard);
    if (data && data->hasText()) {
        const QString text = data->text().trimmed();
        if (text.startsWith("[Command]") || text.startsWith("[Commands]"))
            return text;
    }

    return QString();
}

} // namespace

Q_DECLARE_METATYPE(Command)

CommandDialog::CommandDialog(
        const Commands &pluginCommands, const QStringList &formats, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CommandDialog)
    , m_pluginCommands(pluginCommands)
    , m_formats(formats)
{
    ui->setupUi(this);
    ui->pushButtonLoadCommands->setIcon(iconLoadCommands());
    ui->pushButtonSaveCommands->setIcon(iconSaveCommands());
    ui->pushButtonCopyCommands->setIcon(iconCopyCommands());
    ui->pushButtonPasteCommands->setIcon(iconPasteCommands());
    ui->itemOrderListCommands->setFocus();
    ui->itemOrderListCommands->setAddRemoveButtonsVisible(true);

    foreach ( const Command &command, commands(false) )
        addCommandWithoutSave(command);

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

    m_savedCommands = commands(false, true);

    connect(QApplication::clipboard(), SIGNAL(dataChanged()),
            this, SLOT(onClipboardChanged()));
    onClipboardChanged();
}

CommandDialog::~CommandDialog()
{
    delete ui;
}

CommandDialog::Commands CommandDialog::commands(bool onlyEnabled, bool onlySaved) const
{
    if (!onlySaved) {
        Commands commands;

        for (int i = 0; i < ui->itemOrderListCommands->itemCount(); ++i) {
            Command c;

            QWidget *w = ui->itemOrderListCommands->widget(i);
            if (w) {
                const CommandWidget *commandWidget = qobject_cast<const CommandWidget *>(w);
                Q_ASSERT(commandWidget);
                c = commandWidget->command();
            } else {
                c = ui->itemOrderListCommands->data(i).value<Command>();
            }

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
}

void CommandDialog::apply()
{
    const Commands cmds = commands(false, false);
    saveCommands(cmds);
    m_savedCommands = commands(false, true);
    emit commandsSaved();
}

bool CommandDialog::maybeClose(QWidget *saveMessageBoxParent)
{
    if ( hasUnsavedChanges() ) {
        const QMessageBox::StandardButton button = QMessageBox::warning(
                    saveMessageBoxParent,
                    tr("Unsaved Changes"), tr("Command dialog has unsaved changes."),
                    QMessageBox::Save |  QMessageBox::Discard | QMessageBox::Cancel);

        if (button == QMessageBox::Cancel)
            return false;

        if (button == QMessageBox::Save)
            apply();
    }

    QDialog::reject();
    return true;
}

void CommandDialog::reject()
{
    maybeClose(this);
}

void CommandDialog::tryPasteCommandFromClipboard()
{
    const QString text = commandsToPaste();
    if (!text.isEmpty())
        onCommandDropped( text, ui->itemOrderListCommands->currentRow() );
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

void CommandDialog::on_itemOrderListCommands_addButtonClicked()
{
    AddCommandDialog *addCommandDialog = new AddCommandDialog(m_pluginCommands, this);
    addCommandDialog->setAttribute(Qt::WA_DeleteOnClose, true);
    connect(addCommandDialog, SIGNAL(addCommands(QList<Command>)), SLOT(onAddCommands(QList<Command>)));
    addCommandDialog->show();
}

void CommandDialog::on_itemOrderListCommands_itemSelectionChanged()
{
    bool hasSelection = !ui->itemOrderListCommands->selectedRows().isEmpty();
    ui->pushButtonSaveCommands->setEnabled(hasSelection);
    ui->pushButtonCopyCommands->setEnabled(hasSelection);
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

void CommandDialog::on_pushButtonCopyCommands_clicked()
{
    copySelectedCommandsToClipboard();
}

void CommandDialog::on_pushButtonPasteCommands_clicked()
{
    tryPasteCommandFromClipboard();
}

void CommandDialog::on_lineEditFilterCommands_textChanged(const QString &text)
{
    for (int i = 0; i < ui->itemOrderListCommands->itemCount(); ++i) {
        const Command c = ui->itemOrderListCommands->data(i).value<Command>();
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
    // Accept and reject roles are handled automatically.
    case QDialogButtonBox::AcceptRole:
        break;
    case QDialogButtonBox::RejectRole:
        break;
    default:
        break;
    }
}

void CommandDialog::onAddCommands(const QList<Command> &commands)
{
    const int targetRow = qMax( 0, ui->itemOrderListCommands->currentRow() );
    for (int i = 0; i < commands.count(); ++i)
        addCommandWithoutSave(commands[i], targetRow + i);
    ui->itemOrderListCommands->setCurrentItem(targetRow);
}

void CommandDialog::onClipboardChanged()
{
    ui->pushButtonPasteCommands->setEnabled(!commandsToPaste().isEmpty());
}

void CommandDialog::addCommandWithoutSave(const Command &command, int targetRow)
{
    ItemOrderList::ItemPtr item(new CommandItem(command, m_formats, this));
    ui->itemOrderListCommands->insertItem(
                command.name, command.enable, command.automatic,
                getCommandIcon(command.icon), item, targetRow);
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
    const QString data = getTextData(readTemporaryFileContent(tmpfile));
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

bool CommandDialog::hasUnsavedChanges() const
{
    return m_savedCommands != commands(false, false);
}

CommandDialog::Commands loadCommands(bool onlyEnabled)
{
    QSettings settings;
    return loadCommands(&settings, onlyEnabled);
}

void saveCommands(const CommandDialog::Commands &commands)
{
    Settings settings;
    saveCommands(commands, settings.settingsData());
}
