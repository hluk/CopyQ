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

#include "commandwidget.h"
#include "ui_commandwidget.h"

#include "common/appconfig.h"
#include "common/command.h"
#include "common/mimetypes.h"
#include "common/shortcuts.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "gui/shortcutdialog.h"
#include "gui/tabicons.h"
#include "item/itemfactory.h"
#include "platform/platformnativeinterface.h"

#include <QAction>
#include <QFontMetrics>
#include <QMenu>

namespace {

const QIcon iconClipboard() { return getIcon("", IconClipboard); }
const QIcon iconMenu() { return getIcon("", IconBars); }
const QIcon iconShortcut() { return getIcon("", IconKeyboard); }
const QIcon iconScript() { return getIcon("", IconCog); }
const QIcon iconDisplay() { return getIcon("", IconEye); }

QStringList serializeShortcuts(const QList<QKeySequence> &shortcuts)
{
    if ( shortcuts.isEmpty() )
        return QStringList();

    QStringList shortcutTexts;
    shortcutTexts.reserve( shortcuts.size() );

    for (const auto &shortcut : shortcuts)
        shortcutTexts.append(portableShortcutText(shortcut));

    return shortcutTexts;
}

void deserializeShortcuts(
        const QStringList &serializedShortcuts, ShortcutButton *shortcutButton)
{
    shortcutButton->resetShortcuts();

    for (const auto &shortcutText : serializedShortcuts)
        shortcutButton->addShortcut(shortcutText);
}

} // namespace

CommandWidget::CommandWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::CommandWidget)
{
    ui->setupUi(this);

    connect(ui->lineEditName, &QLineEdit::textChanged,
            this, &CommandWidget::onLineEditNameTextChanged);
    connect(ui->buttonIcon, &IconSelectButton::currentIconChanged,
            this, &CommandWidget::onButtonIconCurrentIconChanged);
    connect(ui->checkBoxShowAdvanced, &QCheckBox::stateChanged,
            this, &CommandWidget::onCheckBoxShowAdvancedStateChanged);

    for (auto checkBox : findChildren<QCheckBox *>()) {
        connect(checkBox, &QCheckBox::stateChanged,
                this, &CommandWidget::updateWidgets);
    }

    for (auto lineEdit : findChildren<QLineEdit *>()) {
        connect(lineEdit, &QLineEdit::textEdited,
                this, &CommandWidget::updateWidgets);
    }

    connect(ui->shortcutButtonGlobalShortcut, &ShortcutButton::shortcutAdded,
            this, &CommandWidget::updateWidgets);
    connect(ui->shortcutButtonGlobalShortcut, &ShortcutButton::shortcutRemoved,
            this, &CommandWidget::updateWidgets);

    connect(ui->commandEdit, &CommandEdit::changed,
            this, &CommandWidget::updateWidgets);
    connect(ui->commandEdit, &CommandEdit::commandTextChanged,
            this, &CommandWidget::onCommandEditCommandTextChanged);

    updateWidgets();

#ifdef NO_GLOBAL_SHORTCUTS
    ui->checkBoxGlobalShortcut->hide();
    ui->shortcutButtonGlobalShortcut->hide();
#else
    ui->checkBoxGlobalShortcut->setIcon(iconShortcut());
#endif

    ui->checkBoxAutomatic->setIcon(iconClipboard());
    ui->checkBoxInMenu->setIcon(iconMenu());
    ui->checkBoxIsScript->setIcon(iconScript());
    ui->checkBoxDisplay->setIcon(iconDisplay());

    // Add tab names to combo boxes.
    initTabComboBox(ui->comboBoxCopyToTab);
    initTabComboBox(ui->comboBoxOutputTab);

    if ( !platformNativeInterface()->canGetWindowTitle() )
        ui->lineEditWindow->hide();
}

CommandWidget::~CommandWidget()
{
    delete ui;
}

