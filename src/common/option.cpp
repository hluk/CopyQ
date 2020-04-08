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

#include "option.h"

#include <QObject>

namespace {

QString getToolTip(const QObject &object)
{
    return object.property("toolTip").toString();
}

QString toolTip(const QObject &object)
{
    const QString toolTip = getToolTip(object);

    if ( toolTip.isEmpty() && object.parent() )
        return getToolTip( *object.parent() );

    return toolTip;
}

} // namespace

Option::Option()
    : m_default_value()
    , m_value()
    , m_property_name(nullptr)
    , m_obj(nullptr)
{}

Option::Option(const QVariant &default_value, const char *property_name,
               QObject *obj)
    : m_default_value(default_value)
    , m_value(m_default_value)
    , m_property_name(property_name)
    , m_obj(obj)
{
    if (m_obj)
        m_obj->setProperty(m_property_name, m_default_value);
}

QVariant Option::value() const
{
    return m_obj != nullptr ? m_obj->property(m_property_name) : m_value;
}

bool Option::setValue(const QVariant &value)
{
    if (m_obj != nullptr) {
        m_obj->setProperty(m_property_name, value);
        return m_obj->property(m_property_name) == value;
    }

    m_value = value;
    return true;
}

void Option::reset()
{
    setValue(m_default_value);
}

QString Option::tooltip() const
{
    return m_obj != nullptr ? toolTip(*m_obj) : QString();
}
