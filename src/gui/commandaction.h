// SPDX-License-Identifier: GPL-3.0-or-later

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
