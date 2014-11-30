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

#include "settings.h"

#include "common/common.h"
#include "common/config.h"
#include "common/log.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QStringList>

namespace {

bool isMainApp()
{
    Q_ASSERT(qApp);
    return qApp->property("CopyQ_server").toBool();
}

bool needsUpdate(const Settings &newSettings, const QSettings &oldSettings)
{
    if ( Settings::isEmpty(oldSettings) )
        return false;

    const QFileInfo newFile(newSettings.fileName());
    const QFileInfo oldFile(oldSettings.fileName());

    return newFile.size() != oldFile.size()
            || newFile.lastModified() < oldFile.lastModified();
}

void copySettings(const QSettings &from, QSettings *to)
{
    to->clear();

    foreach ( const QString &key, from.allKeys() )
        to->setValue(key, from.value(key));

    to->sync();
}

QString lockFileName()
{
    return getConfigurationFilePath(".bad");
}

bool isLastSaveUnfinished()
{
    return QFile::exists(lockFileName());
}

void beginSave()
{
    QFile lockFile(lockFileName());
    lockFile.open(QIODevice::WriteOnly);
}

void endSave()
{
    QFile::remove(lockFileName());
}

} // namespace

bool Settings::isEmpty(const QSettings &settings)
{
    return settings.childGroups().isEmpty();
}

Settings::Settings()
    : QSettings(
          QSettings::defaultFormat(),
          QSettings::UserScope,
          QCoreApplication::organizationName(),
          QCoreApplication::applicationName() + "-bak" )
{
    Q_ASSERT( isMainThread() );
}

Settings::~Settings()
{
    // Only main application is allowed to change settings.
    if (isMainApp()) {
        sync();

        beginSave();
        QSettings settings;
        copySettings(*this, &settings);
        endSave();
    }
}

void Settings::restore()
{
    if (!isMainApp())
        return;

    Settings appSettings;

    if ( isLastSaveUnfinished() ) {
        log("Restoring application settings", LogWarning);

        if ( isEmpty(appSettings) ) {
            log("Cannot restore application settings", LogError);
        } else {
            QSettings settings;
            copySettings(appSettings, &settings);
        }

        endSave();
    } else {
        const QSettings settings;
        if ( needsUpdate(appSettings, settings) )
            copySettings(settings, &appSettings);
    }
}