Command CommandWidget::command() const
{
    Command c;
    c.name   = ui->lineEditName->text();
    c.re     = QRegularExpression( ui->lineEditMatch->text() );
    c.wndre  = QRegularExpression( ui->lineEditWindow->text() );
    c.matchCmd = ui->lineEditMatchCmd->command();
    c.cmd    = ui->commandEdit->command();
    c.sep    = ui->lineEditSeparator->text();
    c.input  = ui->comboBoxInputFormat->currentText();
    c.output = ui->comboBoxOutputFormat->currentText();
    c.wait   = ui->checkBoxWait->isChecked();
    c.automatic = ui->checkBoxAutomatic->isChecked();
    c.display = ui->checkBoxDisplay->isChecked();
    c.inMenu  = ui->checkBoxInMenu->isChecked();
    c.isGlobalShortcut  = ui->checkBoxGlobalShortcut->isChecked();
    c.isScript  = ui->checkBoxIsScript->isChecked();
    c.transform = ui->checkBoxTransform->isChecked();
    c.remove = ui->checkBoxIgnore->isChecked();
    c.hideWindow = ui->checkBoxHideWindow->isChecked();
    c.enable = true;
    c.icon   = ui->buttonIcon->currentIcon();
    c.shortcuts = serializeShortcuts( ui->shortcutButton->shortcuts() );
    c.globalShortcuts = serializeShortcuts( ui->shortcutButtonGlobalShortcut->shortcuts() );
    c.tab    = ui->comboBoxCopyToTab->currentText();
    c.outputTab = ui->comboBoxOutputTab->currentText();

    return c;
}

void CommandWidget::setCommand(const Command &c)
{
    ui->lineEditName->setText(c.name);
    ui->lineEditMatch->setText( c.re.pattern() );
    ui->lineEditWindow->setText( c.wndre.pattern() );
    ui->lineEditMatchCmd->setCommand(c.matchCmd);
    ui->commandEdit->setCommand(c.cmd);
    ui->lineEditSeparator->setText(c.sep);
    ui->comboBoxInputFormat->setEditText(c.input);
    ui->comboBoxOutputFormat->setEditText(c.output);
    ui->checkBoxWait->setChecked(c.wait);
    ui->checkBoxAutomatic->setChecked(c.automatic);
    ui->checkBoxDisplay->setChecked(c.display);
    ui->checkBoxInMenu->setChecked(c.inMenu);
    ui->checkBoxGlobalShortcut->setChecked(c.isGlobalShortcut);
    ui->checkBoxIsScript->setChecked(c.isScript);
    ui->checkBoxTransform->setChecked(c.transform);
    ui->checkBoxIgnore->setChecked(c.remove);
    ui->checkBoxHideWindow->setChecked(c.hideWindow);
    ui->buttonIcon->setCurrentIcon(c.icon);
    deserializeShortcuts(c.shortcuts, ui->shortcutButton);
    deserializeShortcuts(
                c.globalShortcuts,
                ui->shortcutButtonGlobalShortcut);
    ui->comboBoxCopyToTab->setEditText(c.tab);
    ui->comboBoxOutputTab->setEditText(c.outputTab);

    if (c.cmd.isEmpty())
        ui->tabWidget->setCurrentWidget(ui->tabAdvanced);
}

void CommandWidget::setFormats(const QStringList &formats)
{
    setComboBoxItems(ui->comboBoxInputFormat, formats);
    setComboBoxItems(ui->comboBoxOutputFormat, formats);
}

void CommandWidget::showEvent(QShowEvent *event)
{
    AppConfig appConfig;
    const bool showAdvanced = appConfig.option<Config::show_advanced_command_settings>();
    ui->checkBoxShowAdvanced->setChecked(showAdvanced);

    QWidget::showEvent(event);
}

void CommandWidget::onLineEditNameTextChanged(const QString &text)
{
    emit nameChanged(text);
}

void CommandWidget::onButtonIconCurrentIconChanged()
{
    emitIconChanged();
}

void CommandWidget::onCheckBoxShowAdvancedStateChanged(int state)
{
    const bool showAdvanced = state == Qt::Checked;
    AppConfig appConfig;
    appConfig.setOption(Config::show_advanced_command_settings::name(), showAdvanced);
    ui->tabWidget->setVisible(showAdvanced);
    ui->labelDescription->setVisible(showAdvanced);
}

void CommandWidget::onCommandEditCommandTextChanged(const QString &command)
{
    emit commandTextChanged(command);
}

