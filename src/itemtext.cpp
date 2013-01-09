/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#include "itemtext.h"

#include <QPaintEvent>
#include <QPainter>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>

namespace {

// Limit number of characters for performance reasons.
const int maxChars = 100*1024;

void init(QTextDocument &doc, const QFont &font)
{
    doc.setDefaultFont(font);
    doc.setUndoRedoEnabled(false);
}

} // namespace

ItemText::ItemText(const QString &text, Qt::TextFormat format, QWidget *parent)
    : QWidget(parent)
    , ItemWidget(this)
    , m_textDocument()
    , m_searchTextDocument()
    , m_textFormat(format)
{
    init(m_textDocument, font());
    init(m_searchTextDocument, font());

    if (m_textFormat == Qt::PlainText)
        m_textDocument.setPlainText( text.left(maxChars) );
    else
        m_textDocument.setHtml( text.left(maxChars) );

    updateSize();
    updateItem();
}

void ItemText::highlight(const QRegExp &re, const QFont &highlightFont, const QPalette &highlightPalette)
{
    m_searchTextDocument.clear();
    if ( !re.isEmpty() ) {
        bool plain = m_textFormat == Qt::PlainText;
        const QString &text = plain ? m_textDocument.toPlainText() : m_textDocument.toHtml();
        if (plain)
            m_searchTextDocument.setPlainText(text);
        else
            m_searchTextDocument.setHtml(text);

        QTextCursor cur = m_searchTextDocument.find(re);
        int a = cur.position();
        while ( !cur.isNull() ) {
            QTextCharFormat fmt = cur.charFormat();
            if ( cur.hasSelection() ) {
                fmt.setBackground( highlightPalette.base() );
                fmt.setForeground( highlightPalette.text() );
                fmt.setFont(highlightFont);
                cur.setCharFormat(fmt);
            } else {
                cur.movePosition(QTextCursor::NextCharacter);
            }
            cur = m_searchTextDocument.find(re, cur);
            int b = cur.position();
            if (a == b) {
                cur.movePosition(QTextCursor::NextCharacter);
                cur = m_searchTextDocument.find(re, cur);
                b = cur.position();
                if (a == b) break;
            }
            a = b;
        }
    }

    update();
}

void ItemText::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    QTextDocument &doc = !m_searchTextDocument.isEmpty() ? m_searchTextDocument : m_textDocument;
    doc.drawContents(&painter, event->rect());
}

void ItemText::updateSize()
{
    const int w = maximumWidth();
    m_searchTextDocument.setTextWidth(w);
    m_textDocument.setTextWidth(w);
    resize( m_textDocument.idealWidth(), m_textDocument.size().height() );
}
