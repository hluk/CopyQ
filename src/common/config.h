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

#ifndef CONFIG_H
#define CONFIG_H

class QByteArray;
class QPoint;
class QString;
class QVariant;
class QWidget;

QString getConfigurationFilePath(const QString &suffix);

QString settingsDirectoryPath();

QVariant geometryOptionValue(const QString &optionName);
void setGeometryOptionValue(const QString &optionName, const QVariant &value);

void restoreWindowGeometry(QWidget *w, bool openOnCurrentScreen);

void saveWindowGeometry(QWidget *w, bool openOnCurrentScreen);

QByteArray mainWindowState(const QString &mainWindowObjectName);

void saveMainWindowState(const QString &mainWindowObjectName, const QByteArray &state);

void moveToCurrentWorkspace(QWidget *w);

void moveWindowOnScreen(QWidget *w, QPoint pos);

void setGeometryGuardBlockedUntilHidden(QWidget *w, bool blocked = true);
bool isGeometryGuardBlockedUntilHidden(const QWidget *w);

#endif // CONFIG_H
