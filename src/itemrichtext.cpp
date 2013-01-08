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

#include "include/itemrichtext.h"

#include <QTextDocument>
#include <QAbstractTextDocumentLayout>

ItemRichText::ItemRichText(const QString &html, QWidget *parent)
    : QTextEdit(parent)
    , ItemWidget(this)
    , m_doc(NULL)
{
    setFrameShape(QFrame::NoFrame);
    setWordWrapMode(QTextOption::NoWrap);
    setUndoRedoEnabled(false);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // text or HTML
    QTextDocument *doc = new QTextDocument(this);
    doc->setHtml(html);

    setDocument(doc);
    viewport()->setAutoFillBackground(false);
    QSize size = doc->documentLayout()->documentSize().toSize();
    resize(size);
    updateItem();
}

void ItemRichText::highlight(const QRegExp &re, const QFont &highlightFont, const QPalette &highlightPalette)
{
    if ( re.isEmpty() ) {
        if (m_doc != NULL) {
            QTextDocument *doc = document();
            setDocument(m_doc);
            m_doc = NULL;
            delete doc;
        }
    } else {
        m_doc = document();
        QTextDocument *doc = m_doc->clone(this);
        setDocument(doc);

        int a, b;
        QTextCursor cur = doc->find(re);
        a = cur.position();
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
            cur = doc->find(re, cur);
            b = cur.position();
            if (a == b) {
                cur.movePosition(QTextCursor::NextCharacter);
                cur = doc->find(re, cur);
                b = cur.position();
                if (a == b) break;
            }
            a = b;
        }
    }
}
