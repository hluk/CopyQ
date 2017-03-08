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

#include "dummyplatform.h"

#include "dummyclipboard.h"

#include "app/applicationexceptionhandler.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QStringList>

PlatformPtr createPlatformNativeInterface()
{
    return PlatformPtr(new DummyPlatform);
}

QCoreApplication *DummyPlatform::createConsoleApplication(int &argc, char **argv)
{
    return new ApplicationExceptionHandler<QCoreApplication>(argc, argv);
}

QApplication *DummyPlatform::createServerApplication(int &argc, char **argv)
{
    return new ApplicationExceptionHandler<QApplication>(argc, argv);
}

QApplication *DummyPlatform::createMonitorApplication(int &argc, char **argv)
{
    return new ApplicationExceptionHandler<QApplication>(argc, argv);
}

QCoreApplication *DummyPlatform::createClientApplication(int &argc, char **argv)
{
    return new ApplicationExceptionHandler<QCoreApplication>(argc, argv);
}

PlatformClipboardPtr DummyPlatform::clipboard()
{
    return PlatformClipboardPtr(new DummyClipboard());
}

bool DummyPlatform::findPluginDir(QDir *pluginsDir)
{
    pluginsDir->setPath( qApp->applicationDirPath() );
    return pluginsDir->cd("plugins");
}

QString DummyPlatform::defaultEditorCommand()
{
    return "gedit %1";
}

QString DummyPlatform::translationPrefix()
{
    return QString();
}

QStringList DummyPlatform::getCommandLineArguments(int argc, char **argv)
{
    QStringList arguments;

    for (int i = 1; i < argc; ++i)
        arguments.append( QString::fromUtf8(argv[i]) );

    return arguments;
}
