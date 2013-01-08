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

#include <QTextCursor>
#include <QTextDocument>

ItemText::ItemText(const QString &text, QWidget *parent)
    : QLabel(parent)
    , ItemWidget(this)
    , m_labelText()
{
    setMargin(4);
    setAutoFillBackground(false);
    setTextFormat(Qt::PlainText);
    setText(text);
    adjustSize();
    updateItem();
}

void ItemText::highlight(const QRegExp &re, const QFont &highlightFont, const QPalette &highlightPalette)
{
    if ( re.isEmpty() ) {
        if ( !m_labelText.isNull() ) {
            setTextFormat(Qt::PlainText);
            setText(m_labelText);
            m_labelText = QString();
        }
    } else {
        if ( m_labelText.isNull() )
            m_labelText = text();
        QTextDocument doc(m_labelText);
        doc.setDefaultFont( font() );

        QTextCursor cur = doc.find(re);
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
            cur = doc.find(re, cur);
            int b = cur.position();
            if (a == b) {
                cur.movePosition(QTextCursor::NextCharacter);
                cur = doc.find(re, cur);
                b = cur.position();
                if (a == b) break;
            }
            a = b;
        }

        setTextFormat(Qt::RichText);
        setText(doc.toHtml());
    }
}
