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

#include "gui/commanddialog.h"
#include "ui_commanddialog.h"

#include "common/command.h"
#include "common/commandstore.h"
#include "common/common.h"
#include "common/config.h"
#include "common/mimetypes.h"
#include "common/settings.h"
#include "common/textdata.h"
#include "gui/addcommanddialog.h"
#include "gui/commandwidget.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "platform/platformnativeinterface.h"

#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>

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

    QVariant data() const override { return QVariant::fromValue(m_command); }

private:
    QWidget *createWidget(QWidget *parent) const override
    {
        auto cmdWidget = new CommandWidget(parent);
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

QIcon getCommandIcon(const QString &iconString)
{
    return iconFromFile(iconString);
}

bool hasCommandsToPaste(const QString &text)
{
    return text.startsWith("[Command]") || text.startsWith("[Commands]");
}

QString commandsToPaste()
{
    const QMimeData *data = clipboardData(QClipboard::Clipboard);
    if (data && data->hasText()) {
        const QString text = data->text().trimmed();
        if (hasCommandsToPaste(text))
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

    addCommandsWithoutSave(loadAllCommands(), -1);
    if ( ui->itemOrderListCommands->itemCount() != 0 )
        ui->itemOrderListCommands->setCurrentItem(0);

    auto act = new QAction(ui->itemOrderListCommands);
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

    m_savedCommands = currentCommands();

    connect(QApplication::clipboard(), SIGNAL(dataChanged()),
            this, SLOT(onClipboardChanged()));
    onClipboardChanged();
}

CommandDialog::~CommandDialog()
{
    delete ui;
}

void CommandDialog::addCommands(const Commands &commands)
{
    addCommandsWithoutSave(commands, 0);
}

void CommandDialog::apply()
{
    const auto cmds = currentCommands();
    saveCommands(cmds);
    m_savedCommands = cmds;
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
    const auto commands = importCommandsFromText(text);
    addCommandsWithoutSave(commands, row);
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
    auto addCommandDialog = new AddCommandDialog(m_pluginCommands, this);
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

    for (const auto &fileName : fileNames) {
        const auto commands = importCommandsFromFile(fileName);
        addCommandsWithoutSave(commands, -1);
    }
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
    // Omit pasting commands accidentally to filter text field.
    if (hasCommandsToPaste(text)) {
        ui->lineEditFilterCommands->clear();
        onCommandDropped( text, ui->itemOrderListCommands->currentRow() );
        return;
    }

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
    addCommandsWithoutSave(commands, targetRow);
}

void CommandDialog::onClipboardChanged()
{
    ui->pushButtonPasteCommands->setEnabled(!commandsToPaste().isEmpty());
}

Commands CommandDialog::currentCommands() const
{
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

        commands.append(c);
    }

    return commands;
}

void CommandDialog::addCommandsWithoutSave(const Commands &commands, int targetRow)
{
    QList<int> rowsToSelect;

    const int count = ui->itemOrderListCommands->rowCount();
    int row = targetRow >= 0 ? targetRow : count;

    for (auto &command : commands) {
        ItemOrderList::ItemPtr item(new CommandItem(command, m_formats, this));
        ui->itemOrderListCommands->insertItem(
                    command.name, command.enable, command.automatic,
                    getCommandIcon(command.icon), item, row);
        rowsToSelect.append(row);
        ++row;
    }

    ui->itemOrderListCommands->setSelectedRows(rowsToSelect);
}

Commands CommandDialog::selectedCommands() const
{
    const auto rows = ui->itemOrderListCommands->selectedRows();
    const auto cmds = currentCommands();
    Commands commandsToSave;
    for (int row : rows) {
        Q_ASSERT(row < cmds.size());
        commandsToSave.append(cmds.value(row));
    }

    return commandsToSave;
}

QString CommandDialog::serializeSelectedCommands()
{
    const Commands commands = selectedCommands();
    if ( commands.isEmpty() )
        return QString();

    return exportCommands(commands);
}

bool CommandDialog::hasUnsavedChanges() const
{
    return m_savedCommands != currentCommands();
}
