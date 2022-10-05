// SPDX-License-Identifier: GPL-3.0-or-later

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
            !isTab && tabs.indexOf(QRegularExpression(QString::fromLatin1("^%1/.*").arg(QRegularExpression::escape(name)))) != -1;

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
