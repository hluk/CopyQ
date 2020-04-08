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

#include "tabs.h"

#include <QRegularExpression>
#include <QSettings>

constexpr auto settingsGroupTabs = "Tabs";
constexpr auto optionTabName = "name";
constexpr auto optionIconName = "icon";
constexpr auto optionMaxItemCount = "max_item_count";
constexpr auto optionStoreItems = "store_items";

struct Tabs::PrivateData {
    QHash<QString, TabProperties> tabs;
};

Tabs::Tabs()
    : m_data(new PrivateData())
{
    QSettings settings;
    const int size = settings.beginReadArray(settingsGroupTabs);
    QHash<QString, QVariantMap> dataMaps;
    for(int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);

        TabProperties properties;
        properties.name = settings.value(optionTabName).toString();
        properties.iconName = settings.value(optionIconName).toString();
        properties.storeItems = settings.value(optionStoreItems, true).toBool();

        bool ok;
        const int maxItemCount = settings.value(optionMaxItemCount).toInt(&ok);
        if (ok)
            properties.maxItemCount = maxItemCount;

        setTabProperties(properties);
    }
}

Tabs::~Tabs() = default;

Tabs::Tabs(const Tabs &other)
    : m_data(new PrivateData())
{
    m_data->tabs = other.m_data->tabs;
}

Tabs &Tabs::operator=(const Tabs &other)
{
    m_data->tabs = other.m_data->tabs;
    return *this;
}

void Tabs::setTabProperties(const TabProperties &tabProperties)
{
    if ( tabProperties.name.isEmpty() )
        return;

    m_data->tabs[tabProperties.name] = tabProperties;
}

void Tabs::save(QSettings *settings, const QStringList &tabs)
{
    settings->beginWriteArray(settingsGroupTabs, m_data->tabs.size());

    int row = 0;
    for (auto it = m_data->tabs.constBegin(); it != m_data->tabs.constEnd(); ++it) {
        const auto &name = it.key();
        const auto &tab = it.value();

        const bool isTab = tabs.contains(name);
        const bool isTabGroup =
            !isTab && tabs.indexOf(QRegularExpression(QString("^%1/.*").arg(QRegularExpression::escape(name)))) != -1;

        if (isTab || isTabGroup) {
            settings->setArrayIndex(row++);
            settings->setValue(optionTabName, name);
            settings->setValue(optionIconName, tab.iconName);
            if (isTab) {
                settings->setValue(optionMaxItemCount, tab.maxItemCount);
                settings->setValue(optionStoreItems, tab.storeItems);
            }
        }
    }

    settings->endArray();
}

TabProperties Tabs::tabProperties(const QString &name) const
{
    TabProperties tab = m_data->tabs.value(name);
    tab.name = name;
    return tab;
}
