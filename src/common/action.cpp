/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

#include <QCoreApplication>

#include <string.h>

namespace {

void startProcess(QProcess *process, const QStringList &args)
{
    QString executable = args.value(0);

    // Replace "copyq" command with full application path.
    if (executable == "copyq")
        executable = QCoreApplication::applicationFilePath();

    process->start(executable, args.mid(1), QIODevice::ReadWrite);
}

template <typename Entry, typename Container>
void appendAndClearNonEmpty(Entry &entry, Container &containter)
{
    if ( !entry.isEmpty() ) {
        containter.append(entry);
        entry.clear();
    }
}

bool getScriptFromLabel(const char *label, const QStringRef &cmd, QString *script)
{
    if ( cmd.startsWith(label) ) {
        *script = cmd.string()->mid( cmd.position() + strlen(label) );
        return true;
    }

    return false;
}

QList< QList<QStringList> > parseCommands(const QString &cmd, const QStringList &capturedTexts)
{
    QList< QList<QStringList> > lines;
    QList<QStringList> commands;
    QStringList command;
    QString script;

    QString arg;
    QChar quote;
    bool escape = false;
    bool percent = false;

    for (int i = 0; i < cmd.size(); ++i) {
        const QChar &c = cmd[i];

        if (percent) {
            if (c >= '1' && c <= '9') {
                arg.resize( arg.size() - 1 );
                arg.append( capturedTexts.value(c.digitValue() - 1) );
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
            } else if (c == '\n') {
                // Ignore escaped new line character.
            } else {
                arg.append(c);
            }
        } else if (c == '\\') {
            escape = true;
        } else if (!quote.isNull()) {
            if (quote == c) {
                quote = QChar();
                command.append(arg);
                arg.clear();
            } else {
                arg.append(c);
            }
        } else if (c == '\'' || c == '"') {
            quote = c;
        } else if (c == '|') {
            appendAndClearNonEmpty(arg, command);
            appendAndClearNonEmpty(command, commands);
        } else if (c == '\n' || c == ';') {
            appendAndClearNonEmpty(arg, command);
            appendAndClearNonEmpty(command, commands);
            appendAndClearNonEmpty(commands, lines);
        } else if ( c.isSpace() ) {
            if (!arg.isEmpty()) {
                command.append(arg);
                arg.clear();
            }
        } else if ( c == ':' && i + 1 < cmd.size() && cmd[i+1] == '\n' ) {
            // If there is unescaped colon at the end of a line,
            // treat the rest of the command as single argument.
            appendAndClearNonEmpty(arg, command);
            arg = cmd.mid(i + 2);
            break;
        } else {
            if ( arg.isEmpty() && command.isEmpty() ) {
                // Treat command as script if known label is present.
                const QStringRef c = cmd.midRef(i);
                if ( getScriptFromLabel("copyq:", c, &script) )
                    command << "copyq" << "eval" << "--" << script << capturedTexts;
                else if ( getScriptFromLabel("sh:", c, &script) )
                    command << "sh" << "-c" << "--" << script << "--" << capturedTexts;
                else if ( getScriptFromLabel("bash:", c, &script) )
                    command << "bash" << "-c" << "--" << script << "--" << capturedTexts;
                else if ( getScriptFromLabel("perl:", c, &script) )
                    command << "perl" << "-e" << script << "--" << capturedTexts;
                else if ( getScriptFromLabel("python:", c, &script) )
                    command << "python" << "-c" << script << capturedTexts;
                else if ( getScriptFromLabel("ruby:", c, &script) )
                    command << "ruby" << "-e" << script << "--" << capturedTexts;

                if ( !script.isEmpty() ) {
                    commands.append(command);
                    lines.append(commands);
                    return lines;
                }
            }

            arg.append(c);
        }
    }

    appendAndClearNonEmpty(arg, command);
    appendAndClearNonEmpty(command, commands);
    appendAndClearNonEmpty(commands, lines);

    return lines;
}

} // namespace

