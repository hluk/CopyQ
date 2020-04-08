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

#ifndef COMMANDSTORE_H
#define COMMANDSTORE_H

#include <QVector>

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

#endif // COMMANDSTORE_H
