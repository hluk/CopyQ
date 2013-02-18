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

#include "itemweb.h"

#include "contenttype.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QModelIndex>
#include <QMouseEvent>
#include <QPalette>
#include <QTextCodec>
#include <QtPlugin>
#include <QtWebKit/QWebFrame>
#include <QtWebKit/QWebHistory>
#include <QtWebKit/QWebPage>
#include <QVariant>

namespace {

bool getHtml(const QModelIndex &index, const QStringList &formats, QString *text)
{
    int i = formats.indexOf("text/html");
    if (i == -1)
        return false;

    QByteArray bytes = index.data(contentType::firstFormat + i).toByteArray();
    QTextCodec *codec = QTextCodec::codecForHtml(bytes, QTextCodec::codecForLocale());
    *text = codec->toUnicode(bytes);

    // Remove trailing null character.
    if ( text->endsWith(QChar(0)) )
        text->resize(text->size() - 1);

    return true;
}

} // namespace

ItemWeb::ItemWeb(const QString &html, QWidget *parent)
    : QWebView(parent)
    , ItemWidget(this)
{
    QWebFrame *frame = page()->mainFrame();
    frame->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);
    frame->setScrollBarPolicy(Qt::Vertical,   Qt::ScrollBarAlwaysOff);

    const QFont &defaultFont = font();
    settings()->setFontFamily(QWebSettings::StandardFont, defaultFont.family());
    // DPI resolution can be different than the one used by this widget.
    QWidget* window = QApplication::desktop()->screen();
    int dpi = window->logicalDpiX();
    int pt = defaultFont.pointSize();
    settings()->setFontSize(QWebSettings::DefaultFontSize, pt * dpi / 72);

    history()->setMaximumItemCount(0);

    QPalette pal(palette());
    pal.setBrush(QPalette::Base, Qt::transparent);
    page()->setPalette(pal);
    setAttribute(Qt::WA_OpaquePaintEvent, false);

    connect( frame, SIGNAL(loadFinished(bool)),
             this, SLOT(onItemChanged()) );
    connect( frame, SIGNAL(contentsSizeChanged(QSize)),
             this, SLOT(onItemChanged()) );

    connect( frame, SIGNAL(loadFinished(bool)),
             this, SLOT(onItemChanged()) );
    connect( frame, SIGNAL(contentsSizeChanged(QSize)),
             this, SLOT(onItemChanged()) );

    // Selecting text copies it to clipboard.
    connect( this, SIGNAL(selectionChanged()), SLOT(copy()) );

    setHtml(html);
    updateSize();
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
    updateSize();
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

void ItemWeb::mouseDoubleClickEvent(QMouseEvent *e)
{
    if ( e->modifiers().testFlag(Qt::ShiftModifier) )
        QWebView::mouseDoubleClickEvent(e);
    else
        e->ignore();
}

void ItemWeb::contextMenuEvent(QContextMenuEvent *e)
{
    e->ignore();
}

void ItemWeb::wheelEvent(QWheelEvent *e)
{
    e->ignore();
}

ItemWidget *ItemWebLoader::create(const QModelIndex &index, QWidget *parent) const
{
    const QStringList formats = index.data(contentType::formats).toStringList();

    QString html;
    if ( getHtml(index, formats, &html) )
        return new ItemWeb(html, parent);

    return NULL;
}

QStringList ItemWebLoader::formatsToSave() const
{
    return QStringList("text/plain") << QString("text/html");
}

Q_EXPORT_PLUGIN2(itemweb, ItemWebLoader)
