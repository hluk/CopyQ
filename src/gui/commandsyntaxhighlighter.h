// SPDX-License-Identifier: GPL-3.0-or-later

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
