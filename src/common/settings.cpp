// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings.h"

#include <QCoreApplication>

Settings::Settings()
    : QSettings(
          QSettings::defaultFormat(),
          QSettings::UserScope,
          QCoreApplication::organizationName(),
          QCoreApplication::applicationName() )
{
}

Settings::Settings(const QString &path)
    : QSettings(path, QSettings::IniFormat)
{
}
