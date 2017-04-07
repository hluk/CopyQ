/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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

void CommandEdit::resizeToContent()
{
    auto document = ui->plainTextEditCommand->document();
    QTextCursor tc(document);
    tc.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    const auto cursorRect = ui->plainTextEditCommand->cursorRect(tc);
    const auto h = static_cast<int>( cursorRect.bottom() + 2 * cursorRect.height() );
    const auto margins = ui->plainTextEditCommand->contentsMargins();
    ui->plainTextEditCommand->setMinimumHeight( h + margins.top() + margins.bottom() );
}

void CommandEdit::on_plainTextEditCommand_textChanged()
{
    emit changed();
    // TODO: Highlight syntax errors!
}
