/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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
    Q_ASSERT(from.group().isEmpty());
    Q_ASSERT(to->group().isEmpty());

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

bool Settings::canModifySettings()
{
    Q_ASSERT(qApp);
    return qApp->property("CopyQ_server").toBool();
}

Settings::Settings()
    : m_settings(
          QSettings::defaultFormat(),
          QSettings::UserScope,
          QCoreApplication::organizationName(),
          QCoreApplication::applicationName() + "-bak" )
    , m_changed(false)
{
    Q_ASSERT( isMainThread() );
}

Settings::~Settings()
{
    // Only main application is allowed to change settings.
    if (canModifySettings() && m_changed) {
        m_settings.sync();

        beginSave();
        QSettings to;
        while ( !m_settings.group().isEmpty() )
            m_settings.endGroup();
        copySettings(m_settings, &to);
        endSave();
    }
}

void Settings::restore()
{
    if (!canModifySettings())
        return;

    Settings appSettings;

    if ( isLastSaveUnfinished() ) {
        log("Restoring application settings", LogWarning);

        if ( appSettings.isEmpty() ) {
            log("Cannot restore application settings", LogError);
        } else {
            QSettings settings;
            copySettings(appSettings.m_settings, &settings);
        }

        endSave();
    } else {
        const QSettings settings;
        if ( needsUpdate(appSettings, settings) )
            copySettings(settings, &appSettings.m_settings);
    }
}
