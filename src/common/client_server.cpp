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

#include "common/client_server.h"

#include <QCoreApplication>
#include <QString>
#include <QtGlobal>

QString serverName(const QString &name)
{
    // applicationName changes case depending on whether this is a GUI app
    // or a console app on OS X.
    return QCoreApplication::applicationName().toLower() + "_"
#ifdef Q_OS_WIN
            + qgetenv("USERNAME")
#else
            + qgetenv("USER")
#endif
            + "_" + name;
}

QString clipboardServerName()
{
    return serverName("s");
}
