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

#ifndef COMMANDSYNTAXHIGHLIGHTER_H
#define COMMANDSYNTAXHIGHLIGHTER_H

#include <QtContainerFwd>

class QPlainTextEdit;
class QTextEdit;

QList<QString> scriptableKeywords();
QList<QString> scriptableProperties();
QList<QString> scriptableFunctions();
/// Constructors and functions from ECMA specification supported by Qt plus ByteArray.
QList<QString> scriptableObjects();

void installCommandSyntaxHighlighter(QTextEdit *editor);
void installCommandSyntaxHighlighter(QPlainTextEdit *editor);

#endif // COMMANDSYNTAXHIGHLIGHTER_H
