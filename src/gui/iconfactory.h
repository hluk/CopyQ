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

#ifndef ICONFACTORY_H
#define ICONFACTORY_H

#include <QColor>
#include <QString>

class QIcon;
class QPixmap;
class QPainter;
class QObject;
class QString;
class QVariant;
class QWidget;

QIcon getIcon(const QString &themeName, unsigned short id);

QIcon getIcon(const QVariant &iconOrIconId);

QIcon getIconFromResources(const QString &iconName);

QIcon iconFromFile(const QString &fileName, const QString &tag = QString(), const QColor &color = QColor());

unsigned short toIconId(const QString &fileNameOrId);

QPixmap createPixmap(unsigned short id, const QColor &color, int size);

/// Return app icon (color is calculated from session name).
QIcon appIcon();

void setActivePaintDevice(QObject *device);

QColor getDefaultIconColor(const QWidget &widget, bool selected = false);

void setSessionIconColor(QColor color);

void setSessionIconTag(const QString &tag);

void setSessionIconTagColor(QColor color);

void setSessionIconEnabled(bool enabled);

QColor sessionIconColor();

QString sessionIconTag();

QColor sessionIconTagColor();

void setUseSystemIcons(bool useSystemIcons);

#endif // ICONFACTORY_H
