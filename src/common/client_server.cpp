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

#include "common/client_server.h"
#include "common/config.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QString>
#include <QStringList>
#include <QtGlobal>

QString clipboardServerName()
{
    // applicationName changes case depending on whether this is a GUI app
    // or a console app on OS X.
    const QString appName = QCoreApplication::applicationName().toLower();

#ifdef Q_OS_UNIX
    const QString socketPath = settingsDirectoryPath();
    QDir(socketPath).mkpath(".");

    // On Unix, files for local socket are created in temporary path which can be
    // overridden by environment variable. This can lead to having multiple
    // instances that can write simultaneously to same settings and data files.
    // It's ugly but creating socket files in settings directory should fix this.
    return socketPath + "/." + appName + "_s";
#else
    return appName + "_" + qgetenv("USERNAME") + "_s";
#endif
}
