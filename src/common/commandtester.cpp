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

#include "commandtester.h"

#include "common/action.h"
#include "common/mimetypes.h"
#include "common/textdata.h"

#include <QProcess>
#include <QCoreApplication>

CommandTester::CommandTester(QObject *parent)
    : QObject(parent)
    , m_action(nullptr)
    , m_state(CommandTesterState::NotRunning)
{
}

void CommandTester::abort()
{
    if (m_action)
        m_state = CommandTesterState::Aborting;
    else
        m_state = CommandTesterState::NotRunning;
}

void CommandTester::setCommands(const QVector<Command> &commands)
{
    abort();
    m_currentCommandIndex = 0;
    m_commands = commands;
}

bool CommandTester::isCompleted() const
{
    return !m_action;
}

bool CommandTester::hasCommands() const
{
    return m_currentCommandIndex < m_commands.size();
}

QVariantMap CommandTester::data() const
{
    return m_data;
}

void CommandTester::setData(const QVariantMap &data)
{
    abort();
    m_currentCommandIndex = 0;
    m_data = data;
}

void CommandTester::waitForAction(Action *action)
{
    Q_ASSERT(!m_action);
    m_action = action;
    connect(action, SIGNAL(destroyed()),
            this, SLOT(onActionFinished()));
    connect(action, SIGNAL(dataChanged(QVariantMap)),
            this, SLOT(onDataChanged(QVariantMap)));
}

void CommandTester::start()
{
    if (m_action) {
        m_state = CommandTesterState::Restarting;
    } else {
        m_state = CommandTesterState::Running;
        m_currentCommandIndex = 0;
        startNext();
    }
}

void CommandTester::onTestActionFinished()
{
    Q_ASSERT(m_action);
    Q_ASSERT(!m_action->isRunning());

    const bool passed = !m_action->actionFailed() && m_action->exitCode() == 0;
    m_action->deleteLater();
    m_action = nullptr;

    if (m_state == CommandTesterState::Aborting)
        abort();
    else if (m_state == CommandTesterState::Restarting)
        start();
    else
        commandPassed(passed);
}

void CommandTester::onActionFinished()
{
    m_action = nullptr;
    startNext();
}

void CommandTester::onDataChanged(const QVariantMap &data)
{
    if (m_state == CommandTesterState::Running)
        m_data = data;
}

void CommandTester::startNext()
{
    Q_ASSERT(!m_action);

    if (m_state == CommandTesterState::Aborting) {
        abort();
        return;
    }

    if (m_state == CommandTesterState::Restarting) {
        start();
        return;
    }

    if ( !hasCommands() || m_state != CommandTesterState::Running ) {
        m_state = CommandTesterState::NotRunning;
        emit finished();
        return;
    }

    Command *command = &m_commands[m_currentCommandIndex];

    if (command->matchCmd.isEmpty()) {
        commandPassed(true);
    } else {
        m_action = new Action(this);

        const QString text = getTextData(m_data);
        m_action->setInput(text.toUtf8());
        m_action->setData(m_data);
        m_action->setIgnoreExitCode(true);

        const QString arg = getTextData(m_action->input());
        m_action->setCommand(command->matchCmd, QStringList(arg));

        connect(m_action, SIGNAL(actionFinished(Action*)), SLOT(onTestActionFinished()));

        emit requestActionStart(m_action);
    }
}

QString CommandTester::currentActionName() const
{
    return m_action ? m_action->name() : QString();
}

void CommandTester::commandPassed(bool passed)
{
    Q_ASSERT(hasCommands());
    const Command &command = m_commands[m_currentCommandIndex];
    ++m_currentCommandIndex;
    emit commandPassed(command, passed);
}
