// SPDX-License-Identifier: GPL-3.0-or-later

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
