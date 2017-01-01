/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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

#include "appconfig.h"

#include "platform/platformnativeinterface.h"

#include <QObject>
#include <QString>

Config::Config<QString>::Value Config::editor::defaultValue()
{
    return createPlatformNativeInterface()->defaultEditorCommand();
}

Config::Config<bool>::Value Config::autostart::defaultValue()
{
    return createPlatformNativeInterface()->isAutostartEnabled();
}

QString defaultClipboardTabName()
{
    return QObject::tr(
                "&clipboard", "Default name of the tab that automatically stores new clipboard content");
}

QVariant AppConfig::option(const QString &name) const
{
    return m_settings.value(name);
}

AppConfig::AppConfig(AppConfig::Category category)
{
    m_settings.beginGroup(
                category == OptionsCategory ? "Options" : "Theme");
}

void AppConfig::setOption(const QString &name, const QVariant &value)
{
    if ( option(name) != value )
        m_settings.setValue(name, value);
}

void AppConfig::removeOption(const QString &name)
{
    m_settings.remove(name);
}
