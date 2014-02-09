/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

#include "iconfactory.h"

#include "gui/icons.h"
#include "gui/iconfont.h"

#include <QBitmap>
#include <QCoreApplication>
#include <QFile>
#include <QFontDatabase>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QVariant>
#include <QWidget>

namespace {

const int iconSize = 16;

/// Up to this value of background lightness, icon color will be lighter.
const int lightThreshold = 150;

QPixmap colorizedPixmap(const QPixmap &pix, const QColor &color)
{
    QPixmap pix2( pix.size() );
    pix2.fill(color);
    pix2.setMask( pix.mask() );

    return pix2;
}

void colorizePixmap(QPixmap *pix, const QColor &from, const QColor &to)
{
    QPixmap pix2( pix->size() );
    pix2.fill(to);
    pix2.setMask( pix->createMaskFromColor(from, Qt::MaskOutColor) );

    QPainter p(pix);
    p.drawPixmap(0, 0, pix2);
}

QColor sessionNameToColor(const QString &name)
{
    if (name.isEmpty())
        return QColor(Qt::white);

    int r = 0;
    int g = 0;
    int b = 0;

    foreach (const QChar &c, name) {
        const ushort x = c.unicode() % 3;
        if (x == 0)
            r += 255;
        else if (x == 1)
            g += 255;
        else
            b += 255;
    }

    int max = qMax(r, qMax(g, b));
    r = r * 255 / max;
    g = g * 255 / max;
    b = b * 255 / max;

    return QColor(r, g, b);
}

} // namespace

IconFactory::IconFactory()
    : m_iconColor(Qt::black)
    , m_iconColorActive(Qt::white)
    , m_useSystemIcons(true)
    , m_loaded(false)
    , m_iconCache()
    , m_resourceIconCache()
{
    m_loaded = QFontDatabase::addApplicationFont(":/images/fontawesome-webfont.ttf") != -1;
}

IconFactory::~IconFactory()
{
}

QIcon IconFactory::getIcon(const QString &themeName, ushort id)
{
    return getIcon(themeName, id, m_iconColor, m_iconColorActive);
}

QIcon IconFactory::getIcon(const QString &themeName, ushort id, const QColor &color,
                           const QColor &activeColor)
{
    // Icon from theme
    if ( (!m_loaded || useSystemIcons()) && !themeName.isEmpty() ) {
        QIcon icon = QIcon::fromTheme(themeName);
        if ( !icon.isNull() )
            return icon;
    }

    // Icon with different color the default one
    if ( color != m_iconColor || activeColor != m_iconColorActive ) {
        QIcon icon( createPixmap(id, color) );
        if ( activeColor.isValid() )
            icon.addPixmap( createPixmap(id, activeColor), QIcon::Selected );
        return icon;
    }

    IconCache::iterator it = m_iconCache.find(id);
    if ( it == m_iconCache.end() ) {
        QIcon icon( createPixmap(id, m_iconColor) );
        if ( m_iconColorActive.isValid() ) {
            icon.addPixmap( createPixmap(id, m_iconColorActive), QIcon::Active );
            icon.addPixmap( createPixmap(id, m_iconColorActive), QIcon::Selected );
        }
        it = m_iconCache.insert(id, icon);
    }

    return *it;
}

const QIcon &IconFactory::getIcon(const QString &iconName)
{
    ResourceIconCache::iterator it = m_resourceIconCache.find(iconName);
    if ( it == m_resourceIconCache.end() )
        it = m_resourceIconCache.insert( iconName, getIcon(iconName, m_iconColor, m_iconColorActive) );

    return *it;
}

QIcon IconFactory::getIcon(const QString &iconName, const QColor &color, const QColor &activeColor)
{
    const QString resourceName = QString(":/images/") + iconName + QString(".svg");
    QIcon icon;

    // Tint tab icons.
    if ( iconName.startsWith(QString("tab_")) ) {
        QPixmap pix(resourceName);
        icon = colorizedPixmap(pix, color.isValid() ? color : m_iconColor);
        icon.addPixmap( colorizedPixmap(pix, activeColor.isValid() ? activeColor : m_iconColorActive), QIcon::Active );
    } else {
        icon = QIcon(resourceName);
    }

    return icon;
}

