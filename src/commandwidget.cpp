#include "commandwidget.h"
#include "ui_commandwidget.h"

#include "shortcutdialog.h"

#include <QFileDialog>
#include <QPicture>
#include <QDebug>

CommandWidget::CommandWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CommandWidget)
{
    ui->setupUi(this);
    setFocusProxy(ui->lineEditName);
}

CommandWidget::~CommandWidget()
{
    delete ui;
}

Command CommandWidget::command() const
{
    Command c;
    c.name = ui->lineEditName->text();
    c.re   = QRegExp( ui->lineEditMatch->text() );
    c.cmd  = ui->lineEditCommand->text();
    c.sep  = ui->lineEditSeparator->text();
    c.input  = ui->checkBoxInput->isChecked();
    c.output = ui->checkBoxOutput->isChecked();
    c.wait   = ui->checkBoxWait->isChecked();
    c.automatic = ui->checkBoxAutomatic->isChecked();
    c.ignore = ui->checkBoxIgnore->isChecked();
    c.enable = ui->checkBoxEnable->isChecked();
    c.icon = ui->lineEditIcon->text();
    c.shortcut = ui->pushButtonShortcut->text();
    c.tab = ui->comboBoxCopyToTab->currentText();
    c.outputTab = ui->comboBoxOutputTab->currentText();

    return c;
}

void CommandWidget::setCommand(const Command &c) const
{
    ui->lineEditName->setText(c.name);
    ui->lineEditMatch->setText( c.re.pattern() );
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