void CommandWidget::updateWidgets()
{
    const bool isScript = ui->checkBoxIsScript->isChecked();
    const bool isAutomatic = !isScript
            && (ui->checkBoxAutomatic->isChecked() || ui->checkBoxDisplay->isChecked());
    const bool inMenu = !isScript && ui->checkBoxInMenu->isChecked();
    const bool isGlobalShortcut = !isScript && ui->checkBoxGlobalShortcut->isChecked();
    const bool copyOrExecute = inMenu || isAutomatic;

    ui->checkBoxAutomatic->setVisible(!isScript);
    ui->checkBoxDisplay->setVisible(!isScript);
    ui->checkBoxInMenu->setVisible(!isScript);
    ui->checkBoxGlobalShortcut->setVisible(!isScript);
    ui->checkBoxIsScript->setVisible(!isAutomatic && !inMenu && !isGlobalShortcut);

    ui->widgetGlobalShortcut->setVisible(isGlobalShortcut);
    ui->widgetMenuShortcut->setVisible(inMenu);

    ui->groupBoxMatchItems->setVisible(copyOrExecute);
    ui->groupBoxAction->setVisible(copyOrExecute);
    ui->groupBoxInMenu->setVisible(inMenu);
    ui->groupBoxCommandOptions->setHidden(!copyOrExecute || ui->commandEdit->isEmpty());

    ui->labelDescription->setText(description());

    emitIconChanged();
}

void CommandWidget::emitIconChanged()
{
    emit iconChanged();
}

QString CommandWidget::description() const
{
    const Command cmd = command();

    if (cmd.type() & CommandType::Script)
        return "<b>Extends scripting</b> or command line.";

    if (cmd.type() & CommandType::Display)
        return "Changes <b>visual</b> item representation.";

    QString description("<table><tr><td>");

    if (cmd.type() & CommandType::Automatic) {
        description.append("On <b>clipboard change</b>");
    } else if (cmd.type() & CommandType::GlobalShortcut) {
        description.append("On <b>global shortcut</b>");
    } else if (cmd.type() & CommandType::Menu) {
        description.append("On <b>menu item or application shortcut</b>");
    }

    if ( !(cmd.type() & CommandType::GlobalShortcut) ) {
        if (cmd.input.isEmpty() || cmd.input == mimeText)
            description.append( QString("<div><b>input format:</b> text</div>") );
        else if (cmd.input == "!OUTPUT")
            description.append( QString("<div><b>input format NOT:</b> %1</div>").arg(cmd.output) );
        else
            description.append( QString("<div><b>input format:</b> %1</div>").arg(cmd.input) );
    }

    description.append("</td><td width=15></td><td>");

    const bool isAutomaticOrMenu = cmd.type() & (CommandType::Automatic | CommandType::Menu);

    if ( !cmd.re.pattern().isEmpty() && isAutomaticOrMenu ) {
        description.append(
            QString("<div>if text matches <b>/%1/</b></div>").arg(cmd.re.pattern()) );
    }

    if ( !cmd.wndre.pattern().isEmpty() && cmd.type() & CommandType::Automatic ) {
        description.append(
            QString("<div>if current window title matches <b>/%1/</b></div>").arg(cmd.wndre.pattern()) );
    }

    if ( !cmd.matchCmd.isEmpty() && isAutomaticOrMenu )
        description.append("<div>if <b>filter</b> command succeeds</div>");

    description.append("</td><td width=15></td><td>");

    if (cmd.wait) {
        description.append("<div><b>shows action dialog</b></div>");
    } else if ( !cmd.cmd.isEmpty() && isAutomaticOrMenu ) {
        if ( !cmd.output.isEmpty() )
            description.append( QString("<div><b>output format:</b> %1</div>").arg(cmd.output) );
        if ( !cmd.outputTab.isEmpty() )
            description.append( QString("<div><b>output tab:</b> %1</div>").arg(cmd.outputTab) );
    }

    if ( !cmd.tab.isEmpty() && cmd.type() & CommandType::Automatic )
        description.append( QString("<div>saves clipboard in tab <b>%1</b></div>").arg(cmd.tab) );

    if (cmd.remove) {
        if ( cmd.type() & CommandType::Automatic )
            description.append("<div><b>ignores clipboard</b></div>");
        else if ( cmd.type() & CommandType::Menu )
            description.append("<div><b>removes</b> items</div>");
    } else if (cmd.transform) {
        if ( cmd.type() & CommandType::Automatic )
            description.append("<div><b>replaces data</b></div>");
        else if ( cmd.type() & CommandType::Menu )
            description.append("<div><b>replaces selected items</b></div>");
    }

    if (cmd.hideWindow && cmd.type() & CommandType::Menu )
        description.append("<br/><b>closes</b> main window");

    description.append("</td></tr></table>");

    return description;
}