Action::Action(const QString &cmd,
               const QByteArray &input,
               const QStringList &capturedTexts,
               const QStringList &inputFormats,
               const QString &outputItemFormat,
               const QString &itemSeparator,
               const QString &outputTabName,
               const QModelIndex &index)
    : QProcess()
    , m_input(input)
    , m_sep(index.isValid() ? QString() : itemSeparator)
    , m_cmds(parseCommands(cmd, capturedTexts))
    , m_tab(outputTabName)
    , m_inputFormats(inputFormats)
    , m_outputFormat(outputItemFormat != "text/plain" ? outputItemFormat : QString())
    , m_index(index)
    , m_errstr()
    , m_lastOutput()
    , m_failed(false)
    , m_firstProcess(NULL)
    , m_currentLine(-1)
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

QString Action::command() const
{
    QString text;
    foreach ( const QList<QStringList> &line, m_cmds ) {
        foreach ( const QStringList &args, line ) {
            if ( !text.isEmpty() )
                text.append(QChar('|'));
            text.append(args.join(" "));
        }
        text.append('\n');
    }
    return text.trimmed();
}

QString Action::outputFormat() const
{
    return m_outputFormat.isEmpty() ? QString("text/plain") : m_outputFormat;
}

bool Action::start()
{
    if ( m_currentLine + 1 >= m_cmds.size() )
        return false;

    ++m_currentLine;
    const QList<QStringList> &cmds = m_cmds[m_currentLine];

    Q_ASSERT( !cmds.isEmpty() );

    if ( cmds.size() > 1 ) {
        QProcess *lastProcess = new QProcess(this);
        m_firstProcess = lastProcess;
        for ( int i = 0; i + 1 < cmds.size(); ++i ) {
            const QStringList &args = cmds[i];
            Q_ASSERT( !args.isEmpty() );
            QProcess *process = (i + 2 == cmds.size()) ? this : new QProcess(this);
            lastProcess->setStandardOutputProcess(process);
            startProcess(lastProcess, args);
            lastProcess = process;
        }
    } else {
        m_firstProcess = this;
    }

    startProcess(this, cmds.last());
    return true;
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
    if (m_firstProcess == NULL)
        return;

    // write input
    if ( !m_input.isEmpty() )
        m_firstProcess->write( m_input );
    m_firstProcess->closeWriteChannel();
    m_firstProcess = NULL;

    emit actionStarted(this);
}

void Action::actionFinished()
{
    if ( !m_outputFormat.isEmpty() ) {
        if ( !m_outputData.isNull() ) {
            if (m_index.isValid())
                emit newItem(m_outputData, m_outputFormat, m_index);
            else
                emit newItem(m_outputData, m_outputFormat, m_tab);
            m_outputData = QByteArray();
        }
    } else if ( !m_lastOutput.isNull() ) {
        actionOutput();
        if ( !m_lastOutput.isNull() ) {
            QStringList items;
            items.append(m_lastOutput);
            if (m_index.isValid())
                emit newItems(items, m_index);
            else
                emit newItems(items, m_tab);
            m_lastOutput = QString();
        }
    }

    if ( !start() )
        emit actionFinished(this);
}

void Action::actionOutput()
{
    if (!m_outputFormat.isEmpty()) {
        m_outputData.append( readAll() );
        return;
    }

    m_lastOutput.append( QString::fromUtf8(readAll()) );
    if ( m_lastOutput.isEmpty() || m_sep.isEmpty() )
        return;

    // Split to items.
    QStringList items;
    items = m_lastOutput.split(m_sep);
    m_lastOutput = items.takeLast();
    if (m_index.isValid())
        emit newItems(items, m_index);
    else
        emit newItems(items, m_tab);
}

void Action::actionErrorOutput()
{
    m_errstr += QString::fromUtf8( readAllStandardError() );
}

void Action::terminate()
{
    // try to terminate process
    QProcess::terminate();
    // if process still running: kill it
    if ( !waitForFinished(5000) )
        kill();
}
