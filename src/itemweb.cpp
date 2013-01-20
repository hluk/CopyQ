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
#include <QtWebKit/QWebHistory>
#include <QApplication>
#include <QDesktopWidget>
#include <QPalette>
#include <QMouseEvent>

ItemWeb::ItemWeb(const QString &html, QWidget *parent)
    : QWebView(parent)
    , ItemWidget(this)
{
    QWebFrame *frame = page()->mainFrame();
    frame->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);
    frame->setScrollBarPolicy(Qt::Vertical,   Qt::ScrollBarAlwaysOff);

    const QFont &defaultFont = font();
    settings()->setFontFamily(QWebSettings::StandardFont, defaultFont.family());
    settings()->setFontSize(QWebSettings::DefaultFontSize, defaultFont.pointSize());

    history()->setMaximumItemCount(0);

    QPalette pal(palette());
    pal.setBrush(QPalette::Base, Qt::transparent);
    page()->setPalette(pal);
    setAttribute(Qt::WA_OpaquePaintEvent, false);

    connect( frame, SIGNAL(loadFinished(bool)),
             this, SLOT(onItemChanged()) );
    connect( frame, SIGNAL(contentsSizeChanged(QSize)),
             this, SLOT(onItemChanged()) );

    setHtml(html);

    updateSize();
    updateItem();

    connect( frame, SIGNAL(loadFinished(bool)),
             this, SLOT(onItemChanged()) );
    connect( frame, SIGNAL(contentsSizeChanged(QSize)),
             this, SLOT(onItemChanged()) );

    // Selecting text copies it to clipboard.
    connect( this, SIGNAL(selectionChanged()), SLOT(copy()) );
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
    updateSize();
    emit itemChanged(this);
}

void ItemWeb::updateSize()
{
    const int w = maximumWidth();
    page()->setPreferredContentsSize( QSize(w, 10) );
    QSize size( w, page()->mainFrame()->contentsSize().height() );
    page()->setViewportSize(size);
    resize(size);
}

void ItemWeb::copy()
{
    triggerPageAction(QWebPage::Copy);
}

void ItemWeb::mousePressEvent(QMouseEvent *e)
{
    QWebView::mousePressEvent(e);
    e->ignore();
}

void ItemWeb::contextMenuEvent(QContextMenuEvent *e)
{
    e->ignore();
}

#endif // HAS_WEBKIT
