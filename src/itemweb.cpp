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

#ifdef HAS_WEBKIT

#include "itemweb.h"
#include <QtWebKit/QWebPage>
#include <QtWebKit/QWebFrame>

ItemWeb::ItemWeb(const QString &html, QWidget *parent)
    : QWebView(parent)
    , ItemWidget(this)
{
    QWebFrame *frame = page()->mainFrame();
    frame->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);
    frame->setScrollBarPolicy(Qt::Vertical,   Qt::ScrollBarAlwaysOff);

    if (parent != NULL) {
        const QSize size( parent->contentsRect().width(), 512 );
        resize( size.width(), size.height() );
    }

    connect( frame, SIGNAL(loadFinished(bool)),
             this, SLOT(onItemChanged()) );
    connect( frame, SIGNAL(contentsSizeChanged(QSize)),
             this, SLOT(onItemChanged()) );
    setHtml(html);

    updateItem();
}

void ItemWeb::highlight(const QRegExp &re, const QFont &, const QPalette &)
{
    // FIXME: Set hightlight color and font!
    // FIXME: Hightlight text matching regular expression!
    findText( QString(), QWebPage::HighlightAllOccurrences );

    if ( !re.isEmpty() )
        findText( re.pattern(), QWebPage::HighlightAllOccurrences );
}

void ItemWeb::onItemChanged()
{
    QSize size = page()->mainFrame()->contentsSize();
    int w = parentWidget() != NULL ? parentWidget()->contentsRect().width() : 0;
    resize( qMax(w, size.width()), size.height() );
    emit itemChanged(this);
}

#endif // HAS_WEBKIT
