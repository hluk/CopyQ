/*
    Copyright (c) 2009, Lukas Holecek <hluk@email.cz>

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

#include "clipboarditem.h"
#include <QTextCursor>
#include <QPainter>
#include <QDebug>

ClipboardItem::ClipboardItem (const QString &txt, const QString &css, const QRegExp &search)
{
    m_doctmp = QString("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">"
        "<html><head><meta name=\"qrichtext\" content=\"1\" />"
        "<style type=\"text/css\">%1"
        "\n\nQTextDocument {color: %3 !important;}</style></head><body>"
        "<div class=\"number\">%4</div>"
        "<div class=\"txt\">%2</div></body></html>").arg(css);

    setData(txt, search);

    updateCache();

    QTextDocument doc;
    doc.setHtml( m_cache.arg("#000000").arg("99") );
    m_size = doc.size().toSize();
}

void ClipboardItem::setData(const QString &txt, const QRegExp &search)
{
    if ( m_txt != txt || m_search != search ) {
        m_txt = txt;
        m_search = search;
        m_cache.clear(); // invalidate cache
    }
}

void ClipboardItem::updateCache() const
{
    // Highlight matched text:
    // 1. split matched and unmatched text,
    // 2. escape each text,
    // 3. replace '\n' -> <br />,
    // 4. concat matched (highlighted) and unmatched.
    QString body;
    QTextDocument doc;
    if ( !m_search.isEmpty() ) {
        doc.setPlainText(m_txt);
        QTextCursor c = doc.find(m_search);
        int last = 0;
        while ( c.hasSelection() ) {
            body += Qt::escape( m_txt.mid(last,c.selectionStart()) ) +
                "<span class=\"em\">" + Qt::escape( c.selectedText() ) + "</span>";
            last = c.selectionEnd();
            c = doc.find(m_search, c);
        }
        if ( last != m_txt.length() )
            body += Qt::escape( m_txt.mid(last) );
    }
    else
        body += Qt::escape(m_txt).trimmed().replace('\n', "<br />");

    m_cache = m_doctmp.arg( body );
}

void ClipboardItem::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QTextDocument doc;

    // background color
    QColor color;
    if( option.state & QStyle::State_Selected )
        color = option.palette.color(QPalette::HighlightedText);
    else
        color = option.palette.color(QPalette::Text);

    if ( m_cache.isEmpty() )
        updateCache();
    doc.setHtml( m_cache.arg(color.name()).arg(index.row()) );
    //doc.setDefaultFont(option.font);

    painter->save();
    QRectF rect( 0, 0, option.rect.width(), option.rect.height()+3 );
    doc.drawContents(painter, rect);
    painter->restore();
    m_size = doc.size().toSize();
}
