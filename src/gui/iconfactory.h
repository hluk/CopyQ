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
#include <QFlags>
#include <QFont>
#include <QHash>
#include <QPalette>
#include <QPointer>

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

    QIcon getIcon(const QString &themeName, ushort id);
    QIcon getIconFromResources(const QString &iconName);

    void setUseSystemIcons(bool enable) { m_useSystemIcons = enable; }
    bool useSystemIcons() const { return m_useSystemIcons || !m_iconFontLoaded; }

    QIcon iconFromFile(const QString &fileName);

    void drawIcon(ushort id, const QRect &itemRect, QPainter *painter);

    QPixmap createPixmap(ushort id, const QColor &color, int size);

    /// Return app icon (color is calculated from session name).
    QIcon appIcon(AppIconFlags flags = AppIconNormal);

    QObject *activePaintDevice() const { return m_activePaintDevice; }
    void setActivePaintDevice(QObject *device) { m_activePaintDevice = device; }

private:
    bool m_useSystemIcons;
    bool m_iconFontLoaded;
    QPointer<QObject> m_activePaintDevice;
};

QColor getDefaultIconColor(const QWidget &widget, bool selected = false);

#endif // ICONFACTORY_H
