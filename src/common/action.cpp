/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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
#include "common/processsignals.h"
#include "common/timer.h"
#include "item/serialize.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QPointer>
#include <QProcessEnvironment>
#include <QTimer>

#include <cstring>

namespace {

void startProcess(QProcess *process, const QStringList &args, QIODevice::OpenModeFlag mode)
{
    QString executable = args.value(0);

    // Replace "copyq" command with full application path.
    if (executable == "copyq")
        executable = QCoreApplication::applicationFilePath();

    process->start(executable, args.mid(1), mode);
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

template <typename Iterator>
void pipeThroughProcesses(Iterator begin, Iterator end)
{
    auto it1 = begin;
    for (auto it2 = it1 + 1; it2 != end; it1 = it2++) {
        (*it1)->setStandardOutputProcess(*it2);
        connectProcessFinished(*it2, *it1, &QProcess::terminate);
    }
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

QString Action::commandLine() const
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

void Action::setInputWithFormat(const QVariantMap &data, const QString &inputFormat)
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
        finish();
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
        m_processes.push_back(process);
        process->setProcessEnvironment(env);
        if ( !m_workingDirectoryPath.isEmpty() )
            process->setWorkingDirectory(m_workingDirectoryPath);

        connectProcessError(process, this, &Action::onSubProcessError);
        connect( process, &QProcess::readyReadStandardError,
                 this, &Action::onSubProcessErrorOutput );
    }

    pipeThroughProcesses(m_processes.begin(), m_processes.end());

    QProcess *lastProcess = m_processes.back();
    connect( lastProcess, &QProcess::started,
             this, &Action::onSubProcessStarted );
    connectProcessFinished( lastProcess, this, &Action::onSubProcessFinished );
    connect( lastProcess, &QProcess::readyReadStandardOutput,
             this, &Action::onSubProcessOutput );

    // Writing directly to stdin of a process on Windows can hang the app.
    QProcess *firstProcess = m_processes.front();
    connect( firstProcess, &QProcess::started,
             this, &Action::writeInput, Qt::QueuedConnection );
    connect( firstProcess, &QProcess::bytesWritten,
             this, &Action::onBytesWritten, Qt::QueuedConnection );

    const bool needWrite = !m_input.isEmpty();
    if (m_processes.size() == 1) {
        const auto mode =
                (needWrite && m_readOutput) ? QIODevice::ReadWrite
              : needWrite ? QIODevice::WriteOnly
              : m_readOutput ? QIODevice::ReadOnly
              : QIODevice::NotOpen;
        startProcess(firstProcess, cmds.first(), mode);
    } else {
        auto it = m_processes.begin();
        auto cmdIt = cmds.constBegin();
        startProcess(*it, *cmdIt, needWrite ? QIODevice::ReadWrite : QIODevice::ReadOnly);
        for (++it, ++cmdIt; it != m_processes.end() - 1; ++it, ++cmdIt)
            startProcess(*it, *cmdIt, QIODevice::ReadWrite);
        startProcess(lastProcess, cmds.last(), m_readOutput ? QIODevice::ReadWrite : QIODevice::WriteOnly);
    }
}

bool Action::waitForFinished(int msecs)
{
    if ( !isRunning() )
        return true;

    QPointer<QObject> self(this);
    QEventLoop loop;
    QTimer t;
    connect(this, &Action::actionFinished, &loop, &QEventLoop::quit);
    if (msecs >= 0) {
        connect(&t, &QTimer::timeout, &loop, &QEventLoop::quit);
        t.setSingleShot(true);
        t.start(msecs);
    }
    loop.exec(QEventLoop::ExcludeUserInputEvents);

    // Loop stopped because application is exiting?
    while ( self && isRunning() && (msecs < 0 || t.isActive()) )
        QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents, 10);

    return !self || !isRunning();
}

bool Action::isRunning() const
{
    return !m_processes.empty() && m_processes.back()->state() != QProcess::NotRunning;
}

void Action::setData(const QVariantMap &data)
{
    m_data = data;
}

const QVariantMap &Action::data() const
{
    return m_data;
}

void Action::appendOutput(const QByteArray &output)
{
    if ( !output.isEmpty() )
        emit actionOutput(output);
}

void Action::appendErrorOutput(const QByteArray &errorOutput)
{
    m_errorOutput.append(errorOutput);
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
        finish();
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
    if ( m_processes.empty() )
        return;

    auto p = m_processes.back();
    if ( p->isReadable() )
        appendOutput( p->readAll() );
}

void Action::onSubProcessErrorOutput()
{
    QProcess *p = qobject_cast<QProcess*>(sender());
    Q_ASSERT(p);

    if ( p->isReadable() )
        appendErrorOutput( p->readAllStandardError() );
}

void Action::writeInput()
{
    if (m_processes.empty())
        return;

    QProcess *p = m_processes.front();

    if (m_input.isEmpty())
        p->closeWriteChannel();
    else
        p->write(m_input);
}

void Action::onBytesWritten()
{
    if ( !m_processes.empty() )
        m_processes.front()->closeWriteChannel();
}

void Action::terminate()
{
    if (m_processes.empty())
        return;

    for (auto p : m_processes)
        p->terminate();

    waitForFinished(5000);
    for (auto p : m_processes)
        terminateProcess(p);
}

void Action::closeSubCommands()
{
    terminate();

    if (m_processes.empty())
        return;

    m_exitCode = m_processes.back()->exitCode();
    m_failed = m_failed || m_processes.back()->exitStatus() != QProcess::NormalExit;

    for (auto p : m_processes)
        p->deleteLater();

    m_processes.clear();
}

void Action::finish()
{
    closeSubCommands();
    emit actionFinished(this);
}
