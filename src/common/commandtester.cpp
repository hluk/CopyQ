/*
    Copyright (c) 2015, Lukas Holecek <hluk@email.cz>

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
    , m_abort(false)
{
}

void CommandTester::abort()
{
    m_commands.clear();
    m_data.clear();

    if (m_action)
        m_abort = true;
}

void CommandTester::setCommands(
        const QList<Command> &commands, const QVariantMap &data)
{
    abort();
    m_commands = commands;
    m_data = data;
}

bool CommandTester::isCompleted() const
{
    return !m_action;
}

bool CommandTester::hasCommands() const
{
    return !m_commands.isEmpty();
}

const QVariantMap &CommandTester::data() const
{
    return m_data;
}

void CommandTester::start()
{
    if (!m_action)
        startNext();
}

void CommandTester::actionFinished()
{
    Q_ASSERT(m_action);
    Q_ASSERT(!m_action->isRunning());

    bool passed = !m_action->actionFailed() && m_action->exitCode() == 0;
    m_action->deleteLater();
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
    Q_ASSERT(hasCommands());
    const Command command = m_commands.takeFirst();
    emit commandPassed(command, passed);
}

bool CommandTester::maybeFinish()
{
    return !hasCommands();
}
