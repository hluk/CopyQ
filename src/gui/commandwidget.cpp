// SPDX-License-Identifier: GPL-3.0-or-later

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
const QIcon iconScript() { return getIcon("", IconGear); }
const QIcon iconDisplay() { return getIcon("", IconEye); }
#ifdef COPYQ_GLOBAL_SHORTCUTS
const QIcon iconShortcut() { return getIcon("", IconKeyboard); }
#endif

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

    for (auto button : findChildren<QToolButton *>()) {
        connect(button, &QToolButton::toggled,
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

#ifdef COPYQ_GLOBAL_SHORTCUTS
    ui->toolButtonGlobalShortcut->setIcon(iconShortcut());
#else
    ui->toolButtonGlobalShortcut->hide();
    ui->shortcutButtonGlobalShortcut->hide();
#endif

    ui->toolButtonAutomatic->setIcon(iconClipboard());
    ui->toolButtonInMenu->setIcon(iconMenu());
    ui->toolButtonIsScript->setIcon(iconScript());
    ui->toolButtonDisplay->setIcon(iconDisplay());

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
    c.automatic = ui->toolButtonAutomatic->isChecked();
    c.display = ui->toolButtonDisplay->isChecked();
    c.inMenu  = ui->toolButtonInMenu->isChecked();
    c.isGlobalShortcut  = ui->toolButtonGlobalShortcut->isChecked();
    c.isScript  = ui->toolButtonIsScript->isChecked();
    c.transform = ui->checkBoxTransform->isChecked();
    c.remove = ui->checkBoxIgnore->isChecked();
    c.hideWindow = ui->checkBoxHideWindow->isChecked();
    c.enable = true;
    c.icon   = ui->buttonIcon->currentIcon();
    c.shortcuts = serializeShortcuts( ui->shortcutButton->shortcuts() );
    c.globalShortcuts = serializeShortcuts( ui->shortcutButtonGlobalShortcut->shortcuts() );
    c.tab    = ui->comboBoxCopyToTab->currentText();
    c.outputTab = ui->comboBoxOutputTab->currentText();
    c.internalId = m_internalId;

    return c;
}

void CommandWidget::setCommand(const Command &c)
{
    m_internalId = c.internalId;
    const bool isEditable = !m_internalId.startsWith(QLatin1String("copyq_"));

    ui->scrollAreaWidgetContents->setEnabled(isEditable);
    ui->commandEdit->setReadOnly(!isEditable);
    ui->lineEditName->setReadOnly(!isEditable);
    ui->lineEditName->setText(c.name);
    ui->lineEditMatch->setText( c.re.pattern() );
    ui->lineEditWindow->setText( c.wndre.pattern() );
    ui->lineEditMatchCmd->setCommand(c.matchCmd);
    ui->commandEdit->setCommand(c.cmd);
    ui->lineEditSeparator->setText(c.sep);
    ui->comboBoxInputFormat->setEditText(c.input);
    ui->comboBoxOutputFormat->setEditText(c.output);
    ui->checkBoxWait->setChecked(c.wait);
    ui->toolButtonAutomatic->setChecked(c.automatic);
    ui->toolButtonDisplay->setChecked(c.display);
    ui->toolButtonInMenu->setChecked(c.inMenu);
    ui->toolButtonGlobalShortcut->setChecked(c.isGlobalShortcut);
    ui->toolButtonIsScript->setChecked(c.isScript);
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
    const AppConfig appConfig;
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
    setShowAdvanced(state == Qt::Checked);
}

void CommandWidget::onCommandEditCommandTextChanged(const QString &command)
{
    emit commandTextChanged(command);
}

void CommandWidget::updateWidgets()
{
    const bool isScript = ui->toolButtonIsScript->isChecked();
    const bool isAutomatic = !isScript
            && (ui->toolButtonAutomatic->isChecked() || ui->toolButtonDisplay->isChecked());
    const bool inMenu = !isScript && ui->toolButtonInMenu->isChecked();
    const bool isGlobalShortcut = !isScript && ui->toolButtonGlobalShortcut->isChecked();
    const bool copyOrExecute = inMenu || isAutomatic;

    ui->toolButtonAutomatic->setVisible(!isScript);
    ui->toolButtonDisplay->setVisible(!isScript);
    ui->toolButtonInMenu->setVisible(!isScript);
    ui->toolButtonGlobalShortcut->setVisible(!isScript);
    ui->toolButtonIsScript->setVisible(!isAutomatic && !inMenu && !isGlobalShortcut);

    ui->widgetGlobalShortcut->setVisible(isGlobalShortcut);
    ui->widgetMenuShortcut->setVisible(inMenu);

    ui->groupBoxMatchItems->setVisible(copyOrExecute);
    ui->groupBoxAction->setVisible(copyOrExecute);
    ui->groupBoxInMenu->setVisible(inMenu);
    ui->groupBoxCommandOptions->setHidden(!copyOrExecute || ui->commandEdit->isEmpty());

    ui->labelDescription->setText(description());

    updateShowAdvanced();

    emitIconChanged();
}

void CommandWidget::updateShowAdvanced()
{
    ui->widgetCommandType->setVisible(m_showAdvanced);
    ui->tabWidget->setVisible(m_showAdvanced);
    ui->labelDescription->setVisible(m_showAdvanced);

    // Hide the Advanced tab if there are no visible widgets.
    const auto advancedTabWidgets = ui->tabAdvanced->findChildren<QGroupBox*>();
    const auto showAdvancedTab = std::any_of(
        std::begin(advancedTabWidgets), std::end(advancedTabWidgets), [](const QGroupBox *w) {
            return !w->isHidden();
        });
    ui->tabWidget->setTabEnabled(1, showAdvancedTab);
}

void CommandWidget::setShowAdvanced(bool showAdvanced)
{
    if (m_showAdvanced == showAdvanced)
        return;

    m_showAdvanced = showAdvanced;
    AppConfig appConfig;
    appConfig.setOption(Config::show_advanced_command_settings::name(), showAdvanced);
    updateShowAdvanced();
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
            description.append( QLatin1String("<div><b>input format:</b> text</div>") );
        else if (cmd.input == "!OUTPUT")
            description.append( QString::fromLatin1("<div><b>input format NOT:</b> %1</div>").arg(cmd.output) );
        else
            description.append( QString::fromLatin1("<div><b>input format:</b> %1</div>").arg(cmd.input) );
    }

    description.append("</td><td width=15></td><td>");

    const bool isAutomaticOrMenu = cmd.type() & (CommandType::Automatic | CommandType::Menu);

    if ( !cmd.re.pattern().isEmpty() && isAutomaticOrMenu ) {
        description.append(
            QString::fromLatin1("<div>if text matches <b>/%1/</b></div>").arg(cmd.re.pattern()) );
    }

    if ( !cmd.wndre.pattern().isEmpty() && cmd.type() & CommandType::Automatic ) {
        description.append(
            QString::fromLatin1("<div>if current window title matches <b>/%1/</b></div>").arg(cmd.wndre.pattern()) );
    }

    if ( !cmd.matchCmd.isEmpty() && isAutomaticOrMenu )
        description.append("<div>if <b>filter</b> command succeeds</div>");

    description.append("</td><td width=15></td><td>");

    if (cmd.wait) {
        description.append("<div><b>shows action dialog</b></div>");
    } else if ( !cmd.cmd.isEmpty() && isAutomaticOrMenu ) {
        if ( !cmd.output.isEmpty() )
            description.append( QString::fromLatin1("<div><b>output format:</b> %1</div>").arg(cmd.output) );
        if ( !cmd.outputTab.isEmpty() )
            description.append( QString::fromLatin1("<div><b>output tab:</b> %1</div>").arg(cmd.outputTab) );
    }

    if ( !cmd.tab.isEmpty() && cmd.type() & CommandType::Automatic )
        description.append( QString::fromLatin1("<div>saves clipboard in tab <b>%1</b></div>").arg(cmd.tab) );

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
