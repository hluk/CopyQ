// SPDX-License-Identifier: GPL-3.0-or-later

#include "commandedit.h"
#include "ui_commandedit.h"

#include "gui/commandsyntaxhighlighter.h"
#include "gui/commandcompleter.h"

#include <QRegularExpression>
#include <QJSEngine>
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

void CommandEdit::setReadOnly(bool readOnly)
{
    ui->plainTextEditCommand->setReadOnly(readOnly);
}

void CommandEdit::onPlainTextEditCommandTextChanged()
{
    // TODO: Highlight syntax errors!
    const QString command = ui->plainTextEditCommand->toPlainText();
    emit changed();
    emit commandTextChanged(command);
}
