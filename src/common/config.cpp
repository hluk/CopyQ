// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"

#include <QApplication>
#include <QByteArray>
#include <QDir>
#include <QLoggingCategory>
#include <QSettings>
#include <QString>

namespace {

Q_DECLARE_LOGGING_CATEGORY(logCategory)
Q_LOGGING_CATEGORY(logCategory, "copyq.config")

QString getConfigurationFilePathHelper()
{
    const QSettings settings(
                QSettings::IniFormat, QSettings::UserScope,
                QCoreApplication::organizationName(),
                QCoreApplication::applicationName() );
    return settings.fileName();
}

} // namespace

bool ensureSettingsDirectoryExists()
{
    QDir settingsDir( settingsDirectoryPath() );
    if ( !settingsDir.mkpath(QStringLiteral(".")) ) {
        qCCritical(logCategory)
            << "Failed to create the directory for settings:"
            << settingsDir.path();
        return false;
    }

    return true;
}

const QString &getConfigurationFilePath()
{
    static const QString path = getConfigurationFilePathHelper();
    return path;
}

QString getConfigurationFilePath(const char *suffix)
{
    QString path = getConfigurationFilePath();
    // Replace suffix.
    const int i = path.lastIndexOf(QLatin1Char('.'));
    Q_ASSERT(i != -1);
    Q_ASSERT( path.endsWith(QLatin1String(".ini")) );
    return path.left(i) + QLatin1String(suffix);
}

const QString &settingsDirectoryPath()
{
    static const QString path =
        QDir::cleanPath( getConfigurationFilePath() + QLatin1String("/..") );
    return path;
}
