/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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

    for ( const auto &key : from.allKeys() )
        to->setValue(key, from.value(key));

    to->sync();
}

QString lockFileName(const QString &path)
{
    if ( path.isEmpty() )
        return getConfigurationFilePath(".bad");
    return path + ".bad";
}

bool isLastSaveUnfinished(const QString &path)
{
    return QFile::exists(lockFileName(path));
}

void beginSave(const QString &path)
{
    QFile lockFile(lockFileName(path));
    lockFile.open(QIODevice::WriteOnly);
}

void endSave(const QString &path)
{
    QFile::remove(lockFileName(path));
}

} // namespace

bool Settings::canModifySettings = false;

bool Settings::isEmpty(const QSettings &settings)
{
    return settings.childGroups().isEmpty();
}

Settings::Settings()
    : m_settings(
          QSettings::defaultFormat(),
          QSettings::UserScope,
          QCoreApplication::organizationName(),
          QCoreApplication::applicationName() + "-bak" )
    , m_changed(false)
{
}

Settings::Settings(const QString &path)
    : m_settings(path + ".bak", QSettings::IniFormat)
    , m_path(path)
{
}

Settings::~Settings()
{
    // Only main application is allowed to change settings.
    if (canModifySettings && m_changed) {
        m_settings.sync();

        beginSave(m_path);

        while ( !m_settings.group().isEmpty() )
            m_settings.endGroup();

        save();

        endSave(m_path);
    }
}

void Settings::restore()
{
    if ( isLastSaveUnfinished(m_path) ) {
        log("Restoring application settings", LogWarning);

        if ( isEmpty() )
            log("Cannot restore application settings", LogError);
        else
            save();

        endSave(m_path);
    } else if ( m_path.isEmpty() ) {
        restore( QSettings() );
    } else {
        restore( QSettings(m_path, QSettings::IniFormat) );
    }
}

void Settings::restore(const QSettings &settings)
{
    if ( needsUpdate(*this, settings) )
        copySettings(settings, &m_settings);
}

void Settings::save()
{
    if ( m_path.isEmpty() ) {
        QSettings settings;
        copySettings(m_settings, &settings);
    } else {
        QSettings settings(m_path, QSettings::IniFormat);
        copySettings(m_settings, &settings);
    }
}
