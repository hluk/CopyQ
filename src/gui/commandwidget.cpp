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

    connect(ui->checkBoxAutomatic, &QCheckBox::stateChanged,
            this, &CommandWidget::onCheckBoxAutomaticStateChanged);
    connect(ui->checkBoxDisplay, &QCheckBox::stateChanged,
            this, &CommandWidget::onCheckBoxDisplayStateChanged);
    connect(ui->checkBoxInMenu, &QCheckBox::stateChanged,
            this, &CommandWidget::onCheckBoxInMenuStateChanged);
    connect(ui->checkBoxIsScript, &QCheckBox::stateChanged,
            this, &CommandWidget::onCheckBoxIsScriptStateChanged);
    connect(ui->checkBoxGlobalShortcut, &QCheckBox::stateChanged,
            this, &CommandWidget::onCheckBoxGlobalShortcutStateChanged);

    connect(ui->shortcutButtonGlobalShortcut, &ShortcutButton::shortcutAdded,
            this, &CommandWidget::onShortcutButtonGlobalShortcutShortcutAdded);
    connect(ui->shortcutButtonGlobalShortcut, &ShortcutButton::shortcutRemoved,
            this, &CommandWidget::onShortcutButtonGlobalShortcutShortcutRemoved);

    connect(ui->commandEdit, &CommandEdit::changed,
            this, &CommandWidget::onCommandEditChanged);
    connect(ui->commandEdit, &CommandEdit::commandTextChanged,
            this, &CommandWidget::onCommandEditCommandTextChanged);

    ui->widgetAdvanced->hide();

    updateWidgets();

#ifdef NO_GLOBAL_SHORTCUTS
    ui->checkBoxGlobalShortcut->hide();
    ui->shortcutButtonGlobalShortcut->hide();
#else
    ui->checkBoxGlobalShortcut->setIcon(iconShortcut());
#endif

    ui->groupBoxCommand->setFocusProxy(ui->commandEdit);

    ui->checkBoxAutomatic->setIcon(iconClipboard());
    ui->checkBoxInMenu->setIcon(iconMenu());
    ui->checkBoxIsScript->setIcon(iconScript());
    ui->checkBoxDisplay->setIcon(iconDisplay());

    // Add tab names to combo boxes.
    initTabComboBox(ui->comboBoxCopyToTab);
    initTabComboBox(ui->comboBoxOutputTab);

    if ( !createPlatformNativeInterface()->canGetWindowTitle() )
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
    c.re     = QRegExp( ui->lineEditMatch->text() );
    c.wndre  = QRegExp( ui->lineEditWindow->text() );
    c.matchCmd = ui->lineEditMatchCmd->text();
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
    ui->lineEditMatchCmd->setText(c.matchCmd);
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
}

void CommandWidget::setFormats(const QStringList &formats)
{
    setComboBoxItems(ui->comboBoxInputFormat, formats);
    setComboBoxItems(ui->comboBoxOutputFormat, formats);
}

void CommandWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    ui->commandEdit->resizeToContent();
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
    ui->widgetAdvanced->setVisible(showAdvanced);
    updateWidgets();
}

void CommandWidget::onCheckBoxAutomaticStateChanged(int)
{
    updateWidgets();
}

void CommandWidget::onCheckBoxDisplayStateChanged(int)
{
    updateWidgets();
}

void CommandWidget::onCheckBoxInMenuStateChanged(int)
{
    updateWidgets();
}

void CommandWidget::onCheckBoxIsScriptStateChanged(int)
{
    updateWidgets();
}

void CommandWidget::onCheckBoxGlobalShortcutStateChanged(int)
{
    updateWidgets();
}

void CommandWidget::onShortcutButtonGlobalShortcutShortcutAdded(const QKeySequence &)
{
    updateWidgets();
}

void CommandWidget::onShortcutButtonGlobalShortcutShortcutRemoved(const QKeySequence &)
{
    updateWidgets();
}

void CommandWidget::onCommandEditChanged()
{
    updateWidgets();
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

    ui->widgetSpacer->setVisible(ui->widgetAdvanced->isHidden());

    ui->commandEdit->resizeToContent();

    emitIconChanged();
}

void CommandWidget::emitIconChanged()
{
    emit iconChanged();
}
