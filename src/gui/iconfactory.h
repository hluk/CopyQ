// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ICONFACTORY_H
#define ICONFACTORY_H

class QColor;
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

QIcon iconFromFile(const QString &fileName, const QString &tag, const QColor &color);
QIcon iconFromFile(const QString &fileName, const QString &tag);
QIcon iconFromFile(const QString &fileName);

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
