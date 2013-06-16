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

#include <QColor>
#include <QFont>
#include <QHash>

class QIcon;
class QPixmap;

// http://fortawesome.github.com/Font-Awesome/design.html
enum IconId {
    IconFirst = 0xf000,
    IconRemove = 0xf00d,
    IconOff = 0xf011,
    IconCog = 0xf013,
    IconFile = 0xf016,
    IconDownloadAlt = 0xf019,
    IconInbox = 0xf01c,
    IconPlayCircle = 0xf01d,
    IconQRCode = 0xf029,
    IconTag = 0xf02b,
    IconPicture = 0xf03e,
    IconPencil = 0xf040,
    IconEdit = 0xf044,
    IconShare = 0xf045,
    IconRemoveSign = 0xf057,
    IconQuestionSign = 0xf059,
    IconInfoSign = 0xf05a,
    IconArrowLeft = 0xf060,
    IconArrowRight = 0xf061,
    IconArrowUp = 0xf062,
    IconArrowDown = 0xf063,
    IconPlus = 0xf067,
    IconMinus = 0xf068,
    IconAsterisk = 0xf069,
    IconExclamationSign = 0xf06a,
    IconEyeOpen = 0xf06e,
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
    IconKeyboard = 0xf11c,
    IconSortByAlphabet = 0xf15d,
    IconSortByAlphabetAlt = 0xf15e,
    IconTerminal = 0xf120,
    IconEditSign = 0xf14b,
    IconLast = 0xf18a
};

class IconFactory
{
public:
    /** Return singleton instance. */
    static IconFactory *instance();

    static bool hasInstance() { return m_Instance != NULL; }

    IconFactory();

    ~IconFactory();

    const QFont &iconFont() { return m_iconFont; }

    const QPixmap &getPixmap(ushort id);
    const QIcon getIcon(const QString &themeName, ushort id);
    const QIcon &getIcon(const QString &iconName);

    void setUseSystemIcons(bool enable) { m_useSystemIcons = enable; }
    bool useSystemIcons() const { return m_useSystemIcons; }

    void invalidateCache();

    static QIcon iconFromFile(const QString &fileName);

    /**
     * Return true only if the icon resources were successfuly loaded.
     */
    bool isLoaded() const { return m_loaded; }

private:
    static IconFactory* m_Instance;

    QFont m_iconFont;
    QColor m_iconColor;
    bool m_useSystemIcons;
    bool m_loaded;

    typedef QHash<ushort, QPixmap> PixmapCache;
    PixmapCache m_pixmapCache;

    typedef QHash<ushort, QIcon> IconCache;
    IconCache m_iconCache;

    typedef QHash<QString, QIcon> ResourceIconCache;
    ResourceIconCache m_resourceIconCache;
};

const QIcon &getIconFromResources(const QString &iconName);
const QIcon getIcon(const QString &themeName, ushort iconId);

#endif // ICONFACTORY_H
