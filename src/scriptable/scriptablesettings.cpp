// SPDX-License-Identifier: GPL-3.0-or-later

#include "scriptablesettings.h"

ScriptableSettings::ScriptableSettings()
    : QObject()
    , m_settings()
{
}

ScriptableSettings::ScriptableSettings(const QString &fileName)
    : QObject()
    , m_settings(fileName, QSettings::IniFormat)
{
}
