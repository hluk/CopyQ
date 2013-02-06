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

#include "actiondialog.h"
#include "ui_actiondialog.h"

#include "action.h"
#include "client_server.h"
#include "command.h"
#include "configurationmanager.h"

#include <QSettings>
#include <QFile>
#include <QMessageBox>
#include <QMimeData>

const QStringList standardFormats = QStringList() << QString() << QString("text/plain");

ActionDialog::ActionDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ActionDialog)
    , m_re()
    , m_data(NULL)
{
    ui->setupUi(this);

    on_comboBoxInputFormat_currentIndexChanged(QString());
    on_comboBoxOutputFormat_editTextChanged(QString());
    loadSettings();
}

ActionDialog::~ActionDialog()
{
    delete m_data;
    delete ui;
}

void ActionDialog::setInputData(const QMimeData &data)
{
    delete m_data;
    m_data = cloneData(data);

    QString defaultFormat = ui->comboBoxInputFormat->currentText();
    ui->comboBoxInputFormat->clear();
    QStringList formats = QStringList() << standardFormats << data.formats();
    formats.removeDuplicates();
    ui->comboBoxInputFormat->addItems(formats);
    const int index = qMax(0, ui->comboBoxInputFormat->findText(defaultFormat));
    ui->comboBoxInputFormat->setCurrentIndex(index);

}

void ActionDialog::restoreHistory()
{
    ConfigurationManager *cm = ConfigurationManager::instance();

    int maxCount = cm->value("command_history_size").toInt();
    ui->cmdEdit->setMaxCount(maxCount);

    QFile file( dataFilename() );
    file.open(QIODevice::ReadOnly);
    QDataStream in(&file);
    QVariant v;

    ui->cmdEdit->clear();
    while( !in.atEnd() ) {
        in >> v;
        ui->cmdEdit->addItem(v.toString());
    }
    ui->cmdEdit->setCurrentIndex(0);
    ui->cmdEdit->lineEdit()->selectAll();
}

const QString ActionDialog::dataFilename() const
{
    // do not use registry in windows
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QCoreApplication::organizationName(),
                       QCoreApplication::applicationName());
    // .ini -> _cmds.dat
    QString datfilename = settings.fileName();
    datfilename.replace( QRegExp("\\.ini$"),QString("_cmds.dat") );

    return datfilename;
}

void ActionDialog::saveHistory()
{
    QFile file( dataFilename() );
    file.open(QIODevice::WriteOnly);
    QDataStream out(&file);

    for (int i = 0; i < ui->cmdEdit->count(); ++i)
        out << QVariant(ui->cmdEdit->itemText(i));
}

void ActionDialog::createAction()
{
    QString cmd = ui->cmdEdit->currentText();

    if ( cmd.isEmpty() )
        return;

    const QString format = ui->comboBoxInputFormat->currentText();
    const QString input = ( format.isEmpty() || format.toLower().startsWith(QString("text")) )
            ? ui->inputText->toPlainText() : QString();

    // parse arguments
    Action::Commands commands;
    commands.append(QStringList());
    QStringList *command = &commands.last();
    QString arg;
    QChar quote;
    bool escape = false;
    bool percent = false;
    foreach (QChar c, cmd) {
        if (percent) {
            if (c >= '1' && c <= '9') {
                arg.resize( arg.size()-1 );
                int i = c.toLatin1() - '1';
                if (i == 0) {
                    arg.append(input);
                } else if ( input.indexOf(m_re) != -1 && m_re.captureCount() >= i ) {
                    arg.append( m_re.cap(i) );
                }
                continue;
            }
        }
        percent = !escape && c == '%';

        if (escape) {
            escape = false;
            if (c == 'n') {
                arg.append('\n');
            } else if (c == 't') {
                arg.append('\t');
            } else {
                arg.append(c);
            }
        } else if (c == '\\') {
            escape = true;
        } else if (!quote.isNull()) {
            if (quote == c) {
                quote = QChar();
                command->append(arg);
                arg.clear();
            } else {
                arg.append(c);
            }
        } else if (c == '\'' || c == '"') {
            quote = c;
        } else if ( c.isSpace() ) {
            if (!arg.isEmpty()) {
                command->append(arg);
                arg.clear();
            }
        } else if (c == '|') {
            if ( !arg.isEmpty() ) {
                command->append(arg);
                arg.clear();
            }
            commands.append(QStringList());
            command = &commands.last();
        } else {
            arg.append(c);
        }
    }
    if ( !arg.isEmpty() || !quote.isNull() )
        command->append(arg);

    QByteArray data;
    if ( !format.isEmpty() ) {
        if ( !input.isEmpty() )
            data = input.toLocal8Bit();
        else if (m_data != NULL)
            data = m_data->data(format);
    }

    Action *act = new Action( commands, data,
                              ui->comboBoxOutputFormat->currentText(),
                              ui->separatorEdit->text(),
                              ui->comboBoxOutputTab->currentText() );
    emit accepted(act);

    close();
}

