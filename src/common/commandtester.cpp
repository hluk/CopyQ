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

CommandTester::CommandTester(
        const QList<Command> &commands,
        const QVariantMap &data,
        QObject *parent)
    : QObject(parent)
    , m_commands(commands)
    , m_data(data)
    , m_abort(false)
{
    qRegisterMetaType<Command>("Command");
    qRegisterMetaType<quintptr>("quintptr");
}

void CommandTester::start()
{
    bool removeOrTransform = false;

    foreach (const Command &command, m_commands) {
        bool passed = canExecuteCommand(command);
        if (m_abort)
            break;

        removeOrTransform = passed && (command.remove || command.transform);
        emit commandPassed(reinterpret_cast<quintptr>(this), command, passed, m_data);
    }

    emit finished(m_data, removeOrTransform);
}

void CommandTester::abort()
{
    m_abort = true;
}

bool CommandTester::canExecuteCommand(const Command &command)
{
    if (command.matchCmd.isEmpty())
        return true;

    const QString text = getTextData(m_data);

    Action matchAction;
    matchAction.setCommand(command.matchCmd, QStringList(text));
    matchAction.setInput(text.toUtf8());
    matchAction.setData(m_data);
    matchAction.start();

    for ( int i = 0; i < 100 && !matchAction.waitForFinished(100) && !m_abort; ++i )
        QCoreApplication::processEvents();

    if (matchAction.state() != QProcess::NotRunning) {
        matchAction.terminate();
        return false;
    }

    if (matchAction.exitStatus() != QProcess::NormalExit || matchAction.exitCode() != 0)
        return false;

    return true;
}
