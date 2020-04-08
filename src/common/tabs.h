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
