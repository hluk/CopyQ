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

#ifndef ICONFACTORY_H
#define ICONFACTORY_H

#include <QFont>
#include <QIcon>
#include <QPixmap>
#include <QHash>

// http://fortawesome.github.com/Font-Awesome/design.html
enum IconId {
    IconFirst = 0xf000,
    IconRemove = 0xf00d,
    IconOff = 0xf011,
    IconCog = 0xf013,
    IconFile = 0xf016,
    IconPlayCircle = 0xf01d,
    IconPencil = 0xf040,
    IconEdit = 0xf044,
    IconRemoveSign = 0xf057,
    IconQuestionSign = 0xf059,
    IconAsterisk = 0xf069,
    IconExclamationSign = 0xf06a,
    IconFolderOpen = 0xf07c,
    IconCogs = 0xf085,
    IconHandUp = 0xf0a6,
    IconGlobe = 0xf0ac,
    IconWrench = 0xf0ad,
    IconCopy = 0xf0c5,
    IconSave = 0xf0c7,
    IconListOl = 0xf0cb,
    IconSortDown = 0xf0dd,
    IconSortUp = 0xf0de,
    IconPaste = 0xf0ea,
    IconLast = 0xf112
};

class IconFactory
{
public:
    /** Return singleton instance. */
    static IconFactory *instance();

    static bool hasInstance() { return m_Instance != NULL; }

    IconFactory();

    const QFont &iconFont() { return m_iconFont; }

    const QPixmap &getPixmap(ushort id);
    const QIcon getIcon(const QString &themeName, ushort id);
    const QIcon &getIcon(const QString &iconName);

    void setUseSystemIcons(bool enable) { m_useSystemIcons = enable; }
    bool useSystemIcons() const { return m_useSystemIcons; }

    void invalidateCache();

    static QIcon iconFromFile(const QString &fileName);

private:
    static IconFactory* m_Instance;

    QFont m_iconFont;
    bool m_useSystemIcons;

    typedef QHash<ushort, QPixmap> PixmapCache;
    PixmapCache m_pixmapCache;

    typedef QHash<ushort, QIcon> IconCache;
    IconCache m_iconCache;

    typedef QHash<QString, QIcon> ResourceIconCache;
    ResourceIconCache m_resourceIconCache;
};

#endif // ICONFACTORY_H
