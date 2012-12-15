/*
    Copyright (c) 2012, Lukas Holecek <hluk@email.cz>

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

#include "shortcutdialog.h"

#include <QFileDialog>
#include <QPicture>

CommandWidget::CommandWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::CommandWidget)
{
    ui->setupUi(this);
    ui->lineEditIcon->hide();
    ui->checkBoxEnable->hide();
    setFocusProxy(ui->lineEditName);
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
    c.cmd    = ui->lineEditCommand->text();
    c.sep    = ui->lineEditSeparator->text();
    c.input  = ui->checkBoxInput->isChecked();
    c.output = ui->checkBoxOutput->isChecked();
    c.wait   = ui->checkBoxWait->isChecked();
    c.automatic = ui->checkBoxAutomatic->isChecked();
    c.ignore = ui->checkBoxIgnore->isChecked();
    c.enable = ui->checkBoxEnable->isChecked();
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
    ui->checkBoxInput->setChecked(c.input);
    ui->checkBoxOutput->setChecked(c.output);
    ui->checkBoxWait->setChecked(c.wait);
    ui->checkBoxAutomatic->setChecked(c.automatic);
    ui->checkBoxIgnore->setChecked(c.ignore);
    ui->checkBoxEnable->setChecked(c.enable);
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

void CommandWidget::on_pushButtonBrowse_clicked()
{
    QString filename = QFileDialog::getOpenFileName(
                this, tr("Open Icon file"), QString(),
                tr("Image Files (*.png *.jpg *.jpeg *.bmp *.ico *.svg)"));
    if ( !filename.isNull() )
        ui->lineEditIcon->setText(filename);
}

void CommandWidget::on_lineEditIcon_textChanged(const QString &)
{
    QImage image;
    image.load( ui->lineEditIcon->text() );
    QPixmap pix = QPixmap::fromImage(image.scaledToHeight(16));
    ui->labelIcon->setPixmap(pix);
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

void CommandWidget::on_lineEditCommand_textChanged(const QString &arg1)
{
    ui->groupBoxCommandOptions->setDisabled( arg1.isEmpty() );
}

void CommandWidget::on_checkBoxOutput_toggled(bool checked)
{
    ui->lineEditSeparator->setEnabled(checked);
    ui->separatorLabel->setEnabled(checked);
    ui->labelOutputTab->setEnabled(checked);
    ui->comboBoxOutputTab->setEnabled(checked);
}

void CommandWidget::setTabs(const QStringList &tabs, QComboBox *w)
{
    QString text = w->currentText();
    w->clear();
    w->addItem("");
    w->addItems(tabs);
    w->setEditText(text);
}
