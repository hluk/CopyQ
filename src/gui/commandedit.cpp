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

#include "commandedit.h"
#include "ui_commandedit.h"

#include "gui/commandsyntaxhighlighter.h"
#include "gui/commandcompleter.h"

#include <QRegularExpression>
#include <QScriptEngine>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

CommandEdit::CommandEdit(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::CommandEdit)
{
    ui->setupUi(this);

    connect(ui->plainTextEditCommand, &QPlainTextEdit::textChanged,
            this, &CommandEdit::onPlainTextEditCommandTextChanged);

    ui->labelErrors->hide();
    ui->plainTextEditCommand->document()->setDefaultFont(commandFont());

    auto document = ui->plainTextEditCommand->document();
    QTextCursor tc(document);
    const QRect cursorRect = ui->plainTextEditCommand->cursorRect(tc);
    const QMargins margins = ui->plainTextEditCommand->contentsMargins();
    const int minimumHeight = 3 * cursorRect.height() / 2
            + margins.top() + margins.bottom();
    ui->plainTextEditCommand->setMinimumHeight(minimumHeight);

    setFocusProxy(ui->plainTextEditCommand);

    installCommandSyntaxHighlighter(ui->plainTextEditCommand);

    new CommandCompleter(ui->plainTextEditCommand);
}

CommandEdit::~CommandEdit()
{
    delete ui;
}

void CommandEdit::setCommand(const QString &command) const
{
    ui->plainTextEditCommand->setPlainText(command);
}

QString CommandEdit::command() const
{
    return ui->plainTextEditCommand->toPlainText();
}

bool CommandEdit::isEmpty() const
{
    return ui->plainTextEditCommand->document()->characterCount() == 0;
}

QFont CommandEdit::commandFont() const
{
    QFont font("Monospace");
    font.setStyleHint(QFont::TypeWriter);
    font.setPointSize(10);
    return font;
}

void CommandEdit::onPlainTextEditCommandTextChanged()
{
    QString errors;
    QList<QTextEdit::ExtraSelection> selections;

    const QString command = ui->plainTextEditCommand->toPlainText();
    const QRegularExpression scriptPrefix("(^|\\b)copyq:");
    const auto m = scriptPrefix.match(command);
    const int pos = m.capturedStart();

    if (pos != -1) {
        const int scriptStartPos = pos + m.capturedLength();

        const QScriptSyntaxCheckResult result = QScriptEngine::checkSyntax(command.mid(scriptStartPos));

        if (result.state() == QScriptSyntaxCheckResult::Error) {
            errors = result.errorMessage();
            if (errors.isEmpty())
                errors = "Syntax error";

            const int line = result.errorLineNumber() - 1;
            const int column = result.errorColumnNumber() - 1;
            QTextDocument *doc = ui->plainTextEditCommand->document();
            const int firstBlockNumber = doc->findBlock(pos).blockNumber();
            QTextBlock block = doc->findBlockByNumber(firstBlockNumber + line);
            const int blockStartPos = line == 0 ? scriptStartPos : block.position();
            const int errorPosition = column + blockStartPos;

            QTextCursor cursor = ui->plainTextEditCommand->textCursor();
            cursor.setPosition(errorPosition);
            cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);

            QTextEdit::ExtraSelection selection;
            selection.cursor = cursor;
            selection.format = cursor.blockCharFormat();
            selection.format.setUnderlineColor(Qt::red);
            selection.format.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
            selections.append(selection);
        }
    }

    ui->plainTextEditCommand->setExtraSelections(selections);
    ui->labelErrors->setVisible(!errors.isEmpty());
    ui->labelErrors->setText(errors);

    emit changed();
    emit commandTextChanged(command);
}
