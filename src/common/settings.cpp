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

#include "common/log.h"

#include <QCoreApplication>
#include <QSet>
#include <QSharedPointer>
#include <QStringList>

#include <exception>

namespace {

typedef QSharedPointer<QSettings> SettingsPtr;
SettingsPtr backupSettings()
{
    return SettingsPtr( new QSettings(QCoreApplication::organizationName(),
                                      QCoreApplication::applicationName() + "-bak") );
}

} // namespace

Settings::Settings() : QSettings()
{
    Settings::restoreSettings();
}

Settings::~Settings()
{
    try {
        backUp();
    } catch (std::exception &e) {
        log( QString("Failed to back up settings (%1)!").arg(e.what()), LogError );
    }
}

bool Settings::isEmpty(const QSettings &settings)
{
    return settings.childGroups().isEmpty();
}

void Settings::restoreSettings()
{
    QSettings settings;

    if ( isEmpty(settings) ) {
        SettingsPtr backupSettings = ::backupSettings();

        if ( !isEmpty(*backupSettings) ) {
            log("Restoring application settings", LogWarning);
            foreach ( const QString &key, backupSettings->allKeys() )
                settings.setValue(key, backupSettings->value(key));
        }
    }
}

void Settings::backUp()
{
    sync();
    SettingsPtr backupSettings = ::backupSettings();

    if (backupSettings->allKeys().toSet() != allKeys().toSet())
        backupSettings->clear();

    foreach ( const QString &key, allKeys() )
        backupSettings->setValue( key, value(key) );

    backupSettings->sync();
}
