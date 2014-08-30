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

#include "commandwidget.h"
#include "ui_commandwidget.h"

#include "common/command.h"
#include "common/mimetypes.h"
#include "gui/commandsyntaxhighlighter.h"
#include "gui/configurationmanager.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "gui/shortcutdialog.h"
#include "item/itemfactory.h"

#include <QAction>
#include <QFontMetrics>
#include <QMenu>
#include <QSyntaxHighlighter>

namespace {

QStringList serializeShortcuts(const QList<QKeySequence> &shortcuts)
{
    QStringList shortcutTexts;

    foreach (const QKeySequence &shortcut, shortcuts)
        shortcutTexts.append(shortcut.toString(QKeySequence::PortableText));

    return shortcutTexts;
}

void deserializeShortcuts(const QStringList &serializedShortcuts, ShortcutButton *shortcutButton)
{
    shortcutButton->resetShortcuts();

    foreach (const QString &shortcutText, serializedShortcuts)
        shortcutButton->addShortcut(shortcutText);
}

void setComboBoxItems(const QStringList &items, QComboBox *w)
{
    const QString text = w->currentText();
    w->clear();
    w->addItem(QString());
    w->addItems(items);
    w->setEditText(text);
}

} // namespace

CommandWidget::CommandWidget(QWidget *parent)
    : QWidget(parent)
    , ui(NULL)
    , m_cmd()
{
}

CommandWidget::~CommandWidget()
{
    if (ui) {
        delete ui;
#if !defined(COPYQ_WS_X11) && !defined(Q_OS_WIN)
        ui->lineEditWindow->hide();
        ui->labelWindow->hide();
#endif
    }
}

Command CommandWidget::command() const
{
    if (!ui)
        return m_cmd;

    Command c;
    c.name   = ui->lineEditName->text();
    c.re     = QRegExp( ui->lineEditMatch->text() );
    c.wndre  = QRegExp( ui->lineEditWindow->text() );
    c.matchCmd = ui->lineEditMatchCmd->text();
    c.cmd    = ui->lineEditCommand->toPlainText();
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
    c.globalShortcuts = serializeShortcuts( ui->shortcutButtonGlobalShortcut->shortcuts() );
    c.tab    = ui->comboBoxCopyToTab->currentText();
    c.outputTab = ui->comboBoxOutputTab->currentText();

    return c;
}

void CommandWidget::setCommand(const Command &c)
{
    if (!ui) {
        m_cmd = c;
        return;
    }

    ui->lineEditName->setText(c.name);
    ui->lineEditMatch->setText( c.re.pattern() );
    ui->lineEditWindow->setText( c.wndre.pattern() );
    ui->lineEditMatchCmd->setText(c.matchCmd);
    ui->lineEditCommand->setPlainText(c.cmd);
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
    deserializeShortcuts(c.globalShortcuts, ui->shortcutButtonGlobalShortcut);
    ui->comboBoxCopyToTab->setEditText(c.tab);
    ui->comboBoxOutputTab->setEditText(c.outputTab);
}

QString CommandWidget::currentIcon() const
{
    return ui ? ui->buttonIcon->currentIcon() : m_cmd.icon;
}

void CommandWidget::updateIcons()
{
    if (!ui)
        return;

    ui->shortcutButtonGlobalShortcut->updateIcons();
    ui->shortcutButton->updateIcons();
}

void CommandWidget::showEvent(QShowEvent *event)
{
    init();
    QWidget::showEvent(event);
}

void CommandWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateCommandEditSize();
}

void CommandWidget::on_lineEditName_textChanged(const QString &name)
{
    emit nameChanged(name);
}

void CommandWidget::on_buttonIcon_currentIconChanged(const QString &iconString)
{
    emit iconChanged(iconString);
}

void CommandWidget::on_lineEditCommand_textChanged()
{
    updateWidgets();
    updateCommandEditSize();
}

void CommandWidget::on_checkBoxAutomatic_stateChanged(int)
{
    updateWidgets();
    emit automaticChanged(ui->checkBoxAutomatic->isChecked());
}

void CommandWidget::on_checkBoxInMenu_stateChanged(int)
{
    updateWidgets();
}

void CommandWidget::on_shortcutButtonGlobalShortcut_shortcutAdded(const QKeySequence &)
{
    updateWidgets();
}

void CommandWidget::on_shortcutButtonGlobalShortcut_shortcutRemoved(const QKeySequence &)
{
    updateWidgets();
}

void CommandWidget::init()
{
    if (ui)
        return;

    QScopedPointer<Ui::CommandWidget> uiGuard(new Ui::CommandWidget);
    uiGuard->setupUi(this);
    ui = uiGuard.take();

    updateWidgets();

    QFont font("Monospace");
    font.setStyleHint(QFont::TypeWriter);
    font.setPointSize(10);
    ui->lineEditCommand->document()->setDefaultFont(font);

    installCommandSyntaxHighlighter(ui->lineEditCommand);

#ifdef NO_GLOBAL_SHORTCUTS
    ui->shortcutButtonGlobalShortcut->hide();
    ui->labelGlobalShortcut->hide();
#else
    ui->shortcutButtonGlobalShortcut->setExpectModifier(true);
#endif

    ConfigurationManager *cm = ConfigurationManager::instance();

    // Add tab names to combo boxes.
    QStringList tabs = cm->value("tabs").toStringList();
    setComboBoxItems(tabs, ui->comboBoxCopyToTab);
    setComboBoxItems(tabs, ui->comboBoxOutputTab);

    // Add formats to combo boxex.
    QStringList formats = cm->itemFactory()->formatsToSave();
    formats.prepend(mimeText);
    formats.removeDuplicates();

    setComboBoxItems(formats, ui->comboBoxInputFormat);
    setComboBoxItems(formats, ui->comboBoxOutputFormat);

    setCommand(m_cmd);

    updateIcons();
}

void CommandWidget::updateWidgets()
{
    Q_ASSERT(ui);
    bool inMenu = ui->checkBoxInMenu->isChecked();
    bool copyOrExecute = inMenu || ui->checkBoxAutomatic->isChecked();
#ifdef NO_GLOBAL_SHORTCUTS
    bool globalShortcut = false;
#else
    bool globalShortcut = ui->shortcutButtonGlobalShortcut->shortcutCount() > 0;
#endif

    ui->groupBoxMatchItems->setVisible(copyOrExecute);
    ui->groupBoxCommand->setVisible(copyOrExecute || globalShortcut);
    ui->groupBoxAction->setVisible(copyOrExecute);
    ui->groupBoxInMenu->setVisible(inMenu);
    ui->groupBoxCommandOptions->setHidden(!copyOrExecute || ui->lineEditCommand->document()->characterCount() == 0);
}

void CommandWidget::updateCommandEditSize()
{
    if (!ui)
        return;

    const QFontMetrics fm( ui->lineEditCommand->document()->defaultFont() );
    const int lines = ui->lineEditCommand->document()->size().height() + 2;
    const int visibleLines = qBound(3, lines, 20);
    const int h = visibleLines * fm.lineSpacing();
    ui->lineEditCommand->setMinimumHeight(h);
}
