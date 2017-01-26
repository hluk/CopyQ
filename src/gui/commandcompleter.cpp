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

#include "commandcompleter.h"
#include "commandsyntaxhighlighter.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QScrollBar>

#include <QDebug>

namespace {

QStringList scriptableCompletions()
{
    return scriptableObjects()
         + scriptableProperties()
         + scriptableFunctions()
         + scriptableKeywords();
}

} // namespace

CommandCompleter::CommandCompleter(QPlainTextEdit *editor)
    : QObject(editor)
    , m_editor(editor)
    , m_completer(new QCompleter(scriptableCompletions(), this))
{
    m_completer->setWidget(m_editor);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);

    m_completer->popup()->installEventFilter(this);

    connect( m_completer, SIGNAL(activated(QString)),
             this, SLOT(insertCompletion(QString)) );

    connect( m_editor, SIGNAL(textChanged()),
             this, SLOT(updateCompletion()) );
    connect( m_editor, SIGNAL(cursorPositionChanged()),
             m_completer->popup(), SLOT(hide()) );
}

bool CommandCompleter::eventFilter(QObject *, QEvent *event)
{
    if (event->type() != QEvent::KeyPress)
        return false;

    QKeyEvent *e = static_cast<QKeyEvent*>(event);
    QAbstractItemView *popup = m_completer->popup();

    switch (e->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        m_completer->setCurrentRow( popup->currentIndex().row() );
        insertCompletion( m_completer->currentCompletion() );
        popup->hide();
        return true;

    case Qt::Key_Escape:
        popup->hide();
        return true;

    default:
        return false;
    }
}

void CommandCompleter::updateCompletion()
{
    const QString completionPrefix = textUnderCursor();

    QAbstractItemView *popup = m_completer->popup();

    if ( completionPrefix.length() < 3 ) {
        popup->hide();
    } else {
        if (completionPrefix != m_completer->completionPrefix()) {
            m_completer->setCompletionPrefix(completionPrefix);
            popup->setCurrentIndex(m_completer->completionModel()->index(0, 0));
        }

        QRect rect = m_editor->cursorRect();
        QScrollBar *scrollBar = popup->verticalScrollBar();
        rect.setWidth(
                    popup->sizeHintForColumn(0)
                    + (scrollBar ? scrollBar->sizeHint().width() : 0) );
        m_completer->complete(rect);
    }
}

void CommandCompleter::insertCompletion(const QString &completion)
{
    QTextCursor tc = m_editor->textCursor();
    int extra = completion.length() - m_completer->completionPrefix().length();
    tc.movePosition(QTextCursor::Left);
    tc.movePosition(QTextCursor::EndOfWord);
    tc.insertText(completion.right(extra));
    m_editor->setTextCursor(tc);
}

QString CommandCompleter::textUnderCursor() const
{
    QTextCursor tc = m_editor->textCursor();
    tc.select(QTextCursor::WordUnderCursor);
    return tc.selectedText();
}
