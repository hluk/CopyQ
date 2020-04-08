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

#ifndef COMMANDACTION_H
#define COMMANDACTION_H

#include "common/command.h"

#include <QAction>
#include <QPointer>

class CommandAction final : public QAction
{
    Q_OBJECT
public:
    CommandAction(const Command &command,
            const QString &name,
            QMenu *parentMenu = nullptr);

    const Command &command() const;

signals:
    void triggerCommand(CommandAction *self, const QString &triggeredShortcut);

protected:
    bool event(QEvent *event) override;

private:
    void onTriggered();
    void onChanged();

    Command m_command;
    QString m_triggeredShortcut;
};

#endif // COMMANDACTION_H
