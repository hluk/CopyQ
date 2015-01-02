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

CommandEdit::CommandEdit(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::CommandEdit)
{
    ui->setupUi(this);
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
}

void CommandEdit::updateCommandEditSize()
{
    const QFontMetrics fm( ui->plainTextEditCommand->document()->defaultFont() );
    const int lines = ui->plainTextEditCommand->document()->size().height() + 2;
    const int visibleLines = qBound(3, lines, 20);
    const int h = visibleLines * fm.lineSpacing();
    ui->plainTextEditCommand->setMinimumHeight(h);
}
