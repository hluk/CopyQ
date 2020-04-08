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

#include "gui/commanddialog.h"
#include "ui_commanddialog.h"

#include "common/command.h"
#include "common/commandstore.h"
#include "common/common.h"
#include "common/mimetypes.h"
#include "common/settings.h"
#include "common/textdata.h"
#include "gui/addcommanddialog.h"
#include "gui/commandwidget.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "platform/platformnativeinterface.h"

#include <QAction>
#include <QClipboard>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>

namespace {

const QIcon iconLoadCommands() { return getIcon("document-open", IconFolderOpen); }
const QIcon iconSaveCommands() { return getIcon("document-save", IconSave); }
const QIcon iconCopyCommands() { return getIcon("edit-copy", IconCopy); }
const QIcon iconPasteCommands() { return getIcon("edit-paste", IconPaste); }

QIcon getCommandIcon(const QString &iconString, int commandType)
{
    const auto icon =
            commandType & CommandType::Automatic ? IconClipboard
          : commandType & CommandType::GlobalShortcut ? IconKeyboard
          : commandType & CommandType::Script ? IconCog
          : commandType & CommandType::Display ? IconEye
          : commandType & CommandType::Menu ? IconBars
          : IconExclamationTriangle;
    const auto color =
            commandType & CommandType::Disabled ? QColor(Qt::lightGray)
          : commandType & CommandType::Automatic ? QColor(240,220,200)
          : commandType & CommandType::GlobalShortcut ? QColor(100,255,150)
          : commandType & CommandType::Script ? QColor(255,220,100)
          : commandType & CommandType::Display ? QColor(100,220,255)
          : commandType & CommandType::Menu ? QColor(100,220,255)
          : QColor(255,100,100);

    return iconFromFile(iconString, QString(QChar(icon)), color);
}

bool hasCommandsToPaste(const QString &text)
{
    return text.startsWith("[Command]") || text.startsWith("[Commands]");
}

QString commandsToPaste()
{
    const QMimeData *data = clipboardData();
    if (data && data->hasText()) {
        const QString text = data->text().trimmed();
        if (hasCommandsToPaste(text))
            return text;
    }

    return QString();
}

} // namespace

Q_DECLARE_METATYPE(Command)

class CommandItem final : public ItemOrderList::Item {
public:
    CommandItem(const Command &command, const QStringList &formats, CommandDialog *cmdDialog)
        : m_command(command)
        , m_cmdDialog(cmdDialog)
        , m_formats(formats)
    {
    }

    QVariant data() const override { return QVariant::fromValue(m_command); }

private:
    QWidget *createWidget(QWidget *parent) override
    {
        auto cmdWidget = new CommandWidget(parent);
        cmdWidget->setFormats(m_formats);
        cmdWidget->setCommand(m_command);

        QObject::connect( cmdWidget, &CommandWidget::iconChanged,
                          m_cmdDialog, &CommandDialog::onCurrentCommandWidgetIconChanged );
        QObject::connect( cmdWidget, &CommandWidget::nameChanged,
                          m_cmdDialog, &CommandDialog::onCurrentCommandWidgetNameChanged );
        QObject::connect( cmdWidget, &CommandWidget::commandTextChanged,
                          m_cmdDialog, &CommandDialog::onCommandTextChanged );

        return cmdWidget;
    }

    Command m_command;
    CommandDialog *m_cmdDialog;
    QStringList m_formats;
};

