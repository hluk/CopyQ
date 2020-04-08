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

#include "commandaction.h"

#include "common/common.h"
#include "common/mimetypes.h"
#include "common/shortcuts.h"
#include "common/textdata.h"
#include "gui/iconfactory.h"

#include <QMenu>
#include <QShortcutEvent>

CommandAction::CommandAction(
        const Command &command,
        const QString &name,
        QMenu *parentMenu)
    : QAction(parentMenu)
    , m_command(command)
{
    setText( elideText(name, parentMenu->font(), QString()) );

    setIcon( iconFromFile(m_command.icon) );
    if (m_command.icon.size() == 1)
        setProperty( "CopyQ_icon_id", m_command.icon[0].unicode() );

    connect(this, &QAction::triggered, this, &CommandAction::onTriggered);

    parentMenu->addAction(this);
}

const Command &CommandAction::command() const
{
    return m_command;
}

bool CommandAction::event(QEvent *event)
{
    if (event->type() == QEvent::Shortcut) {
        QShortcutEvent *shortcutEvent = static_cast<QShortcutEvent*>(event);
        m_triggeredShortcut = portableShortcutText(shortcutEvent->key());
    }

    return QAction::event(event);
}

void CommandAction::onTriggered()
{
    const auto triggeredShortcut = m_triggeredShortcut;
    m_triggeredShortcut.clear();
    emit triggerCommand(this, triggeredShortcut);
}
