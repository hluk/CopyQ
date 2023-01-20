// SPDX-License-Identifier: GPL-3.0-or-later

#include "dummyplatform.h"

#include "dummyclipboard.h"

#include "app/applicationexceptionhandler.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QStringList>

PlatformNativeInterface *platformNativeInterface()
{
    static DummyPlatform platform;
    return &platform;
}

QCoreApplication *DummyPlatform::createConsoleApplication(int &argc, char **argv)
{
    return new ApplicationExceptionHandler<QCoreApplication>(argc, argv);
}

QApplication *DummyPlatform::createServerApplication(int &argc, char **argv)
{
    return new ApplicationExceptionHandler<QApplication>(argc, argv);
}

QGuiApplication *DummyPlatform::createMonitorApplication(int &argc, char **argv)
{
    return new ApplicationExceptionHandler<QGuiApplication>(argc, argv);
}

QGuiApplication *DummyPlatform::createClipboardProviderApplication(int &argc, char **argv)
{
    return new ApplicationExceptionHandler<QGuiApplication>(argc, argv);
}

QCoreApplication *DummyPlatform::createClientApplication(int &argc, char **argv)
{
    return new ApplicationExceptionHandler<QCoreApplication>(argc, argv);
}

QGuiApplication *DummyPlatform::createTestApplication(int &argc, char **argv)
{
    return new ApplicationExceptionHandler<QGuiApplication>(argc, argv);
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
