// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef COMMANDSTORE_H
#define COMMANDSTORE_H

#include "common/command.h"

#include <QVector>

class QSettings;

using Commands = QVector<Command>;

Commands loadAllCommands();
void saveCommands(const Commands &commands);

Commands loadCommands(QSettings *settings);
void saveCommands(const Commands &commands, QSettings *settings);

Commands importCommandsFromFile(const QString &filePath);
Commands importCommandsFromText(const QString &commands);
QString exportCommands(const Commands &commands);

#endif // COMMANDSTORE_H
