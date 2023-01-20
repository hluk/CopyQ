// SPDX-License-Identifier: GPL-3.0-or-later

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
