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

#ifndef COMMANDTESTER_H
#define COMMANDTESTER_H

#include "common/command.h"

#include <QObject>
#include <QVariant>

Q_DECLARE_METATYPE(Command)
Q_DECLARE_METATYPE(quintptr)

class CommandTester : public QObject
{
    Q_OBJECT
public:
    CommandTester(
            const QList<Command> &commands,
            const QVariantMap &data,
            QObject *parent = NULL);

signals:
    void commandPassed(quintptr id, const Command &command, bool passed, const QVariantMap &data);
    void finished(const QVariantMap &data, bool removeOrTransform);

public slots:
    void start();
    void abort();

private:
    bool canExecuteCommand(const Command &command);

    QList<Command> m_commands;
    QVariantMap m_data;
    bool m_abort;
};

#endif // COMMANDTESTER_H