CommandDialog::CommandDialog(
        const Commands &pluginCommands, const QStringList &formats, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CommandDialog)
    , m_pluginCommands(pluginCommands)
    , m_formats(formats)
{
    ui->setupUi(this);

    connect(ui->itemOrderListCommands, &ItemOrderList::addButtonClicked,
            this, &CommandDialog::onItemOrderListCommandsAddButtonClicked);
    connect(ui->itemOrderListCommands, &ItemOrderList::itemSelectionChanged,
            this, &CommandDialog::onItemOrderListCommandsItemSelectionChanged);
    connect(ui->pushButtonLoadCommands, &QPushButton::clicked,
            this, &CommandDialog::onPushButtonLoadCommandsClicked);
    connect(ui->pushButtonSaveCommands, &QPushButton::clicked,
            this, &CommandDialog::onPushButtonSaveCommandsClicked);
    connect(ui->pushButtonCopyCommands, &QPushButton::clicked,
            this, &CommandDialog::onPushButtonCopyCommandsClicked);
    connect(ui->pushButtonPasteCommands, &QPushButton::clicked,
            this, &CommandDialog::onPushButtonPasteCommandsClicked);
    connect(ui->lineEditFilterCommands, &QLineEdit::textChanged,
            this, &CommandDialog::onLineEditFilterCommandsTextChanged);
    connect(ui->buttonBox, &QDialogButtonBox::clicked,
            this, &CommandDialog::onButtonBoxClicked);

    ui->pushButtonLoadCommands->setIcon(iconLoadCommands());
    ui->pushButtonSaveCommands->setIcon(iconSaveCommands());
    ui->pushButtonCopyCommands->setIcon(iconCopyCommands());
    ui->pushButtonPasteCommands->setIcon(iconPasteCommands());
    ui->itemOrderListCommands->setWiderIconsEnabled(true);
    ui->itemOrderListCommands->setEditable(true);
    ui->itemOrderListCommands->setItemsMovable(true);

    addCommandsWithoutSave(loadAllCommands(), -1);
    if ( ui->itemOrderListCommands->itemCount() != 0 )
        ui->itemOrderListCommands->setCurrentItem(0);

    auto act = new QAction(ui->itemOrderListCommands);
    ui->itemOrderListCommands->addAction(act);
    act->setShortcut(QKeySequence::Paste);
    connect(act, &QAction::triggered, this, &CommandDialog::tryPasteCommandFromClipboard);

    act = new QAction(ui->itemOrderListCommands);
    ui->itemOrderListCommands->addAction(act);
    act->setShortcut(QKeySequence::Copy);
    connect(act, &QAction::triggered, this, &CommandDialog::copySelectedCommandsToClipboard);

    ui->itemOrderListCommands->setDragAndDropValidator(QRegularExpression("\\[Commands?\\]"));
    connect( ui->itemOrderListCommands, &ItemOrderList::dropped,
             this, &CommandDialog::onCommandDropped );

    connect( ui->itemOrderListCommands, &ItemOrderList::itemCheckStateChanged,
             this, &CommandDialog::onCommandEnabledDisabled );

    connect(this, &QDialog::finished, this, &CommandDialog::onFinished);

    m_savedCommands = currentCommands();

    connect(QApplication::clipboard(), &QClipboard::dataChanged,
            this, &CommandDialog::onClipboardChanged);
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

void CommandDialog::onCommandEnabledDisabled(int row)
{
    updateIcon(row);
}

void CommandDialog::onCurrentCommandWidgetIconChanged()
{
    const auto row = ui->itemOrderListCommands->currentRow();
    updateIcon(row);
}

void CommandDialog::onCurrentCommandWidgetNameChanged(const QString &name)
{
    ui->itemOrderListCommands->setCurrentItemLabel(name);
}

void CommandDialog::onFinished(int result)
{
    if (result == QDialog::Accepted)
        apply();
}

void CommandDialog::onItemOrderListCommandsAddButtonClicked()
{
    AddCommandDialog addCommandDialog(m_pluginCommands, this);
    connect(&addCommandDialog, &AddCommandDialog::addCommands, this, &CommandDialog::onAddCommands);
    addCommandDialog.exec();
}

void CommandDialog::onItemOrderListCommandsItemSelectionChanged()
{
    bool hasSelection = !ui->itemOrderListCommands->selectedRows().isEmpty();
    ui->pushButtonSaveCommands->setEnabled(hasSelection);
    ui->pushButtonCopyCommands->setEnabled(hasSelection);
}

void CommandDialog::onPushButtonLoadCommandsClicked()
{
    const QStringList fileNames =
            QFileDialog::getOpenFileNames(this, tr("Open Files with Commands"),
                                          QString(), tr("Commands (*.ini);; CopyQ Configuration (copyq.conf copyq-*.conf)"));

    for (const auto &fileName : fileNames) {
        const auto commands = importCommandsFromFile(fileName);
        addCommandsWithoutSave(commands, -1);
    }
}

void CommandDialog::onPushButtonSaveCommandsClicked()
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

void CommandDialog::onPushButtonCopyCommandsClicked()
{
    copySelectedCommandsToClipboard();
}

void CommandDialog::onPushButtonPasteCommandsClicked()
{
    tryPasteCommandFromClipboard();
}

void CommandDialog::onLineEditFilterCommandsTextChanged(const QString &text)
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

void CommandDialog::onButtonBoxClicked(QAbstractButton *button)
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

void CommandDialog::onAddCommands(const QVector<Command> &commands)
{
    const int targetRow = qMax( 0, ui->itemOrderListCommands->currentRow() );
    addCommandsWithoutSave(commands, targetRow);
}

void CommandDialog::onClipboardChanged()
{
    ui->pushButtonPasteCommands->setEnabled(!commandsToPaste().isEmpty());
}

void CommandDialog::onCommandTextChanged(const QString &command)
{
    // Paste commands (starting with [Command] or [Commands]) correctly
    // even if mistakingly pasted into text edit widget.
    if ( hasCommandsToPaste(command) ) {
        const int row = ui->itemOrderListCommands->currentRow();
        ui->itemOrderListCommands->removeRow(row);
        onCommandDropped(command, row);
    }
}

Command CommandDialog::currentCommand(int row) const
{
    Command c;

    QWidget *w = ui->itemOrderListCommands->widget(row);
    if (w) {
        const CommandWidget *commandWidget = qobject_cast<const CommandWidget *>(w);
        Q_ASSERT(commandWidget);
        c = commandWidget->command();
    } else {
        c = ui->itemOrderListCommands->data(row).value<Command>();
    }

    c.enable = ui->itemOrderListCommands->isItemChecked(row);

    return c;
}

Commands CommandDialog::currentCommands() const
{
    Commands commands;
    commands.reserve( ui->itemOrderListCommands->itemCount() );

    for (int i = 0; i < ui->itemOrderListCommands->itemCount(); ++i)
        commands.append( currentCommand(i) );

    return commands;
}

void CommandDialog::addCommandsWithoutSave(const Commands &commands, int targetRow)
{
    const int count = ui->itemOrderListCommands->rowCount();
    int row = targetRow >= 0 ? targetRow : count;

    QList<int> rowsToSelect;
    rowsToSelect.reserve( commands.size() );

    for (auto &command : commands) {
        ItemOrderList::ItemPtr item(new CommandItem(command, m_formats, this));
        const auto icon = getCommandIcon(command.icon, command.type());
        const auto state = command.enable
                ? ItemOrderList::Checked
                : ItemOrderList::Unchecked;
        ui->itemOrderListCommands->insertItem(
                    command.name, icon, item, row, state);
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

void CommandDialog::updateIcon(int row)
{
    const auto command = currentCommand(row);
    const auto icon = getCommandIcon(command.icon, command.type());
    ui->itemOrderListCommands->setItemIcon(row, icon);
}
