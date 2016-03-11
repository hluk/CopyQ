/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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

#ifndef APPCONFIG_H
#define APPCONFIG_H

#include "common/settings.h"

#include <QVariant>

class QString;

class AppConfig
{
public:
    enum Category {
        OptionsCategory,
        ThemeCategory
    };

    explicit AppConfig(Category category = OptionsCategory);

    QVariant option(const QString &name) const;

    template <typename T>
    T option(const QString &name, T defaultValue) const
    {
        const QVariant value = option(name);
        return value.isValid() ? value.value<T>() : defaultValue;
    }

    bool isOptionOn(const QString &name) const
    {
        return option(name).toBool();
    }

    template <typename T>
    T optionInRange(const QString &name, T min, T max) const
    {
        const T value = option(name).value<T>();
        return qBound(min, value, max);
    }

    void setOption(const QString &name, const QVariant &value);

    void removeOption(const QString &name);

private:
    Settings m_settings;
};

#endif // APPCONFIG_H
