// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"

#include <QApplication>
#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSettings>
#include <QStandardPaths>
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

const QString &configurationFilePath()
{
    static const QString path = getConfigurationFilePathHelper();
    return path;
}

QString configurationFilePath(const char *suffix)
{
    QString path = configurationFilePath();
    // Replace suffix.
    const int i = path.lastIndexOf(QLatin1Char('.'));
    Q_ASSERT(i != -1);
    Q_ASSERT( path.endsWith(QLatin1String(".ini")) );
    return path.left(i) + QLatin1String(suffix);
}

const QString &settingsDirectoryPath()
{
    static const QString path =
        QDir::cleanPath( configurationFilePath() + QLatin1String("/..") );
    return path;
}

QString applicationLaunchPath()
{
#ifdef COPYQ_WITH_APPIMAGE
    if (const QString appImage = qEnvironmentVariable("APPIMAGE"); !appImage.isEmpty())
        return appImage;
#endif
    return QCoreApplication::applicationFilePath();
}

QString itemDataPath()
{
    return qApp->property("CopyQ_item_data_path").toString();
}

QString tabDataFileBasePath()
{
    const QFileInfo info(itemDataPath());
    return info.path() + QLatin1Char('/')
        + QCoreApplication::applicationName() + QLatin1String("_tab_");
}

QString statePath()
{
    const QVariant prop = qApp->property("CopyQ_state_path");
    if (prop.isValid())
        return prop.toString();

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    const QString stateHome = qEnvironmentVariable("XDG_STATE_HOME");
    const QString base = (!stateHome.isEmpty() && stateHome.startsWith(QLatin1Char('/')))
        ? stateHome
        : QDir::homePath() + QLatin1String("/.local/state");
    return base + QLatin1Char('/') + QCoreApplication::organizationName();
#else
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
#endif
}

QString stateFilePath(const char *suffix)
{
    return statePath() + QLatin1Char('/')
        + QCoreApplication::applicationName() + QLatin1String(suffix);
}

bool ensureStateDirectoryExists()
{
    QDir dir(statePath());
    if (!dir.mkpath(QStringLiteral("."))) {
        qCCritical(logCategory)
            << "Failed to create the directory for state:"
            << dir.path();
        return false;
    }
    return true;
}

QString adjustedInstallPath(const QString &compiledPath)
{
#ifdef COPYQ_WITH_APPIMAGE
    if (const QString appDir = qEnvironmentVariable("APPDIR"); !appDir.isEmpty())
        return appDir + compiledPath;
#endif
    return compiledPath;
}
