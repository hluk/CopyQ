/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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

class QString;
class QByteArray;
class QWidget;

QString getConfigurationFilePath(const QString &suffix);

QString settingsDirectoryPath();

QByteArray geometryOptionValue(const QString &optionName);

void restoreWindowGeometry(QWidget *w, bool openOnCurrentScreen);

void saveWindowGeometry(QWidget *w, bool openOnCurrentScreen);

#endif // CONFIG_H
