#include <QAction>
#include "action.h"

// free action IDs
bool g_ids[10] = {true,true,true,true,true,
                  true,true,true,true,true};

Action::Action(const QString &cmd, const QStringList &args,
               const QByteArray &input,
               bool outputItems, const QString &itemSeparator) : QProcess(),
    m_input(input), m_sep(itemSeparator), m_cmd(cmd), m_args(args)
{
    m_id = 10;
    for (int i = 0; i<10; ++i) {
        if( g_ids[i] ) {
            g_ids[i] = false;
            m_id = i;
            break;
        }
    }

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
}

void Action::actionError(QProcess::ProcessError)
{
    if ( state() != Running ) {
        m_errstr = QString("Error: %1\n").arg(errorString()) + m_errstr;
        emit actionFinished(this);
    }
}

void Action::actionStarted()
{
    // menu item
    QString txt;
    if (m_id < 10)
        txt = QString("&%1. ").arg(m_id);
    txt += QString("kill ") + m_cmd;
    m_menuItem = new QAction(txt, this);
    connect( m_menuItem, SIGNAL(triggered()),
             this, SLOT(terminate()) );
    emit addMenuItem(m_menuItem);

    // write input
    if ( !m_input.isEmpty() )
        write( m_input );
    closeWriteChannel();
}

void Action::actionFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if ( exitStatus != NormalExit )
        m_errstr = QString("Error: %1\n").arg(errorString()) + m_errstr;
    else if ( exitCode != 0 )
        m_errstr = QString("Exit code: %1\n").arg(exitCode) + m_errstr;

    emit removeMenuItem(m_menuItem);

    if (m_id < 10)
        g_ids[m_id] = true;

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

void Action::terminate()
{
    // try to terminate process
    QProcess::terminate();
    // if process still running: kill it
    if ( !waitForFinished(5000) )
        kill();
}
