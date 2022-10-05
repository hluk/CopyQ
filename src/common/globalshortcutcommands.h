// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GLOBALSHORTCUTCOMMANDS_H
#define GLOBALSHORTCUTCOMMANDS_H

#include "common/command.h"

#include <QtContainerFwd>

class QString;

QVector<Command> globalShortcutCommands();

QString pasteAsPlainTextScript(const QString &what);

#endif // GLOBALSHORTCUTCOMMANDS_H
