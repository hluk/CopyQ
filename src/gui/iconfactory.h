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

#ifndef ICONFACTORY_H
#define ICONFACTORY_H

#include <QColor>
#include <QFont>
#include <QHash>
#include <QPalette>
#include <QFlags>

class QIcon;
class QPixmap;
class QPainter;

enum AppIconFlag {
    AppIconNormal,
    AppIconRunning,
    AppIconDisabled
};
Q_DECLARE_FLAGS(AppIconFlags, AppIconFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(AppIconFlags)

class IconFactory
{
public:
    IconFactory();

    ~IconFactory();

    const QColor &iconColor() { return m_iconColor; }

    const QPixmap &getPixmap(ushort id);
    QIcon getIcon(const QString &themeName, ushort id);
    QIcon getIcon(const QString &themeName, ushort id, const QColor &color, const QColor &activeColor);
    const QIcon &getIcon(const QString &iconName);
    QIcon getIcon(const QString &iconName, const QColor &color, const QColor &activeColor);

    void setUseSystemIcons(bool enable) { m_useSystemIcons = enable; }
    bool useSystemIcons() const { return m_useSystemIcons; }

    void invalidateCache();

    QIcon iconFromFile(const QString &fileName, const QColor &color = QColor(),
                       const QColor &activeColor = QColor());

    void drawIcon(ushort id, const QRect &itemRect, QPainter *painter);

    QPixmap createPixmap(ushort id, const QColor &color, int size = -1);

    void setDefaultColors(const QColor &color, const QColor &activeColor);

    QIcon iconFromPrefix(const QString &iconSuffix, const QString &resources);

    /// Return app icon (color is calculated from session name).
    QIcon appIcon(AppIconFlags flags = AppIconNormal);

private:
    QColor m_iconColor;
    QColor m_iconColorActive;
    bool m_useSystemIcons;
    bool m_loaded;

    typedef QHash<ushort, QIcon> IconCache;
    IconCache m_iconCache;

    typedef QHash<QString, QIcon> ResourceIconCache;
    ResourceIconCache m_resourceIconCache;
};

QColor getDefaultIconColor(const QColor &color);

QColor getDefaultIconColor(const QWidget &widget, QPalette::ColorRole colorRole);

template<typename Widget>
QColor getDefaultIconColor(QPalette::ColorRole colorRole)
{
    Widget w;
    return getDefaultIconColor(w, colorRole);
}

#endif // ICONFACTORY_H
