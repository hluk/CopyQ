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

#include "iconfactory.h"

#include "gui/icons.h"

#include <QBitmap>
#include <QCoreApplication>
#include <QFontDatabase>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QPixmap>

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

} // namespace

IconFactory::IconFactory()
    : m_iconFont()
    , m_iconColor()
    , m_iconColorActive()
    , m_useSystemIcons(true)
    , m_loaded(false)
    , m_iconCache()
    , m_resourceIconCache()
{
    const int id = QFontDatabase::addApplicationFont(":/images/fontawesome-webfont.ttf");
    if (id != -1) {
        m_loaded = true;
        m_iconFont = QFont("FontAwesome");

        // Try to get menu color more precisely by rendering dummy widget and getting color of pixel
        // that is presumably menu background.
        QMenu menu;

        QImage img(1, 1, QImage::Format_RGB32);
        menu.resize(16, 16);

        menu.render(&img, QPoint(-8, -8));
        m_iconColor = getDefaultIconColor( img.pixel(0, 0) );

        menu.setActiveAction( menu.addAction(QString()) );
        menu.render(&img, QPoint(-8, -8));
        m_iconColorActive = getDefaultIconColor( img.pixel(0, 0) );
    }
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
    if ( (!isLoaded() || useSystemIcons()) && !themeName.isEmpty() ) {
        QIcon icon = QIcon::fromTheme(themeName);
        if ( !icon.isNull() )
            return icon;
    }

    // Icon with different color than for QMenu
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
    if ( it == m_resourceIconCache.end() ) {
        const QString resourceName = QString(":/images/") + iconName + QString(".svg");
        QIcon icon;

        // Tint tab icons.
        if ( iconName.startsWith(QString("tab_")) ) {
            QPixmap pix(resourceName);
            icon = colorizedPixmap(pix, m_iconColor);
            icon.addPixmap( colorizedPixmap(pix, m_iconColorActive), QIcon::Active );
        } else {
            icon = QIcon(resourceName);
        }

        it = m_resourceIconCache.insert(iconName, icon);
    }

    return *it;
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

    if ( isLoaded() ) {
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
