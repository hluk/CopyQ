/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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
#include "item/serialize.h"

#include <QCoreApplication>
#include <QProcessEnvironment>
#include <QPointer>

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

void startWritingInput(const QByteArray &input, QPointer<QProcess> p)
{
    if (!p)
        return;

    p->write(input);
    p->closeWriteChannel();

    if (!input.isEmpty()) {
        while ( p && !p->waitForBytesWritten(0) )
            QCoreApplication::processEvents();
    }
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
                    command << "copyq" << "eval" << "--" << script;
                else if ( getScriptFromLabel("sh:", c, &script) )
                    command << "sh" << "-c" << "--" << script << "--";
                else if ( getScriptFromLabel("bash:", c, &script) )
                    command << "bash" << "-c" << "--" << script << "--";
                else if ( getScriptFromLabel("perl:", c, &script) )
                    command << "perl" << "-e" << script << "--";
                else if ( getScriptFromLabel("python:", c, &script) )
                    command << "python" << "-c" << script;
                else if ( getScriptFromLabel("ruby:", c, &script) )
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

quintptr actionId(const Action *act)
{
    return reinterpret_cast<quintptr>(act);
}

} // namespace

QMutex Action::actionsLock;
QVector<Action*> Action::actions;

Action::Action(QObject *parent)
    : QObject(parent)
    , m_failed(false)
    , m_currentLine(-1)
    , m_exitCode(0)
{
    setProperty("COPYQ_ACTION_ID", actionId(this));

    const QMutexLocker lock(&actionsLock);
    actions.append(this);
}

Action::~Action()
{
    closeSubCommands();

    const QMutexLocker lock(&actionsLock);
    const int i = actions.indexOf(this);
    Q_ASSERT(i != -1);
    actions.remove(i);
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
        emit actionFinished(this);
        return;
    }

    ++m_currentLine;
    const QList<QStringList> &cmds = m_cmds[m_currentLine];

    Q_ASSERT( !cmds.isEmpty() );

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("COPYQ_ACTION_ID", QString::number(actionId(this)));

    for (int i = 0; i < cmds.size(); ++i) {
        m_processes.append(new QProcess(this));
        m_processes.last()->setProcessEnvironment(env);

        connect( m_processes.last(), SIGNAL(error(QProcess::ProcessError)),
                 SLOT(actionError(QProcess::ProcessError)) );
        connect( m_processes.last(), SIGNAL(readyReadStandardError()),
                 SLOT(actionErrorOutput()) );
    }

    for (int i = 1; i < m_processes.size(); ++i) {
        m_processes[i - 1]->setStandardOutputProcess(m_processes[i]);
        connect( m_processes[i], SIGNAL(finished(int)),
                 m_processes[i - 1], SLOT(terminate()) );
    }

    connect( m_processes.last(), SIGNAL(started()),
             this, SLOT(actionStarted()) );
    connect( m_processes.last(), SIGNAL(finished(int,QProcess::ExitStatus)),
             this, SLOT(actionFinished()) );
    connect( m_processes.last(), SIGNAL(readyReadStandardOutput()),
             this, SLOT(actionOutput()) );

    // Writing directly to stdin of a process on Windows can hang the app.
    connect( m_processes.first(), SIGNAL(started()),
             this, SLOT(writeInput()), Qt::QueuedConnection );

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
    QCoreApplication::processEvents();
    return m_processes.isEmpty() || m_processes.last()->waitForFinished(msecs);
}

bool Action::isRunning() const
{
    return !m_processes.isEmpty() && m_processes.last()->state() != QProcess::NotRunning;
}

void Action::setData(const QVariantMap &data)
{
    m_data = data;
}

const QVariantMap &Action::data() const
{
    return m_data;
}

QVariantMap Action::data(quintptr id)
{
    const QMutexLocker lock(&actionsLock);
    const int i = actions.indexOf(reinterpret_cast<Action*>(id));
    const Action *action = actions.value(i);
    return action ? action->m_data : QVariantMap();
}

void Action::setData(quintptr id, const QVariantMap &data)
{
    const QMutexLocker lock(&actionsLock);
    const int i = actions.indexOf(reinterpret_cast<Action*>(id));
    Action *action = actions.value(i);
    if (action && action->m_data != data) {
        action->m_data = data;
        emit action->dataChanged(data);
    }
}

void Action::actionError(QProcess::ProcessError)
{
    QProcess *p = qobject_cast<QProcess*>(sender());
    Q_ASSERT(p);

    if (!m_errorString.isEmpty())
        m_errorString.append("\n");
    m_errorString.append( p->errorString() );
    m_failed = true;

    if ( !isRunning() ) {
        closeSubCommands();
        emit actionFinished(this);
    }
}

void Action::actionStarted()
{
    if (m_currentLine == 0)
        emit actionStarted(this);
}

void Action::actionFinished()
{
    actionOutput();

    if (hasTextOutput()) {
        if (canEmitNewItems()) {
            m_items.append(m_lastOutput);
            if (m_index.isValid())
                emit newItems(m_items, m_index);
            else
                emit newItems(m_items, m_tab);
            m_lastOutput = QString();
            m_items.clear();
        }
    } else if (canEmitNewItems()) {
        if (m_index.isValid())
            emit newItem(m_outputData, m_outputFormat, m_index);
        else
            emit newItem(m_outputData, m_outputFormat, m_tab);
        m_outputData = QByteArray();
    }

    start();
}

void Action::actionOutput()
{
    QProcess *p = qobject_cast<QProcess*>(sender());
    Q_ASSERT(p);

    const QByteArray output = p->readAll();

    if (hasTextOutput()) {
        m_lastOutput.append( getTextData(output) );
        if ( !m_lastOutput.isEmpty() && !m_sep.isEmpty() ) {
            // Split to items.
            QStringList items;
            items = m_lastOutput.split(m_sep);
            m_lastOutput = items.takeLast();
            if (m_index.isValid()) {
                emit newItems(items, m_index);
            } else if (!m_tab.isEmpty()) {
                emit newItems(items, m_tab);
            }
        }
    } else if (!m_outputFormat.isEmpty()) {
        m_outputData.append(output);
    }
}

void Action::actionErrorOutput()
{
    QProcess *p = qobject_cast<QProcess*>(sender());
    Q_ASSERT(p);

    m_errstr.append( getTextData(p->readAllStandardError()) );
}

void Action::writeInput()
{
    if (m_processes.value(0) == sender())
        startWritingInput(m_input, m_processes.value(0));
}

bool Action::hasTextOutput() const
{
    return !m_outputFormat.isEmpty() && m_outputFormat == mimeText;
}

void Action::terminate()
{
    if (m_processes.isEmpty())
        return;

    // try to terminate process
    foreach (QProcess *p, m_processes)
        p->terminate();

    // if process still running: kill it
    if ( !m_processes.last()->waitForFinished(5000) )
        m_processes.last()->kill();
}

bool Action::canEmitNewItems() const
{
    return (m_index.isValid() || !m_tab.isEmpty())
            && ( (!m_outputFormat.isEmpty() && !m_outputData.isNull())
                 || (m_outputFormat == mimeText && !m_lastOutput.isEmpty()) );
}

void Action::closeSubCommands()
{
    if (m_processes.isEmpty())
        return;

    m_exitCode = m_processes.last()->exitCode();
    m_failed = m_failed || m_processes.last()->exitStatus() != QProcess::NormalExit;

    foreach (QProcess *p, m_processes)
        p->deleteLater();

    m_processes.clear();
}
