/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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

#include "common/mimetypes.h"
#include "common/textdata.h"

#include <QApplication>
#include <QDesktopServices>
#include <QMouseEvent>
#include <QPalette>
#include <QtPlugin>
#include <QtWebKit/QWebHistory>
#include <QtWebKitWidgets/QWebFrame>
#include <QtWebKitWidgets/QWebPage>
#include <QVariant>

namespace {

const char optionMaximumHeight[] = "max_height";

bool getHtml(const QVariantMap &data, QString *text)
{
    *text = getTextData(data, mimeHtml);
    if ( text->isEmpty() )
        return false;

    // Remove trailing null character.
    if ( text->endsWith(QChar(0)) )
        text->resize(text->size() - 1);

    return true;
}

bool canMouseInteract(const QInputEvent &event)
{
    return event.modifiers() & Qt::ShiftModifier;
}

} // namespace

ItemWeb::ItemWeb(const QString &html, int maximumHeight, bool preview, QWidget *parent)
    : QWebView(parent)
    , ItemWidget(this)
    , m_copyOnMouseUp(false)
    , m_maximumHeight(maximumHeight)
    , m_preview(preview)
{
    QWebFrame *frame = page()->mainFrame();
    frame->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);

    const QFont &defaultFont = font();
    settings()->setFontFamily(QWebSettings::StandardFont, defaultFont.family());
    // DPI resolution can be different than the one used by this widget.
    int dpi = logicalDpiX();
    int pt = defaultFont.pointSize();
    settings()->setFontSize(QWebSettings::DefaultFontSize, pt * dpi / 72);

    history()->setMaximumItemCount(0);

    QPalette pal(palette());
    pal.setBrush(QPalette::Base, Qt::transparent);
    page()->setPalette(pal);
    setAttribute(Qt::WA_OpaquePaintEvent, false);

    setContextMenuPolicy(Qt::NoContextMenu);

    // Selecting text copies it to clipboard.
    connect( this, &QWebView::selectionChanged, this, &ItemWeb::onSelectionChanged );

    // Open link with external application.
    page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
    connect( page(), &QWebPage::linkClicked, this, &ItemWeb::onLinkClicked );

    settings()->setAttribute(QWebSettings::LinksIncludedInFocusChain, false);
    settings()->setAttribute(QWebSettings::LocalContentCanAccessFileUrls, false);
    settings()->setAttribute(QWebSettings::PrivateBrowsingEnabled, true);

    // Set some remote URL as base URL so we can include remote scripts.
    setHtml(html, QUrl("http://example.com/"));

    connect( frame, &QWebFrame::contentsSizeChanged,
             this, &ItemWeb::onItemChanged );
}

void ItemWeb::highlight(const QRegularExpression &re, const QFont &, const QPalette &)
{
    // FIXME: Set hightlight color and font!
    // FIXME: Hightlight text matching regular expression!
    findText( QString(), QWebPage::HighlightAllOccurrences );

    if ( !re.pattern().isEmpty() )
        findText( re.pattern(), QWebPage::HighlightAllOccurrences );
}

void ItemWeb::onItemChanged()
{
    updateSize(m_maximumSize, m_idealWidth);
}

void ItemWeb::updateSize(QSize maximumSize, int idealWidth)
{
    if (m_resizing)
        return;

    m_resizing = true;
    QWebFrame *frame = page()->mainFrame();

    setMaximumSize(maximumSize);
    m_maximumSize = maximumSize;
    m_idealWidth = idealWidth;

    const int scrollBarWidth = frame->scrollBarGeometry(Qt::Vertical).width();
    page()->setPreferredContentsSize( QSize(idealWidth - scrollBarWidth, 10) );

    int h = frame->contentsSize().height();
    if (0 < m_maximumHeight && m_maximumHeight < h)
        h = m_maximumHeight;

    const QSize size(m_maximumSize.width(), h);
    page()->setViewportSize(size);

    // FIXME: This fixes background color but makes black scroll bar.
    setStyleSheet("background-color:transparent");

    m_resizing = false;
}

void ItemWeb::onSelectionChanged()
{
    m_copyOnMouseUp = true;
}

void ItemWeb::onLinkClicked(const QUrl &url)
{
    if ( !QDesktopServices::openUrl(url) )
        load(url);
}

void ItemWeb::mousePressEvent(QMouseEvent *e)
{
    if ( m_preview || canMouseInteract(*e) ) {
        QMouseEvent e2(QEvent::MouseButtonPress, e->pos(), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
        QCoreApplication::sendEvent( page(), &e2 );
        QWebView::mousePressEvent(e);
        e->ignore();
    } else {
        e->ignore();
        QWebView::mousePressEvent(e);
    }
}

void ItemWeb::mouseMoveEvent(QMouseEvent *e)
{
    if ( m_preview || canMouseInteract(*e) )
        QWebView::mousePressEvent(e);
    else
        e->ignore();
}

void ItemWeb::wheelEvent(QWheelEvent *e)
{
    if ( m_preview || canMouseInteract(*e) )
        QWebView::wheelEvent(e);
    else
        e->ignore();
}

void ItemWeb::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_copyOnMouseUp) {
        m_copyOnMouseUp = false;
        if ( hasSelection() )
            triggerPageAction(QWebPage::Copy);
    } else {
        QWebView::mouseReleaseEvent(e);
    }
}

void ItemWeb::mouseDoubleClickEvent(QMouseEvent *e)
{
    if ( m_preview || canMouseInteract(*e) )
        QWebView::mouseDoubleClickEvent(e);
    else
        e->ignore();
}

QSize ItemWeb::sizeHint() const
{
    return page()->viewportSize();
}

ItemWebLoader::ItemWebLoader()
{
}

ItemWebLoader::~ItemWebLoader() = default;

ItemWidget *ItemWebLoader::create(const QVariantMap &data, QWidget *parent, bool preview) const
{
    if ( data.value(mimeHidden).toBool() )
        return nullptr;

    QString html;
    if ( getHtml(data, &html) ) {
        const int maxHeight = preview ? 0 : m_settings.value(optionMaximumHeight, 0).toInt();
        return new ItemWeb(html, maxHeight, preview, parent);
    }

    return nullptr;
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
    ui.reset(new Ui::ItemWebSettings);
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);
    ui->spinBoxMaxHeight->setValue( m_settings.value(optionMaximumHeight, 0).toInt() );
    return w;
}
