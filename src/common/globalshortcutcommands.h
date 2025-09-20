// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include <QtContainerFwd>

class QString;
struct Command;

QVector<Command> globalShortcutCommands();

QString pasteAsPlainTextScript(const QString &what);
