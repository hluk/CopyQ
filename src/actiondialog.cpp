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
    restoreHistory();
    connect(this, SIGNAL(finished(int)), SLOT(onFinnished(int)));
}

ActionDialog::~ActionDialog()
{
    saveHistory();
    delete ui;
}

void ActionDialog::showEvent(QShowEvent *e)
{
    QDialog::showEvent(e);

    ConfigurationManager::instance(this)->loadGeometry(this);
    ui->cmdEdit->setFocus();
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
    ConfigurationManager *cm = ConfigurationManager::instance(this);

    m_maxitems = cm->value("command_history").toInt();

    QFile file( dataFilename() );
    file.open(QIODevice::ReadOnly);
    QDataStream in(&file);
    QVariant v;
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

    // escape spaces in input
    QStringList args;
    QString arg;
    bool quotes = false;
    bool dquotes = false;
    bool escape = false;
    bool outside = true;
    foreach (QChar c, cmd) {
        if ( outside ) {
            if ( c.isSpace() )
                continue;
            else
                outside = false;
        }

        if ( c == '1' && arg.endsWith('%') ) {
            arg.remove( arg.size()-1, 1 );
            arg.append(input);
            continue;
        }

        if (escape) {
            if ( c == 'n' ) {
                arg.append('\n');
            } else if ( c == 't' ) {
                arg.append('\t');
            } else {
                arg.append(c);
            }
        } else if (c == '\\') {
            escape = true;
        } else if (c == '\'') {
            quotes = !quotes;
        } else if (c == '"') {
            dquotes = !dquotes;
        } else if (quotes) {
            arg.append(c);
        } else if ( c.isSpace() ) {
            outside = true;
            args.append(arg);
            arg.clear();
        } else {
            arg.append(c);
        }
    }
    if ( !outside ) {
        args.append(arg);
        arg.clear();
    }
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

void ActionDialog::on_outputCheckBox_toggled(bool checked)
{
    ui->separatorEdit->setEnabled(checked);
    ui->separatorLabel->setEnabled(checked);
    ui->labelOutputTab->setEnabled(checked);
    ui->comboBoxOutputTab->setEnabled(checked);
}

void ActionDialog::onFinnished(int)
{
    ConfigurationManager::instance()->saveGeometry(this);
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