void ActionDialog::setCommand(const QString &cmd)
{
    ui->cmdEdit->setEditText(cmd);
}

void ActionDialog::setSeparator(const QString &sep)
{
    ui->separatorEdit->setText(sep);
}

void ActionDialog::setInput(const QString &format)
{
    int index = ui->comboBoxInputFormat->findText(format);
    if (index == -1) {
        ui->comboBoxInputFormat->insertItem(0, format);
        index = 0;
    }
    ui->comboBoxInputFormat->setCurrentIndex(index);
}

void ActionDialog::setOutput(const QString &format)
{
    ui->comboBoxOutputFormat->setEditText(format);
}

void ActionDialog::setOutputTabs(const QStringList &tabs,
                                 const QString &currentTabName)
{
    QComboBox *w = ui->comboBoxOutputTab;
    w->clear();
    w->addItem("");
    w->addItems(tabs);
    w->setEditText(currentTabName);
}

void ActionDialog::setRegExp(const QRegExp &re)
{
    m_re = re;
}

void ActionDialog::loadSettings()
{
    ConfigurationManager *cm = ConfigurationManager::instance();

    restoreHistory();

    ui->comboBoxInputFormat->clear();
    ui->comboBoxInputFormat->addItems(standardFormats);
    ui->comboBoxInputFormat->setCurrentIndex(cm->value("action_has_input").toBool() ? 1 : 0);

    ui->comboBoxOutputFormat->clear();
    ui->comboBoxOutputFormat->addItems(standardFormats);
    ui->comboBoxOutputFormat->setCurrentIndex(cm->value("action_has_output").toBool() ? 1 : 0);

    ui->separatorEdit->setText(cm->value("action_separator").toString());
    ui->comboBoxOutputTab->setEditText(cm->value("action_output_tab").toString());
}

void ActionDialog::saveSettings()
{
    ConfigurationManager *cm = ConfigurationManager::instance();
    cm->saveGeometry(this);

    cm->setValue("action_has_input",
                 ui->comboBoxInputFormat->currentText() == QString("text/plain"));
    cm->setValue("action_has_output",
                 ui->comboBoxOutputFormat->currentText() == QString("text/plain"));
    cm->setValue("action_separator",  ui->separatorEdit->text());
    cm->setValue("action_output_tab", ui->comboBoxOutputTab->currentText());

    saveHistory();
}

void ActionDialog::showEvent(QShowEvent *e)
{
    QDialog::showEvent(e);
    ConfigurationManager::instance()->loadGeometry(this);
    updateMinimalGeometry();
}

void ActionDialog::accept()
{
    QDialog::accept();

    QString text = ui->cmdEdit->currentText();
    int i = ui->cmdEdit->findText(text);
    if (i != -1)
        ui->cmdEdit->removeItem(i);
    ui->cmdEdit->insertItem(0, text);

    saveSettings();
}

void ActionDialog::updateMinimalGeometry()
{
    int w = width();
    resize(minimumSize());
    adjustSize();
    resize(w, height());
}

void ActionDialog::on_buttonBox_clicked(QAbstractButton* button)
{
    Command cmd;
    ConfigurationManager *cm;

    switch ( ui->buttonBox->standardButton(button) ) {
    case QDialogButtonBox::Ok:
        createAction();
        break;
    case QDialogButtonBox::Save:
        cmd.name = cmd.cmd = ui->cmdEdit->currentText();
        cmd.input = ui->comboBoxInputFormat->currentText();
        cmd.output = ui->comboBoxOutputFormat->currentText();
        cmd.sep = ui->separatorEdit->text();
        cmd.outputTab = ui->comboBoxOutputTab->currentText();

        cm = ConfigurationManager::instance();
        cm->addCommand(cmd);
        cm->saveSettings();
        QMessageBox::information(
                    this, tr("Command saved"),
                    tr("Command was saved and can be accessed from item menu.\n"
                       "You can set up the command in preferences.") );
        break;
    case QDialogButtonBox::Cancel:
        close();
        break;
    default:
        break;
    }
}

void ActionDialog::on_comboBoxInputFormat_currentIndexChanged(const QString &format)
{
    bool show = format.toLower().startsWith(QString("text"));
    ui->inputText->setVisible(show);

    QString text;
    if ((show || format.isEmpty()) && m_data != NULL) {
        text = format.isEmpty() ? m_data->text() : QString::fromLocal8Bit(m_data->data(format));
    }
    ui->inputText->setPlainText(text);

    updateMinimalGeometry();
}

void ActionDialog::on_comboBoxOutputFormat_editTextChanged(const QString &text)
{
    bool showSeparator = text.toLower().startsWith(QString("text"));
    ui->separatorLabel->setVisible(showSeparator);
    ui->separatorEdit->setVisible(showSeparator);

    bool showOutputTab = !text.isEmpty();
    ui->labelOutputTab->setVisible(showOutputTab);
    ui->comboBoxOutputTab->setVisible(showOutputTab);

    updateMinimalGeometry();
}
