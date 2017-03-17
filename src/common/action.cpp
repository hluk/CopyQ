/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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

#include "common/common.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/textdata.h"
#include "item/serialize.h"

#include <QCoreApplication>
#include <QProcessEnvironment>

#include <cstring>

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
        *script = cmd.string()->mid( cmd.position() + static_cast<int>(strlen(label)) );
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
                const QStringRef cmd1 = cmd.midRef(i);
                if ( getScriptFromLabel("copyq:", cmd1, &script) )
                    command << "copyq" << "eval" << "--" << script;
                else if ( getScriptFromLabel("sh:", cmd1, &script) )
                    command << "sh" << "-c" << "--" << script << "--";
                else if ( getScriptFromLabel("bash:", cmd1, &script) )
                    command << "bash" << "-c" << "--" << script << "--";
                else if ( getScriptFromLabel("perl:", cmd1, &script) )
                    command << "perl" << "-e" << script << "--";
                else if ( getScriptFromLabel("python:", cmd1, &script) )
                    command << "python" << "-c" << script;
                else if ( getScriptFromLabel("ruby:", cmd1, &script) )
                    command << "ruby" << "-e" << script << "--";

                if ( !script.isEmpty() ) {
                    command.append( capturedTexts.mid(1) );
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

Action::Action(QObject *parent)
    : QObject(parent)
    , m_failed(false)
    , m_currentLine(-1)
    , m_exitCode(0)
{
}

Action::~Action()
{
    closeSubCommands();
}

QString Action::command() const
{
    QString text;
    for ( const auto &line : m_cmds ) {
        for ( const auto &args : line ) {
            if ( !text.isEmpty() )
                text.append(QChar('|'));
            text.append(args.join(" "));
        }
        text.append('\n');
    }
    return text.trimmed();
}

void Action::setCommand(const QString &command, const QStringList &arguments)
{
    m_cmds = parseCommands(command, arguments);
}

void Action::setCommand(const QStringList &arguments)
{
    m_cmds.clear();
    m_cmds.append(QList<QStringList>() << arguments);
}

void Action::setInput(const QVariantMap &data, const QString &inputFormat)
{
    if (inputFormat == mimeItems) {
        m_input = serializeData(data);
        m_inputFormats = data.keys();
    } else {
        m_input = data.value(inputFormat).toByteArray();
        m_inputFormats = QStringList(inputFormat);
    }
}

void Action::start()
{
    closeSubCommands();

    if ( m_currentLine + 1 >= m_cmds.size() ) {
        actionFinished();
        return;
    }

    ++m_currentLine;
    const QList<QStringList> &cmds = m_cmds[m_currentLine];

    Q_ASSERT( !cmds.isEmpty() );

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (m_id != -1)
        env.insert("COPYQ_ACTION_ID", QString::number(m_id));
    if ( !m_name.isEmpty() )
        env.insert("COPYQ_ACTION_NAME", m_name);

    for (int i = 0; i < cmds.size(); ++i) {
        auto process = new QProcess(this);
        m_processes.append(process);
        process->setProcessEnvironment(env);
        if ( !m_workingDirectoryPath.isEmpty() )
            process->setWorkingDirectory(m_workingDirectoryPath);

#if QT_VERSION < 0x050600
        connect( process, SIGNAL(error(QProcess::ProcessError)),
                 SLOT(onSubProcessError(QProcess::ProcessError)) );
#else
        connect( process, SIGNAL(errorOccurred(QProcess::ProcessError)),
                 SLOT(onSubProcessError(QProcess::ProcessError)) );
#endif
        connect( process, SIGNAL(readyReadStandardError()),
                 SLOT(onSubProcessErrorOutput()) );
    }

    for (int i = 1; i < m_processes.size(); ++i) {
        m_processes[i - 1]->setStandardOutputProcess(m_processes[i]);
        connect( m_processes[i], SIGNAL(finished(int)),
                 m_processes[i - 1], SLOT(terminate()) );
    }

    connect( m_processes.last(), SIGNAL(started()),
             this, SLOT(onSubProcessStarted()) );
    connect( m_processes.last(), SIGNAL(finished(int,QProcess::ExitStatus)),
             this, SLOT(onSubProcessFinished()) );
    connect( m_processes.last(), SIGNAL(readyReadStandardOutput()),
             this, SLOT(onSubProcessOutput()) );

    // Writing directly to stdin of a process on Windows can hang the app.
    connect( m_processes.first(), SIGNAL(started()),
             this, SLOT(writeInput()), Qt::QueuedConnection );
    connect( m_processes.first(), SIGNAL(bytesWritten(qint64)),
             this, SLOT(onBytesWritten()), Qt::QueuedConnection );

    if (m_outputFormat.isEmpty())
        m_processes.last()->closeReadChannel(QProcess::StandardOutput);

    for (int i = 0; i < m_processes.size(); ++i)
        startProcess(m_processes[i], cmds[i]);
}

bool Action::waitForStarted(int msecs)
{
    return !m_processes.isEmpty() && m_processes.last()->waitForStarted(msecs);
}

bool Action::waitForFinished(int msecs)
{
    if ( !isRunning() )
        return true;

    for ( int waitMsec = 0;
          waitMsec < msecs && !m_processes.isEmpty() && !m_processes.last()->waitForFinished(100);
          waitMsec += 100 )
    {
        QCoreApplication::processEvents();
    }

    return !isRunning();
}

bool Action::isRunning() const
{
    return !m_processes.isEmpty() && m_processes.last()->state() != QProcess::NotRunning;
}

void Action::setData(const QVariantMap &data)
{
    m_data = data;
    emit dataChanged(data);
}

const QVariantMap &Action::data() const
{
    return m_data;
}

void Action::onSubProcessError(QProcess::ProcessError error)
{
    QProcess *p = qobject_cast<QProcess*>(sender());
    Q_ASSERT(p);

    // Ignore write-to-process error, process can ignore the input.
    if (error != QProcess::WriteError) {
        if (!m_errorString.isEmpty())
            m_errorString.append("\n");
        m_errorString.append( p->errorString() );
        m_failed = true;
    }

    if ( !isRunning() )
        actionFinished();
}

void Action::onSubProcessStarted()
{
    if (m_currentLine == 0)
        emit actionStarted(this);
}

void Action::onSubProcessFinished()
{
    onSubProcessOutput();
    start();
}

void Action::onSubProcessOutput()
{
    if ( m_processes.isEmpty() )
        return;

    auto p = m_processes.last();
    const auto output = p->readAll();
    if ( output.isEmpty() )
        return;

    if ( !m_outputFormat.isEmpty() ) {
        m_outputData.append(output);

        if ( !m_sep.isEmpty() ) {
            m_lastOutput.append( getTextData(output) );
            auto items = m_lastOutput.split(m_sep);
            m_lastOutput = items.takeLast();
            if ( !items.isEmpty() )
                emit newItems(items, m_outputFormat, m_tab);
        } else if ( m_outputFormat == mimeText && m_index.isValid() ) {
            emit changeItem(m_outputData, m_outputFormat, m_index);
        }
    }
}

void Action::onSubProcessErrorOutput()
{
    QProcess *p = qobject_cast<QProcess*>(sender());
    Q_ASSERT(p);

    m_errorOutput.append( getTextData(p->readAllStandardError()) );
}

void Action::writeInput()
{
    if (m_processes.isEmpty())
        return;

    QProcess *p = m_processes.first();

    if (m_input.isEmpty())
        p->closeWriteChannel();
    else
        p->write(m_input);
}

void Action::onBytesWritten()
{
    if ( !m_processes.isEmpty() )
        m_processes.first()->closeWriteChannel();
}

void Action::terminate()
{
    if (m_processes.isEmpty())
        return;

    // try to terminate process
    for (auto p : m_processes)
        p->terminate();

    // if process still running: kill it
    if ( !waitForFinished(5000) )
        terminateProcess( m_processes.last() );
}

void Action::closeSubCommands()
{
    terminate();

    if (m_processes.isEmpty())
        return;

    m_exitCode = m_processes.last()->exitCode();
    m_failed = m_failed || m_processes.last()->exitStatus() != QProcess::NormalExit;

    for (auto p : m_processes)
        p->deleteLater();

    m_processes.clear();
}

void Action::actionFinished()
{
    closeSubCommands();

    if ( !m_outputFormat.isEmpty() ) {
        if ( !m_sep.isEmpty() ) {
            if ( !m_lastOutput.isEmpty() )
                emit newItems(QStringList() << m_lastOutput, m_outputFormat, m_tab);
        } else if ( !m_outputData.isEmpty() ) {
            if ( m_index.isValid() )
                emit changeItem(m_outputData, m_outputFormat, m_index);
            else
                emit newItem(m_outputData, m_outputFormat, m_tab);
        }
    }

    emit actionFinished(this);
}
