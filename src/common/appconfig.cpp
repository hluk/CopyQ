// SPDX-License-Identifier: GPL-3.0-or-later

#include "appconfig.h"

#include "platform/platformnativeinterface.h"

#include <QObject>
#include <QString>

Config::Config<QString>::Value Config::editor::defaultValue()
{
    return platformNativeInterface()->defaultEditorCommand();
}

Config::Config<bool>::Value Config::autostart::defaultValue()
{
    return platformNativeInterface()->isAutostartEnabled();
}

QString defaultClipboardTabName()
{
    return QObject::tr(
                "&clipboard", "Default name of the tab that automatically stores new clipboard content");
}

QVariant AppConfig::option(const QString &name) const
{
    return m_settings.value(QStringLiteral("Options/") + name);
}

void AppConfig::setOption(const QString &name, const QVariant &value)
{
    if ( option(name) != value )
        m_settings.setValue(QStringLiteral("Options/") + name, value);
}

void AppConfig::removeOption(const QString &name)
{
    m_settings.remove(QStringLiteral("Options/") + name);
}
