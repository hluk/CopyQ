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

#include "common/action.h"
#include "common/client_server.h"
#include "common/command.h"
#include "gui/configurationmanager.h"

#include <QSettings>
#include <QFile>
#include <QMessageBox>
#include <QMimeData>

namespace {

const QStringList standardFormats = QStringList() << QString() << QString("text/plain");

bool wasChangedByUser(QObject *object)
{
    return object->property("UserChanged").toBool();
}

void setChangedByUser(QWidget *object)
{
    object->setProperty("UserChanged", object->hasFocus());
}

} // namespace

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
        if (v.canConvert(QVariant::String)) {
            // backwards compatibility with versions up to 1.8.2
            QVariantMap values;
            values["cmd"] = v;
            ui->cmdEdit->addItem(v.toString(), values);
        } else {
            QVariantMap values = v.value<QVariantMap>();
            ui->cmdEdit->addItem(values["cmd"].toString(), v);
        }
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
        out << QVariant(ui->cmdEdit->itemData(i));
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
                              ui->comboBoxOutputTab->currentText(),
                              m_index );
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

void ActionDialog::setOutputIndex(const QModelIndex &index)
{
    m_index = index;
}

void ActionDialog::loadSettings()
{
    ui->comboBoxInputFormat->clear();
    ui->comboBoxInputFormat->addItems(standardFormats);

    ui->comboBoxOutputFormat->clear();
    ui->comboBoxOutputFormat->addItems(standardFormats);

    restoreHistory();
    ConfigurationManager::instance()->loadGeometry(this);
    updateMinimalGeometry();
}

void ActionDialog::saveSettings()
{
    ConfigurationManager::instance()->saveGeometry(this);
    saveHistory();
}

void ActionDialog::accept()
{
    QString text = ui->cmdEdit->currentText();

    QVariantMap values;
    values["cmd"] = text;
    values["input"] = ui->comboBoxInputFormat->currentText();
    values["output"] = ui->comboBoxOutputFormat->currentText();
    values["sep"] = ui->separatorEdit->text();
    values["outputTab"] = ui->comboBoxOutputTab->currentText();

    int i = ui->cmdEdit->findText(text);
    if (i != -1)
        ui->cmdEdit->removeItem(i);

    ui->cmdEdit->insertItem(0, text, values);

    saveSettings();

    QDialog::accept();
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

void ActionDialog::on_cmdEdit_currentIndexChanged(int index)
{
    // Restore values from history.
    QVariant v = ui->cmdEdit->itemData(index);
    QVariantMap values = v.value<QVariantMap>();

    // Don't automatically change values if they were edited by user.
    if ( !wasChangedByUser(ui->comboBoxInputFormat) ) {
        int i = ui->comboBoxInputFormat->findText(values.value("input").toString());
        if (i != -1)
            ui->comboBoxInputFormat->setCurrentIndex(i);
    }

    if ( !wasChangedByUser(ui->comboBoxOutputFormat) )
        ui->comboBoxOutputFormat->setEditText(values.value("output").toString());

    if ( !wasChangedByUser(ui->separatorEdit) )
        ui->separatorEdit->setText(values.value("sep").toString());

    if ( !wasChangedByUser(ui->comboBoxOutputTab) )
        ui->comboBoxOutputTab->setEditText(values.value("outputTab").toString());
}

void ActionDialog::on_comboBoxInputFormat_currentIndexChanged(const QString &format)
{
    setChangedByUser(ui->comboBoxInputFormat);

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
    setChangedByUser(ui->comboBoxOutputFormat);

    bool showSeparator = text.toLower().startsWith(QString("text"));
    ui->separatorLabel->setVisible(showSeparator);
    ui->separatorEdit->setVisible(showSeparator);

    bool showOutputTab = !text.isEmpty();
    ui->labelOutputTab->setVisible(showOutputTab);
    ui->comboBoxOutputTab->setVisible(showOutputTab);

    updateMinimalGeometry();
}

void ActionDialog::on_comboBoxOutputTab_editTextChanged(const QString &)
{
    setChangedByUser(ui->comboBoxOutputTab);
}

void ActionDialog::on_separatorEdit_textEdited(const QString &)
{
    setChangedByUser(ui->separatorEdit);
}
