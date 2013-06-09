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

#include <QFontDatabase>
#include <QIcon>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QPixmap>

const int iconSize = 16;
const int fontSize = iconSize - 2;
const QColor iconColor = QColor(72, 72, 72);

// singleton
IconFactory* IconFactory::m_Instance = 0;

IconFactory *IconFactory::instance()
{
    static QMutex mutex;

    if ( !hasInstance() ) {
        QMutexLocker lock(&mutex);
        if ( !hasInstance() )
            m_Instance = new IconFactory();
    }

    return m_Instance;
}

IconFactory::IconFactory()
    : m_iconFont()
    , m_useSystemIcons(true)
    , m_loaded(false)
    , m_pixmapCache()
    , m_iconCache()
    , m_resourceIconCache()
{
    const int id = QFontDatabase::addApplicationFont(":/images/fontawesome-webfont.ttf");
    if (id != -1) {
        m_loaded = true;
        m_iconFont = QFont("FontAwesome");
        m_iconFont.setPixelSize(fontSize);
    }
}

IconFactory::~IconFactory()
{
}

const QPixmap &IconFactory::getPixmap(ushort id)
{
    PixmapCache::iterator it = m_pixmapCache.find(id);
    if ( it == m_pixmapCache.end() ) {
        QPixmap pix(iconSize, iconSize);
        pix.fill(Qt::transparent);

        if ( isLoaded() ) {
            QPainter painter(&pix);

            painter.setFont( iconFont() );
            painter.setPen(iconColor);

            painter.drawText( QRect(1, 1, iconSize - 1, iconSize - 1),
                              QString(QChar(id)) );
        }

        it = m_pixmapCache.insert(id, pix);
    }

    return *it;
}

const QIcon IconFactory::getIcon(const QString &themeName, ushort id)
{
    IconCache::iterator it = m_iconCache.find(id);
    if ( it == m_iconCache.end() ) {
        if ( (!isLoaded() || useSystemIcons()) && !themeName.isEmpty() ) {
            QIcon icon = QIcon::fromTheme(themeName);
            if ( !icon.isNull() )
                return icon;
        }

        QIcon icon( getPixmap(id) );
        it = m_iconCache.insert(id, icon);
    }

    return *it;
}

const QIcon &IconFactory::getIcon(const QString &iconName)
{
    ResourceIconCache::iterator it = m_resourceIconCache.find(iconName);
    if ( it == m_resourceIconCache.end() ) {
        const QString resourceName = QString(":/images/") + iconName + QString(".svg");
        QIcon icon(resourceName);
        it = m_resourceIconCache.insert(iconName, icon);
    }

    return *it;
}

void IconFactory::invalidateCache()
{
    m_pixmapCache.clear();
    m_iconCache.clear();
}

QIcon IconFactory::iconFromFile(const QString &fileName)
{
    if ( fileName.isEmpty() )
        return QIcon();

    ushort unicode = fileName.at(0).unicode();
    if (fileName.size() == 1 && unicode >= IconFirst && unicode <= IconLast)
        return instance()->getIcon("", unicode);

    QImage image(fileName);
    if (image.isNull())
        return QIcon();

    QPixmap pix = QPixmap::fromImage( image.scaledToHeight(iconSize) );
    return QIcon(pix);
}

const QIcon &getIconFromResources(const QString &iconName)
{
    return IconFactory::instance()->getIcon(iconName);
}

const QIcon getIcon(const QString &themeName, ushort iconId)
{
    return IconFactory::instance()->getIcon(themeName, iconId);
}
