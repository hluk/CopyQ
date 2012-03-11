#include <QToolTip>
#include <QSettings>
#include <QFile>
#include <QCompleter>
#include "client_server.h"
#include "actiondialog.h"
#include "action.h"
#include "configurationmanager.h"
#include "ui_actiondialog.h"

ActionDialog::ActionDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ActionDialog), m_completer(NULL), m_actions(0)
{
    ui->setupUi(this);
    ui->inputText->clear();
    ui->iconLabel->setPixmap( QPixmap(":/images/actiondialog.svg") );
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

    restoreGeometry( ConfigurationManager::instance(this)->windowGeometry(
            objectName()) );
    ui->cmdEdit->setFocus();
}

void ActionDialog::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
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
    QSettings settings;

    m_maxitems = settings.value("command_history", 100).toInt();

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

void ActionDialog::closeAction(Action *act)
{
    const QString &errout =  act->errorOutput();
    if ( !errout.isEmpty() )
        emit message( QString("Command failed: ")+act->command(), errout );

    --m_actions;
    if (!m_actions) {
        changeTrayIcon( QIcon(":/images/icon.svg") );
    }

    disconnect(act);
    act->deleteLater();
}

void ActionDialog::runCommand()
{
    QString cmd = ui->cmdEdit->text();

    if ( cmd.isEmpty() )
        return;
    QString input = ui->inputText->toPlainText();
    Action *act;

    // escape spaces in input
    QStringList args;
    QString arg;
    bool quotes = false;
    bool dquotes = false;
    bool escape = false;
    bool outside = true;
    foreach(QChar c, cmd) {
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

    act = new Action( cmd, args, input.toLocal8Bit(),
                      ui->outputCheckBox->isChecked(),
                      ui->separatorEdit->text() );
    connect( act, SIGNAL(actionError(Action*)),
             this, SLOT(closeAction(Action*)) );
    connect( act, SIGNAL(actionFinished(Action*)),
             this, SLOT(closeAction(Action*)) );
    connect( act, SIGNAL(newItems(QStringList)),
             SIGNAL(addItems(QStringList)) );
    connect( act, SIGNAL(addMenuItem(QAction*)),
             this, SIGNAL(addMenuItem(QAction*)) );
    connect( act, SIGNAL(removeMenuItem(QAction*)),
                 this, SIGNAL(removeMenuItem(QAction*)) );

    if (!m_actions) {
        changeTrayIcon( QIcon(":/images/icon-running.svg") );
    }
    ++m_actions;

    log( tr("Executing: %1").arg(cmd) );
    log( tr("Arguments: %1").arg(args.join(", ")) );
    act->start();

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

void ActionDialog::on_outputCheckBox_toggled(bool checked)
{
    ui->separatorEdit->setEnabled(checked);
    ui->separatorLabel->setEnabled(checked);
}

void ActionDialog::onFinnished(int)
{
    ConfigurationManager::instance()->windowGeometry(
            objectName(), saveGeometry() );
}

void ActionDialog::on_buttonBox_clicked(QAbstractButton* button)
{
    QString name;
    ConfigurationManager::Command cmd;

    switch ( ui->buttonBox->standardButton(button) ) {
    case QDialogButtonBox::Ok:
        runCommand();
        break;
    case QDialogButtonBox::Save:
        cmd.name = cmd.cmd = ui->cmdEdit->text();
        cmd.input = ui->inputCheckBox->isChecked();
        cmd.output = ui->outputCheckBox->isChecked();
        cmd.sep = ui->separatorEdit->text();
        cmd.wait = false;
        cmd.automatic = false;
        cmd.ignore = false;

        ConfigurationManager::instance()->addCommand(name, &cmd);
        break;
    case QDialogButtonBox::Cancel:
        close();
        break;
    default:
        break;
    }
}
