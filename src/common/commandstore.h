// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include <QtContainerFwd>

class QSettings;
struct Command;

using Commands = QVector<Command>;

Commands loadAllCommands();
void saveCommands(const Commands &commands);

Commands loadCommands(QSettings *settings);
void saveCommands(const Commands &commands, QSettings *settings);

Commands importCommandsFromFile(const QString &filePath);
Commands importCommandsFromText(const QString &commands);
QString exportCommands(const Commands &commands);
