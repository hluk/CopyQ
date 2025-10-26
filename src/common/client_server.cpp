// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/client_server.h"
#include "common/config.h"

#include <QDir>
#include <QLoggingCategory>
#include <QString>
#include <QtGlobal>

namespace {

Q_DECLARE_LOGGING_CATEGORY(logCategory)
Q_LOGGING_CATEGORY(logCategory, "copyq.client_server")

} // namespace

QString clipboardServerName(const QString &sessionName)
{
    const QString appName =
        sessionName.isEmpty()
        ? QStringLiteral("copyq")
        : QStringLiteral("copyq-%1").arg(sessionName);

#ifdef Q_OS_UNIX
    const QString socketPath = settingsDirectoryPath();
    if ( !QDir(socketPath).mkpath(QStringLiteral(".")) )
        qCCritical(logCategory) << "Failed to create the settings directory:" << socketPath;

    // On Unix, files for local socket are created in temporary path which can be
    // overridden by environment variable. This can lead to having multiple
    // instances that can write simultaneously to same settings and data files.
    // It's ugly but creating socket files in settings directory should fix this.
    return QStringLiteral("%1/.%2_s").arg(socketPath, appName);
#else
    return QStringLiteral("%1_%2_s").arg(appName, qgetenv("USERNAME"));
#endif
}
