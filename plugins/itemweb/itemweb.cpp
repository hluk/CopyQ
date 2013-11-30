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
#include "ui_itemwebsettings.h"

#include "common/contenttype.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QModelIndex>
#include <QMouseEvent>
#include <QPalette>
#include <QtPlugin>
#include <QtWebKit/QWebHistory>
#if QT_VERSION < 0x050000
#   include <QtWebKit/QWebFrame>
#   include <QtWebKit/QWebPage>
#else
#   include <QtWebKitWidgets/QWebFrame>
#   include <QtWebKitWidgets/QWebPage>
#endif
#include <QVariant>

namespace {

const char optionMaximumHeight[] = "max_height";

bool getHtml(const QModelIndex &index, QString *text)
{
    *text = index.data(contentType::html).toString();
    if ( text->isNull() )
        return false;

    // Remove trailing null character.
    if ( text->endsWith(QChar(0)) )
        text->resize(text->size() - 1);

    return true;
}

} // namespace

ItemWeb::ItemWeb(const QString &html, int maximumHeight, QWidget *parent)
    : QWebView(parent)
    , ItemWidget(this)
    , m_copyOnMouseUp(false)
    , m_maximumHeight(maximumHeight)
{
    QWebFrame *frame = page()->mainFrame();
    frame->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);

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

    setContextMenuPolicy(Qt::NoContextMenu);

    connect( frame, SIGNAL(loadFinished(bool)),
             this, SLOT(onItemChanged()) );
    connect( frame, SIGNAL(contentsSizeChanged(QSize)),
             this, SLOT(onItemChanged()) );

    connect( frame, SIGNAL(loadFinished(bool)),
             this, SLOT(onItemChanged()) );
    connect( frame, SIGNAL(contentsSizeChanged(QSize)),
             this, SLOT(onItemChanged()) );

    // Selecting text copies it to clipboard.
    connect( this, SIGNAL(selectionChanged()), SLOT(onSelectionChanged()) );

    setProperty("CopyQ_no_style", true);

    setHtml(html);
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
    updateSize(maximumSize());
}

void ItemWeb::updateSize(const QSize &maximumSize)
{
    setMaximumSize(maximumSize);
    const int w = maximumSize.width();
    const int scrollBarWidth = page()->mainFrame()->scrollBarGeometry(Qt::Vertical).width();
    page()->setPreferredContentsSize( QSize(w - scrollBarWidth, 10) );

    int h = page()->mainFrame()->contentsSize().height();
    if (0 < m_maximumHeight && m_maximumHeight < h)
        h = m_maximumHeight;

    const QSize size(w, h);
    page()->setViewportSize(size);
    setFixedSize(size);
}

void ItemWeb::onSelectionChanged()
{
    m_copyOnMouseUp = true;
}

void ItemWeb::mousePressEvent(QMouseEvent *e)
{
    QMouseEvent e2(QEvent::MouseButtonPress, e->pos(), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
    QCoreApplication::sendEvent( page(), &e2 );
    QWebView::mousePressEvent(e);
    e->ignore();
}

void ItemWeb::wheelEvent(QWheelEvent *e)
{
    e->ignore();
}

void ItemWeb::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_copyOnMouseUp) {
        m_copyOnMouseUp = false;
#if QT_VERSION >= 0x040800
        if ( hasSelection() )
#endif
            triggerPageAction(QWebPage::Copy);
    } else {
        QWebView::mouseReleaseEvent(e);
    }
}

ItemWebLoader::ItemWebLoader()
    : ui(NULL)
{
}

ItemWidget *ItemWebLoader::create(const QModelIndex &index, QWidget *parent) const
{
    QString html;
    if ( getHtml(index, &html) )
        return new ItemWeb(html, m_settings.value(optionMaximumHeight, 0).toInt(), parent);

    return NULL;
}

QStringList ItemWebLoader::formatsToSave() const
{
    return QStringList("text/plain") << QString("text/html");
}

QVariantMap ItemWebLoader::applySettings()
{
    m_settings[optionMaximumHeight] = ui->spinBoxMaxHeight->value();
    return m_settings;
}

QWidget *ItemWebLoader::createSettingsWidget(QWidget *parent)
{
    delete ui;
    ui = new Ui::ItemWebSettings;
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);
    ui->spinBoxMaxHeight->setValue( m_settings.value(optionMaximumHeight, 0).toInt() );
    return w;
}

Q_EXPORT_PLUGIN2(itemweb, ItemWebLoader)
