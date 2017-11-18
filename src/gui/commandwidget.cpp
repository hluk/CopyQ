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

const char globalShortcutsDisabled[] = "DISABLED";

const QIcon iconClipboard() { return getIcon("", IconPaste); }
const QIcon iconMenu() { return getIcon("", IconBars); }
const QIcon iconShortcut() { return getIcon("", IconKeyboard); }

QStringList serializeShortcuts(const QList<QKeySequence> &shortcuts, bool enabled = true)
{
    if ( shortcuts.isEmpty() )
        return QStringList();

    QStringList shortcutTexts;
    shortcutTexts.reserve( shortcuts.size() + (enabled ? 1 : 0) );

    for (const auto &shortcut : shortcuts)
        shortcutTexts.append(portableShortcutText(shortcut));

    if (!enabled)
        shortcutTexts.append(globalShortcutsDisabled);

    return shortcutTexts;
}

void deserializeShortcuts(
        const QStringList &serializedShortcuts, ShortcutButton *shortcutButton,
        QCheckBox *checkBoxEnabled = nullptr
        )
{
    shortcutButton->resetShortcuts();

    bool enabled = !serializedShortcuts.isEmpty();

    for (const auto &shortcutText : serializedShortcuts) {
        if (shortcutText == globalShortcutsDisabled)
            enabled = false;
        else
            shortcutButton->addShortcut(shortcutText);
    }

    if (checkBoxEnabled)
        checkBoxEnabled->setChecked(enabled);
}

} // namespace

CommandWidget::CommandWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::CommandWidget)
{
    ui->setupUi(this);

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
    c.inMenu  = ui->checkBoxInMenu->isChecked();
    c.transform = ui->checkBoxTransform->isChecked();
    c.remove = ui->checkBoxIgnore->isChecked();
    c.hideWindow = ui->checkBoxHideWindow->isChecked();
    c.enable = true;
    c.icon   = ui->buttonIcon->currentIcon();
    c.shortcuts = serializeShortcuts( ui->shortcutButton->shortcuts() );
    c.globalShortcuts = serializeShortcuts(
                ui->shortcutButtonGlobalShortcut->shortcuts(),
                ui->checkBoxGlobalShortcut->isChecked() );
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
    ui->checkBoxInMenu->setChecked(c.inMenu);
    ui->checkBoxTransform->setChecked(c.transform);
    ui->checkBoxIgnore->setChecked(c.remove);
    ui->checkBoxHideWindow->setChecked(c.hideWindow);
    ui->buttonIcon->setCurrentIcon(c.icon);
    deserializeShortcuts(c.shortcuts, ui->shortcutButton);
    deserializeShortcuts(
                c.globalShortcuts,
                ui->shortcutButtonGlobalShortcut,
                ui->checkBoxGlobalShortcut);
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

void CommandWidget::on_lineEditName_textChanged(const QString &text)
{
    emit nameChanged(text);
}

void CommandWidget::on_buttonIcon_currentIconChanged()
{
    emitIconChanged();
}

void CommandWidget::on_checkBoxShowAdvanced_stateChanged(int state)
{
    const bool showAdvanced = state == Qt::Checked;
    AppConfig appConfig;
    appConfig.setOption(Config::show_advanced_command_settings::name(), showAdvanced);
    ui->widgetAdvanced->setVisible(showAdvanced);
    updateWidgets();
}

void CommandWidget::on_checkBoxAutomatic_stateChanged(int)
{
    updateWidgets();
    emitIconChanged();
}

void CommandWidget::on_checkBoxInMenu_stateChanged(int)
{
    updateWidgets();
    emitIconChanged();
}

void CommandWidget::on_checkBoxGlobalShortcut_stateChanged(int)
{
    updateWidgets();
    emitIconChanged();
}

void CommandWidget::on_shortcutButtonGlobalShortcut_shortcutAdded(const QKeySequence &)
{
    updateWidgets();
}

void CommandWidget::on_shortcutButtonGlobalShortcut_shortcutRemoved(const QKeySequence &)
{
    updateWidgets();
}

void CommandWidget::on_commandEdit_changed()
{
    updateWidgets();
}

void CommandWidget::updateWidgets()
{
    bool inMenu = ui->checkBoxInMenu->isChecked();
    bool copyOrExecute = inMenu || ui->checkBoxAutomatic->isChecked();

    ui->widgetGlobalShortcut->setVisible(ui->checkBoxGlobalShortcut->isChecked());
    ui->widgetMenuShortcut->setVisible(inMenu);

    ui->groupBoxMatchItems->setVisible(copyOrExecute);
    ui->groupBoxAction->setVisible(copyOrExecute);
    ui->groupBoxInMenu->setVisible(inMenu);
    ui->groupBoxCommandOptions->setHidden(!copyOrExecute || ui->commandEdit->isEmpty());

    ui->widgetSpacer->setVisible(ui->widgetAdvanced->isHidden());

    ui->commandEdit->resizeToContent();
}

void CommandWidget::emitIconChanged()
{
    const auto tag =
            ui->checkBoxAutomatic->isChecked() ? CommandType::Automatic
          : ui->checkBoxGlobalShortcut->isChecked() ? CommandType::GlobalShortcut
          : ui->checkBoxInMenu->isChecked() && ui->shortcutButton->shortcutCount() > 0 ? CommandType::Shortcut
          : ui->checkBoxInMenu->isChecked() ? CommandType::Menu
          : CommandType::Script;

    emit iconChanged( ui->buttonIcon->currentIcon(), tag );
}
