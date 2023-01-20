// SPDX-License-Identifier: GPL-3.0-or-later

#include "configtabtabs.h"

#include "common/appconfig.h"
#include "common/tabs.h"
#include "gui/iconfactory.h"
#include "gui/itemorderlist.h"
#include "gui/tabicons.h"
#include "gui/tabpropertieswidget.h"

#include <QSettings>
#include <QVBoxLayout>

Q_DECLARE_METATYPE(TabProperties)

namespace {

class TabItem final : public ItemOrderList::Item {
public:
    TabItem(const TabProperties &tab, ItemOrderList *parentList)
        : m_tabProperties(tab)
        , m_parentList(parentList)
    {
    }

    QVariant data() const override
    {
        return QVariant::fromValue(m_tabProperties);
    }

private:
    QWidget *createWidget(QWidget *parent) override
    {
        auto widget = new TabPropertiesWidget(parent);

        widget->setTabName(m_tabProperties.name);
        widget->setIconName(m_tabProperties.iconName);
        widget->setMaxItemCount(m_tabProperties.maxItemCount);
        widget->setStoreItems(m_tabProperties.storeItems);

        QObject::connect( widget, &TabPropertiesWidget::iconNameChanged,
                          [&](const QString &icon) { m_tabProperties.iconName = icon; } );
        QObject::connect( widget, &TabPropertiesWidget::maxItemCountChanged,
                          [&](int count) { m_tabProperties.maxItemCount = count; } );
        QObject::connect( widget, &TabPropertiesWidget::storeItemsChanged,
                          [&](bool store) { m_tabProperties.storeItems = store; } );

        QObject::connect(
            widget, &TabPropertiesWidget::iconNameChanged,
            m_parentList, [&] (const QString &iconName) {
                const auto row = m_parentList->currentRow();
                const auto icon = iconName.isEmpty() ? QIcon() : iconFromFile(iconName);
                m_parentList->setItemIcon(row, icon);
            }
        );

        return widget;
    }

    TabProperties m_tabProperties;
    ItemOrderList *m_parentList;
};

} // namespace

ConfigTabTabs::ConfigTabTabs(QWidget *parent)
    : QWidget(parent)
    , m_list(new ItemOrderList(this))
{
    m_list->setItemsMovable(true);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_list);

    const Tabs tabs;
    for (const auto &name : AppConfig().option<Config::tabs>()) {
        const auto icon = getIconForTabName(name);
        ItemOrderList::ItemPtr item(new TabItem(tabs.tabProperties(name), m_list));
        m_list->appendItem(name, icon, item);
    }
}

void ConfigTabTabs::saveTabs(QSettings *settings)
{
    Tabs tabs;
    QStringList tabList;
    for (int row = 0; row < m_list->itemCount(); ++row) {
        const auto name = m_list->itemLabel(row);
        if (name.isEmpty())
            continue;

        tabList.append(name);
        tabs.setTabProperties( m_list->data(row).value<TabProperties>() );
    }

    tabs.save(settings, tabList);

    AppConfig().setOption("tabs", tabList);
}
