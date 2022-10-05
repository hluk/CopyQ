// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TABS_H
#define TABS_H

#include <QString>
#include <QStringList>

#include <memory>

class QSettings;

struct TabProperties {
    QString name;
    QString iconName;
    int maxItemCount = 0;
    bool storeItems = true;
};

class Tabs
{
public:
    Tabs();
    ~Tabs();

    Tabs(const Tabs &other);
    Tabs &operator=(const Tabs &other);

    TabProperties tabProperties(const QString &name) const;
    void setTabProperties(const TabProperties &tabProperties);

    void save(QSettings *settings, const QStringList &tabs);

private:
    struct PrivateData;
    std::unique_ptr<PrivateData> m_data;
};

#endif // TABS_H
