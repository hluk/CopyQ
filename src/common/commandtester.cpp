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

#include "commandtester.h"

#include "common/action.h"
#include "common/common.h"
#include "common/mimetypes.h"

#include <QProcess>
#include <QCoreApplication>

CommandTester::CommandTester(QObject *parent)
    : QObject(parent)
    , m_action(NULL)
    , m_removeOrTransform(false)
    , m_abort(false)
{
}

void CommandTester::start()
{
    if (!m_action)
        startNext();
}

void CommandTester::abort()
{
    if (m_action) {
        m_abort = true;
        m_action->terminate(0);
    }
}

void CommandTester::setCommands(
        const QList<Command> &commands, const QVariantMap &data)
{
    m_commands = commands;
    m_data = data;
    m_removeOrTransform = false;
    abort();
    start();
}

void CommandTester::actionFinished()
{
    Q_ASSERT(m_action);
    Q_ASSERT(m_action->state() == QProcess::NotRunning);

    bool passed = !m_action->actionFailed() && m_action->exitCode() == 0;
    delete m_action;
    m_action = NULL;

    if (m_abort) {
        start();
    } else {
        commandPassed(passed);
        if (!maybeFinish())
            startNext();
    }
}

void CommandTester::startNext()
{
    Q_ASSERT(!m_action);

    m_abort = false;

    if (!maybeFinish()) {
        Command *command = &m_commands[0];

        while (command->matchCmd.isEmpty()) {
            commandPassed(true);
            if (maybeFinish())
                return;
            command = &m_commands[0];
        }

        m_action = new Action(this);

        const QString text = getTextData(m_data);
        m_action->setInput(text.toUtf8());
        m_action->setData(m_data);

        const QString arg = QString::fromUtf8(m_action->input());
        m_action->setCommand(command->matchCmd, QStringList(arg));

        connect(m_action, SIGNAL(actionFinished(Action*)), SLOT(actionFinished()));
        m_action->start();
    }
}

void CommandTester::commandPassed(bool passed)
{
    Q_ASSERT(!m_commands.isEmpty());
    const Command command = m_commands.takeFirst();
    m_removeOrTransform = passed && (command.remove || command.transform);
    emit commandPassed(command, passed, m_data);
}

bool CommandTester::maybeFinish()
{
    if (!m_commands.isEmpty())
        return false;

    emit finished(m_data, m_removeOrTransform);
    return true;
}
