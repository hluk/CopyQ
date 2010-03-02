#include <QProcess>
#include <QToolTip>
#include <QSettings>
#include <QFile>
#include <QCompleter>
#include <QDebug>
#include "actiondialog.h"
#include "ui_actiondialog.h"

static const QRegExp str("([^\\%])%s");

ActionDialog::ActionDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ActionDialog), m_completer(NULL)
{
    ui->setupUi(this);
    ui->inputText->clear();
    restoreHistory();
}

ActionDialog::~ActionDialog()
{
    saveHistory();
    delete ui;
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

void ActionDialog::setInput(QString input)
{
    if ( input.isEmpty() ) {
        // no input
        ui->inputText->clear();
        ui->inputCheckBox->hide();
        ui->inputInfoLabel->hide();
        ui->inputText->hide();
    }
    else {
        ui->inputText->setText(input);
        ui->inputCheckBox->show();
        ui->inputInfoLabel->show();
        ui->inputText->show();
    }
}

void ActionDialog::add(QString command)
{
    if ( m_history.contains(command) )
        return;

    m_history.prepend(command);
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

void ActionDialog::accept()
{
    QString cmd = ui->cmdEdit->text();
    if ( cmd.isEmpty() )
        return;
    QStringList items;
    QString input = ui->inputText->text();
    QIODevice::OpenMode mode = QIODevice::NotOpen;
    QProcess proc;
    QString errstr;

    // replace %s with input
    if ( cmd.indexOf(str) != -1 ) {
        // if %s present and no input specified
        // TODO: warn user
        if ( input.isEmpty() )
            return;
        cmd.replace( str, "\\1"+input );
    }
    if ( !ui->inputCheckBox->isEnabled() )
        input.clear();

    // IO mode
    if ( !input.isEmpty() )
        mode |= QIODevice::WriteOnly;
    if ( ui->outputCheckBox->isChecked() )
        mode |= QIODevice::ReadOnly;

    // execute command (with input if needed)
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start(cmd, mode);

    if ( proc.waitForStarted() ) {
        // write input
        if (mode & QIODevice::WriteOnly) {
            proc.write( input.toLocal8Bit() );
        }
        proc.closeWriteChannel();

        // read output
        if (mode & QIODevice::ReadOnly) {
            QString stdout;

            if ( proc.waitForFinished() ) {
                if ( proc.exitCode() != 0 )
                    errstr = QString("Exit code: %1\n").arg(proc.exitCode());

                errstr += QString::fromLocal8Bit( proc.readAllStandardError() );
                stdout = QString::fromLocal8Bit( proc.readAll() );

                if ( !stdout.isEmpty() ) {
                    // separate items
                    QRegExp sep( ui->separatorEdit->text() );
                    if ( !sep.isEmpty() ) {
                        items = stdout.split(sep);
                    }
                    else
                        items.append(stdout);
                }
            }
            else
                errstr = proc.errorString();
        }
        emit addItems(items);
        add(cmd);
        close();
    }
    else
        errstr = proc.errorString();

    if ( !errstr.isEmpty() )
        emit error(errstr);
}
