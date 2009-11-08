/*
    Copyright (c) 2009, Lukas Holecek <hluk@email.cz>

    This file is part of Copyq.

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

#include "itemdelegate.h"
#include <QApplication>
#include <QLabel>
#include <QPainter>
#include <QDebug>
#include <QTextDocument>
#include <QTextCursor>

ItemDelegate::ItemDelegate(QWidget *parent) : QStyledItemDelegate(parent)
{
}

void ItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QTextDocument doc;
    QString txt = index.data().toString();
    QString css = (qobject_cast<QWidget *>(parent()))->styleSheet();
    // html template
    const QString htmltmp = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">"
        "<html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">" +
        css + "QTextDocument {color: %1 !important;}</style></head><body>%2</body></html>";
    QString html;

    // set color
    QString color;
    if( option.state & QStyle::State_Selected )
        color = option.palette.color(QPalette::HighlightedText).name();
    else
        color = option.palette.color(QPalette::Text).name();

    // Highlight matched text:
    // 1. split matched and unmatched text,
    // 2. escape each text,
    // 3. replace '\n' -> <br />,
    // 4. concat matched (highlighted) and unmatched.
    QString body("<div class=\"txt\">");
    if ( !m_search.isEmpty() ) {
        doc.setPlainText(txt);
        QTextCursor c = doc.find(m_search);
        int last = 0;
        while ( c.hasSelection() ) {
            body += Qt::escape( txt.mid(last,c.selectionStart()) ) +
                    "<span class=\"em\">" + Qt::escape( c.selectedText() ) + "</span>";
            last = c.selectionEnd();
            c = doc.find(m_search, c);
        }
        if ( last != txt.length() )
            body += Qt::escape( txt.mid(last) );
    }
    else
        body += Qt::escape(txt);
    body += "</div>";
    html = htmltmp.arg(color).arg(body.replace('\n', "<br />"));
    doc.setHtml(html);
    doc.setDefaultFont(option.font);

    painter->save();

    QStyleOptionViewItemV4 options = option;

    // resize
    //m_size = doc.size().toSize();
    //options.rect.setSize( QSize(100,100) );

    // get focus rect and selection background
    const QWidget *widget = options.widget;
    QStyle *style = widget->style();
    style->drawControl(QStyle::CE_ItemViewItem, &options, painter, widget);

    painter->translate( options.rect.left(), options.rect.top()-3 );

    QRectF rect;

    // item number
    QTextDocument numdoc;
    QString number = QString("<div class=\"number\">%1.</div>").arg( index.row() );
    html = htmltmp.arg("yellow").arg(number);
    numdoc.setHtml(html);
    numdoc.setDefaultFont(option.font);
    rect = QRect( 0,0,numdoc.size().width(),numdoc.size().height() );
    numdoc.drawContents(painter, rect);

    painter->translate( numdoc.size().width()+5, 0 );
    rect = QRect( 0, 0, options.rect.width(), options.rect.height()+3 );
    doc.drawContents(painter, rect);

    painter->restore();
}

