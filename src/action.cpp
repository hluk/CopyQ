#include "action.h"

Action::Action(const QString &cmd, const QByteArray &input,
               bool outputItems, const QString &itemSeparator) : QProcess(),
    m_input(input), m_sep(itemSeparator)
{
    setProcessChannelMode(QProcess::SeparateChannels);
    connect( this, SIGNAL(error(QProcess::ProcessError)),
             SLOT(actionError(QProcess::ProcessError)) );
    connect( this, SIGNAL(started()),
             SLOT(actionStarted()) );
    connect( this, SIGNAL(finished(int,QProcess::ExitStatus)),
             SLOT(actionFinished(int,QProcess::ExitStatus)) );
    connect( this, SIGNAL(readyReadStandardError()),
             SLOT(actionErrorOutput()) );

    if ( outputItems )
        connect( this, SIGNAL(readyReadStandardOutput()),
                 SLOT(actionOutput()) );

    start(cmd, QIODevice::ReadWrite);
}

void Action::actionError(QProcess::ProcessError)
{
    emit actionError( errorString() );
}

void Action::actionStarted()
{
    // write input
    if ( !m_input.isEmpty() )
        write( m_input );
    closeWriteChannel();
}

void Action::actionFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QString errstr;

    // read output
    if ( exitStatus )
        actionError( errorString() );
    else if ( exitCode != 0 )
        m_errstr = QString("Exit code: %1\n").arg(exitCode) + m_errstr;

    emit actionFinished(this);
}

void Action::actionOutput()
{
    QString outstr = QString::fromLocal8Bit( readAll() );

    if ( !outstr.isEmpty() ) {
        QStringList items;

        // separate items
        if ( !m_sep.isEmpty() ) {
            items = outstr.split(m_sep);
        }
        else
            items.append(outstr);
        emit newItems(items);
    }
}

void Action::actionErrorOutput()
{
    m_errstr += QString::fromLocal8Bit( readAllStandardError() );
}
