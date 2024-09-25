// SPDX-License-Identifier: GPL-3.0-or-later

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
               QObject *obj, OptionValueConverterPtr &&converter)
    : m_default_value(default_value)
    , m_value(m_default_value)
    , m_property_name(property_name)
    , m_obj(obj)
    , m_converter(std::move(converter))
{
    if (m_obj)
        setValue(m_default_value);
}

Option::Option(const QVariant &default_value, const char *description)
    : m_default_value(default_value)
    , m_value(m_default_value)
    , m_description(description)
{
}

QVariant Option::value() const
{
    if (m_obj == nullptr)
        return m_value;

    const QVariant value = m_obj->property(m_property_name);
    if (m_converter)
        return m_converter->save(value);
    return value;
}

bool Option::setValue(const QVariant &rawValue)
{
    const QVariant value = m_converter ? m_converter->read(rawValue) : rawValue;

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
    if (m_obj) {
        const QString tooltip = toolTip(*m_obj);
        if (tooltip.isEmpty()) {
            const QString text = m_obj->property("text").toString();
            Q_ASSERT(!text.isEmpty());
            return text;
        }
        return tooltip;
    }

    Q_ASSERT(m_description);
    return m_description ? QString::fromUtf8(m_description) : QString();
}
