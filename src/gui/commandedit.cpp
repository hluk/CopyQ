/*
    Copyright (c) 2015, Lukas Holecek <hluk@email.cz>

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

#include <QScriptEngine>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

CommandEdit::CommandEdit(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::CommandEdit)
{
    ui->setupUi(this);
    ui->labelErrors->hide();
    ui->plainTextEditCommand->document()->setDefaultFont(commandFont());

    setFocusProxy(ui->plainTextEditCommand);

    installCommandSyntaxHighlighter(ui->plainTextEditCommand);

    connect( ui->helpButton, SIGNAL(hidden()),
             ui->plainTextEditCommand, SLOT(setFocus()) );
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

void CommandEdit::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateCommandEditSize();
}

void CommandEdit::on_plainTextEditCommand_textChanged()
{
    updateCommandEditSize();
    emit changed();

    QString errors;
    QList<QTextEdit::ExtraSelection> selections;

    QString command = ui->plainTextEditCommand->toPlainText();
    QRegExp scriptPrefix("(^|\\b)copyq:");
    const int pos = command.indexOf(scriptPrefix);

    if (pos != -1) {
        command.remove(0, pos + scriptPrefix.matchedLength());

        const QScriptSyntaxCheckResult result = QScriptEngine::checkSyntax(command);

        if (result.state() == QScriptSyntaxCheckResult::Error) {
            errors = result.errorMessage();

            const int line = result.errorLineNumber() - 1;
            int column = result.errorColumnNumber();
            QTextDocument *doc = ui->plainTextEditCommand->document();
            const int firstLine = doc->findBlock(pos).firstLineNumber();
            QTextBlock block = doc->findBlockByLineNumber(firstLine + line);
            if (firstLine == block.firstLineNumber())
                column += scriptPrefix.matchedLength();

            QTextCursor cursor = ui->plainTextEditCommand->textCursor();
            cursor.setPosition( block.position() + column );
            cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);

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
}

void CommandEdit::updateCommandEditSize()
{
    const QFontMetrics fm( ui->plainTextEditCommand->document()->defaultFont() );
    const int lines = ui->plainTextEditCommand->document()->size().height() + 2;
    const int visibleLines = qBound(3, lines, 20);
    const int h = visibleLines * fm.lineSpacing();
    ui->plainTextEditCommand->setMinimumHeight(h);
}
