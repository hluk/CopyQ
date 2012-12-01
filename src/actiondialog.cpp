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

#include "actiondialog.h"
#include "ui_actiondialog.h"
#include "client_server.h"
#include "action.h"
#include "configurationmanager.h"

#include <QToolTip>
#include <QSettings>
#include <QFile>
#include <QCompleter>
#include <QMessageBox>

ActionDialog::ActionDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ActionDialog)
    , m_maxitems(100)
    , m_completer(NULL)
    , m_history()
{
    ui->setupUi(this);
    connect(this, SIGNAL(finished(int)), SLOT(onFinnished(int)));

    ui->cmdEdit->setFocus();

    loadSettings();
}

ActionDialog::~ActionDialog()
{
    delete ui;
}

void ActionDialog::setInputText(const QString &input)
{
    ui->inputText->setPlainText(input);
}

void ActionDialog::add(const QString &cmd)
{
    if ( m_history.contains(cmd) )
        return;

    m_history.prepend(cmd);
    if ( m_history.size() > m_maxitems )
        m_history.removeLast();

    if ( m_completer )
        delete m_completer;
    m_completer = new QCompleter(m_history, this);
    ui->cmdEdit->setCompleter(m_completer);
}

void ActionDialog::restoreHistory()
{
    ConfigurationManager *cm = ConfigurationManager::instance();

    m_maxitems = cm->value("command_history").toInt();

    QFile file( dataFilename() );
    file.open(QIODevice::ReadOnly);
    QDataStream in(&file);
    QVariant v;
    m_history.clear();
    while( !in.atEnd() ) {
        in >> v;
        m_history.append( v.toString() );
    }

    if ( m_completer )
        delete m_completer;
    m_completer = new QCompleter(m_history, this);
    ui->cmdEdit->setCompleter(m_completer);
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

    QStringList::const_iterator it;
    for( it = m_history.begin(); it != m_history.end(); ++it)
        out << QVariant(*it);
}

void ActionDialog::createAction()
{
    QString cmd = ui->cmdEdit->text();

    if ( cmd.isEmpty() )
        return;
    QString input = ui->inputText->toPlainText();

    // parse arguments
    QStringList args;
    QString arg;
    QChar quote;
    bool escape = false;
    bool percent = false;
    foreach (QChar c, cmd) {
        if (percent) {
            if (c >= '1' && c <= '9') {
                arg.resize( arg.size()-1 );
                int i = c.toAscii() - '1';
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
                args.append(arg);
                arg.clear();
            } else {
                arg.append(c);
            }
        } else if (c == '\'' || c == '"') {
            quote = c;
        } else if ( c.isSpace() ) {
            if (!arg.isEmpty()) {
                args.append(arg);
                arg.clear();
            }
        } else {
            arg.append(c);
        }
    }
    if ( !arg.isEmpty() || !quote.isNull() )
        args.append(arg);
    if ( !args.isEmpty() )
        cmd = args.takeFirst();

    if ( !ui->inputCheckBox->isEnabled() )
        input.clear();

    Action *act = new Action( cmd, args, input.toLocal8Bit(),
                              ui->outputCheckBox->isChecked(),
                              ui->separatorEdit->text(),
                              ui->comboBoxOutputTab->currentText() );
    emit accepted(act);

    add( ui->cmdEdit->text() );

    close();
}

void ActionDialog::setCommand(const QString &cmd)
{
    ui->cmdEdit->setText(cmd);
}

void ActionDialog::setSeparator(const QString &sep)
{
    ui->separatorEdit->setText(sep);
}

void ActionDialog::setInput(bool value)
{
    ui->inputCheckBox->setCheckState(value ? Qt::Checked : Qt::Unchecked);
}

void ActionDialog::setOutput(bool value)
{
    ui->outputCheckBox->setCheckState(value ? Qt::Checked : Qt::Unchecked);
}

void ActionDialog::setOutputTabs(const QStringList &tabs,
                                 const QString &currentTabName)
{
    QComboBox *w = ui->comboBoxOutputTab;
    QString text = w->currentText();
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

    ui->cmdEdit->setText(cm->value("action_command").toString());
    ui->inputCheckBox->setChecked(cm->value("action_has_input").toBool());
    ui->outputCheckBox->setChecked(cm->value("action_has_output").toBool());
    ui->separatorEdit->setText(cm->value("action_separator").toString());
    ui->comboBoxOutputTab->setEditText(cm->value("action_output_tab").toString());

    restoreHistory();
}

void ActionDialog::saveSettings()
{
    ConfigurationManager *cm = ConfigurationManager::instance();
    cm->saveGeometry(this);

    cm->setValue("action_command",    ui->cmdEdit->text());
    cm->setValue("action_has_input",  ui->inputCheckBox->isChecked());
    cm->setValue("action_has_output", ui->outputCheckBox->isChecked());
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

void ActionDialog::on_outputCheckBox_toggled(bool checked)
{
    ui->separatorEdit->setEnabled(checked);
    ui->separatorLabel->setEnabled(checked);
    ui->labelOutputTab->setEnabled(checked);
    ui->comboBoxOutputTab->setEnabled(checked);
}

void ActionDialog::onFinnished(int result)
{
    if (result == QDialog::Accepted)
        saveSettings();
}

void ActionDialog::updateMinimalGeometry()
{
    int w = width();
    resize(minimumSize());
    adjustSize();
    if (w > width())
        resize(w, height());
}

void ActionDialog::on_buttonBox_clicked(QAbstractButton* button)
{
    QString name;
    Command cmd;
    ConfigurationManager *cm;

    switch ( ui->buttonBox->standardButton(button) ) {
    case QDialogButtonBox::Ok:
        createAction();
        break;
    case QDialogButtonBox::Save:
        cmd.name = cmd.cmd = ui->cmdEdit->text();
        cmd.input = ui->inputCheckBox->isChecked();
        cmd.output = ui->outputCheckBox->isChecked();
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
