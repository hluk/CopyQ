/*
    Copyright (c) 2012, Lukas Holecek <hluk@email.cz>

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

Option::Option()
    : m_default_value()
    , m_value()
    , m_property_name(NULL)
    , m_obj(NULL)
{}

Option::Option(const QVariant &default_value, const char *property_name,
               QObject *obj)
    : m_default_value(default_value)
    , m_value()
    , m_property_name(property_name)
    , m_obj(obj)
{
    reset();
}

QVariant Option::value() const
{
    return m_obj != NULL ? m_obj->property(m_property_name) : m_value;
}

void Option::setValue(const QVariant &value)
{
    if (m_obj != NULL)
        m_obj->setProperty(m_property_name, value);
    else
        m_value = value;
}

void Option::reset()
{
    setValue(m_default_value);
}

QString Option::tooltip() const
{
    return m_obj != NULL ? m_obj->property("toolTip").toString() : QString();
}