void IconFactory::invalidateCache()
{
    m_iconCache.clear();
}

QIcon IconFactory::iconFromFile(const QString &fileName, const QColor &color,
                                const QColor &activeColor)
{
    if ( fileName.isEmpty() )
        return QIcon();

    ushort unicode = fileName.at(0).unicode();
    if (fileName.size() == 1 && unicode >= IconFirst && unicode <= IconLast) {
        return getIcon("", unicode,
                       color.isValid() ? color : m_iconColor,
                       activeColor.isValid() ? activeColor : m_iconColorActive);
    }

    QImage image(fileName);
    if (image.isNull())
        return QIcon();

    QPixmap pix = QPixmap::fromImage( image.scaledToHeight(iconSize) );
    return QIcon(pix);
}

void IconFactory::drawIcon(ushort id, const QRect &itemRect, QPainter *painter)
{
    QFont font = iconFont();
    int size = qMin(itemRect.height() - 5, 18);
    font.setPixelSize(size);

    painter->save();
    painter->setFont(font);
    painter->drawText( itemRect.right() - size, itemRect.top() + size, QString(QChar(id)) );
    painter->restore();
}

QPixmap IconFactory::createPixmap(ushort id, const QColor &color, int size)
{
    const int sz = (size > 0) ? size : iconSize;
    QPixmap pix(sz, sz);
    pix.fill(Qt::transparent);

    if (m_loaded) {
        QPainter painter(&pix);

        QFont font = iconFont();
        font.setPixelSize(sz - 2);
        painter.setFont(font);
        painter.setPen(color);
        painter.drawText( QRect(1, 1, sz - 1, sz - 1),
                          QString(QChar(id)) );
    }

    return pix;
}

void IconFactory::setDefaultColors(const QColor &color, const QColor &activeColor)
{
    m_iconColor = color;
    m_iconColorActive = activeColor;
    m_iconCache.clear();
}

QIcon IconFactory::iconFromPrefix(const QString &iconSuffix, const QString &resources)
{
#ifdef COPYQ_ICON_PREFIX
    const QString fileName(COPYQ_ICON_PREFIX + iconSuffix);
    if ( QFile::exists(fileName) )
        return QIcon(fileName);
#else
    Q_UNUSED(iconSuffix)
#endif
    return getIcon(resources);
}

QIcon IconFactory::appIcon(AppIconFlags flags)
{
    QIcon icon = flags.testFlag(AppIconRunning) ? iconFromPrefix("-busy.svg", "icon-running")
                                                : iconFromPrefix("-normal.svg", "icon");
    QColor color = QColor(0x7f, 0xca, 0x9b);

    const QString sessionName = qApp->property("CopyQ_session_name").toString();
    if (!sessionName.isEmpty()) {
        QPixmap pix = icon.pixmap(128, 128);
        const QColor color2 = sessionNameToColor(sessionName);
        colorizePixmap(&pix, color, color2);
        color = color2;
        icon = QIcon(pix);
    }

    if (flags.testFlag(AppIconDisabled)) {
        QPixmap pix = icon.pixmap(128, 128);
        QPainter p(&pix);
        p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
        p.fillRect(pix.rect(), QColor(100, 100, 100, 100));
        icon = QIcon(pix);
    }

    return icon;
}

QColor getDefaultIconColor(const QColor &color)
{
    QColor c = color;
    bool menuBackgrounIsLight = c.lightness() > lightThreshold;
    c.setHsl(c.hue(),
             qMax(0, qMin(255, c.saturation() + (menuBackgrounIsLight ? 30 : 10))),
             qMax(0, qMin(255, c.lightness() + (menuBackgrounIsLight ? -140 : 100))));

    return c;
}

QColor getDefaultIconColor(const QWidget &widget, QPalette::ColorRole colorRole)
{
    return getDefaultIconColor( widget.palette().color(QPalette::Active, colorRole) );
}
