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

#include "action.h"

#include <QAction>

Action::Action(const QString &cmd, const QStringList &args,
               const QByteArray &input, const QString &outputItemFormat,
               const QString &itemSeparator,
               const QString &outputTabName)
    : QProcess()
    , m_input(input)
    , m_sep(itemSeparator)
    , m_cmd(cmd)
    , m_args(args)
    , m_tab(outputTabName)
    , m_outputFormat(outputItemFormat != "text/plain" ? outputItemFormat : QString())
    , m_errstr()
    , m_lastOutput()
    , m_failed(false)
{
    setProcessChannelMode(QProcess::SeparateChannels);
    connect( this, SIGNAL(error(QProcess::ProcessError)),
             SLOT(actionError(QProcess::ProcessError)) );
    connect( this, SIGNAL(started()),
             SLOT(actionStarted()) );
    connect( this, SIGNAL(finished(int,QProcess::ExitStatus)),
             SLOT(actionFinished()) );
    connect( this, SIGNAL(readyReadStandardError()),
             SLOT(actionErrorOutput()) );

    if ( !outputItemFormat.isEmpty() ) {
        connect( this, SIGNAL(readyReadStandardOutput()),
                 SLOT(actionOutput()) );
    }
}

void Action::actionError(QProcess::ProcessError)
{
    if ( state() != Running ) {
        m_failed = true;
        emit actionFinished(this);
    }
}

void Action::actionStarted()
{
    // write input
    if ( !m_input.isEmpty() )
        write( m_input );
    closeWriteChannel();

    emit actionStarted(this);
}

void Action::actionFinished()
{
    if ( !m_outputFormat.isEmpty() ) {
        if ( !m_outputData.isNull() ) {
            emit newItem(m_outputData, m_outputFormat, m_tab);
            m_outputData = QByteArray();
        }
    } else if ( !m_lastOutput.isNull() ) {
        actionOutput();
        if ( !m_lastOutput.isNull() ) {
            QStringList items;
            items.append(m_lastOutput);
            emit newItems(items, m_tab);
            m_lastOutput = QString();
        }
    }

    emit actionFinished(this);
}

void Action::actionOutput()
{
    if (!m_outputFormat.isEmpty()) {
        m_outputData.append( readAll() );
        return;
    }

    m_lastOutput.append( QString::fromLocal8Bit(readAll()) );
    if ( m_lastOutput.isEmpty() || m_sep.isEmpty() )
        return;

    // Split to items.
    QStringList items;
    items = m_lastOutput.split(m_sep);
    m_lastOutput = items.takeLast();
    emit newItems(items, m_tab);
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
