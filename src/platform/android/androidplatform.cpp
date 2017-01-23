/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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

#include "androidplatform.h"

#include "platform/dummy/dummyclipboard.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>

PlatformPtr createPlatformNativeInterface()
{
    return PlatformPtr(new AndroidPlatform);
}

QApplication *AndroidPlatform::createServerApplication(int &argc, char **argv)
{
    return new QApplication(argc, argv);
}

QApplication *AndroidPlatform::createMonitorApplication(int &argc, char **argv)
{
    return new QApplication(argc, argv);
}

QCoreApplication *AndroidPlatform::createClientApplication(int &argc, char **argv)
{
    return new QCoreApplication(argc, argv);
}

PlatformClipboardPtr AndroidPlatform::clipboard()
{
    return PlatformClipboardPtr(new DummyClipboard());
}

bool AndroidPlatform::findPluginDir(QDir *pluginsDir)
{
    // TODO: Get correct paths.
    pluginsDir->setPath( qApp->applicationDirPath() );
    return pluginsDir->exists();
}

QString AndroidPlatform::defaultEditorCommand()
{
    // TODO: Is this needed?
    return QString();
}

QString AndroidPlatform::translationPrefix()
{
    return QString();
}

bool AndroidPlatform::createSystemMutex(const QString &, QObject *)
{
    // TODO: Implement if needed.
    return true;
}
