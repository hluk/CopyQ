/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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
#include "gui/iconfactory.h"
#include "gui/shortcutdialog.h"

#include <QAction>
#include <QFileDialog>
#include <QFontMetrics>
#include <QMenu>

CommandWidget::CommandWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::CommandWidget)
{
    ui->setupUi(this);
    ui->lineEditIcon->hide();
    setFocusProxy(ui->lineEditName);

    IconFactory *factory = IconFactory::instance();
    if ( factory->isLoaded() ) {
        // iconic font
        const QFont &font = factory->iconFont();
        QFontMetrics fm(font);

        ui->buttonIcon->setFont( factory->iconFont() );

        // change icon
        QMenu *menu = new QMenu(this);
        menu->addAction( QString() );
        for (ushort i = IconFirst; i <= IconLast; ++i) {
            QChar c(i);
            if ( fm.inFont(c) ) {
                QAction *act = menu->addAction( QString(c) );
                act->setFont(font);
            }
        }
        ui->buttonIcon->setMenu(menu);
        connect( menu, SIGNAL(triggered(QAction*)),
                 this, SLOT(onIconChanged(QAction*)) );
    }

    updateWidgets();
}

CommandWidget::~CommandWidget()
{
    delete ui;
#if !defined(COPYQ_WS_X11) && !defined(Q_OS_WIN)
    ui->lineEditWindow->hide();
    ui->labelWindow->hide();
#endif
}

Command CommandWidget::command() const
{
    Command c;
    c.name   = ui->lineEditName->text();
    c.re     = QRegExp( ui->lineEditMatch->text() );
    c.wndre  = QRegExp( ui->lineEditWindow->text() );
    c.cmd    = ui->lineEditCommand->text();
    c.sep    = ui->lineEditSeparator->text();
    c.input  = ui->comboBoxInputFormat->currentText();
    c.output = ui->comboBoxOutputFormat->currentText();
    c.wait   = ui->checkBoxWait->isChecked();
    c.automatic = ui->checkBoxAutomatic->isChecked();
    c.inMenu   = ui->checkBoxInMenu->isChecked();
    c.transform = ui->checkBoxTransform->isChecked();
    c.remove = ui->checkBoxIgnore->isChecked();
    c.hideWindow = ui->checkBoxHideWindow->isChecked();
    c.enable = true;
    c.icon   = ui->lineEditIcon->text();
    c.shortcut = ui->pushButtonShortcut->text();
    c.tab    = ui->comboBoxCopyToTab->currentText();
    c.outputTab = ui->comboBoxOutputTab->currentText();

    return c;
}

void CommandWidget::setCommand(const Command &c)
{
    ui->lineEditName->setText(c.name);
    ui->lineEditMatch->setText( c.re.pattern() );
    ui->lineEditWindow->setText( c.wndre.pattern() );
    ui->lineEditCommand->setText(c.cmd);
    ui->lineEditSeparator->setText(c.sep);
    ui->comboBoxInputFormat->setEditText(c.input);
    ui->comboBoxOutputFormat->setEditText(c.output);
    ui->checkBoxWait->setChecked(c.wait);
    ui->checkBoxAutomatic->setChecked(c.automatic);
    ui->checkBoxInMenu->setChecked(c.inMenu);
    ui->checkBoxTransform->setChecked(c.transform);
    ui->checkBoxIgnore->setChecked(c.remove);
    ui->checkBoxHideWindow->setChecked(c.hideWindow);
    ui->lineEditIcon->setText(c.icon);
    ui->pushButtonShortcut->setText(c.shortcut);
    ui->comboBoxCopyToTab->setEditText(c.tab);
    ui->comboBoxOutputTab->setEditText(c.outputTab);
}

void CommandWidget::setTabs(const QStringList &tabs)
{
    setTabs(tabs, ui->comboBoxCopyToTab);
    setTabs(tabs, ui->comboBoxOutputTab);
}

void CommandWidget::setFormats(const QStringList &formats)
{
    QString text = ui->comboBoxInputFormat->currentText();
    ui->comboBoxInputFormat->clear();
    ui->comboBoxInputFormat->addItems(formats);
    ui->comboBoxInputFormat->setEditText(text);

    text = ui->comboBoxOutputFormat->currentText();
    ui->comboBoxOutputFormat->clear();
    ui->comboBoxOutputFormat->addItems(formats);
    ui->comboBoxOutputFormat->setEditText(text);
}

QIcon CommandWidget::icon() const
{
   return ui->buttonIcon->icon();
}

void CommandWidget::on_pushButtonBrowse_clicked()
{
    QString filename = QFileDialog::getOpenFileName(
                this, tr("Open Icon file"), QString(),
                tr("Image Files (*.png *.jpg *.jpeg *.bmp *.ico *.svg)"));
    if ( !filename.isNull() )
        ui->lineEditIcon->setText(filename);
}

void CommandWidget::on_lineEditName_textChanged(const QString &name)
{
    emit nameChanged(name);
}

void CommandWidget::on_lineEditIcon_textChanged(const QString &)
{
    QIcon icon =
            IconFactory::iconFromFile(ui->lineEditIcon->text(), getDefaultIconColor<QToolButton>());
    ui->buttonIcon->setIcon(icon);
    emit iconChanged(icon);
}

void CommandWidget::on_pushButtonShortcut_clicked()
{
    ShortcutDialog *dialog = new ShortcutDialog(this);
    if (dialog->exec() == QDialog::Accepted) {
        QKeySequence shortcut = dialog->shortcut();
        QString text;
        if ( !shortcut.isEmpty() )
            text = shortcut.toString(QKeySequence::NativeText);
        ui->pushButtonShortcut->setText(text);
    }
}

void CommandWidget::on_lineEditCommand_textChanged(const QString &)
{
    updateWidgets();
}

void CommandWidget::on_checkBoxAutomatic_stateChanged(int)
{
    updateWidgets();
}

void CommandWidget::on_checkBoxInMenu_stateChanged(int)
{
    updateWidgets();
}

void CommandWidget::onIconChanged(QAction *action)
{
    ui->lineEditIcon->setText(action->text());
}

void CommandWidget::setTabs(const QStringList &tabs, QComboBox *w)
{
    QString text = w->currentText();
    w->clear();
    w->addItem("");
    w->addItems(tabs);
    w->setEditText(text);
}

void CommandWidget::updateWidgets()
{
    bool inMenu = ui->checkBoxInMenu->isChecked();
    bool copyOrExecute = inMenu || ui->checkBoxAutomatic->isChecked();
    ui->groupBoxAction->setVisible(copyOrExecute);
    ui->groupBoxInMenu->setVisible(inMenu);
    ui->groupBoxCommandOptions->setHidden(!copyOrExecute || ui->lineEditCommand->text().isEmpty());
}
