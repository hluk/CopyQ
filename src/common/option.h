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

#ifndef OPTION_H
#define OPTION_H

#include <QVariant>

class QObject;

/**
 * Configuration option.
 */
class Option final {
public:
    Option();

    /**
     * Create option with a default value.
     * Current value is taken from object's property.
     */
    explicit Option(
            const QVariant &default_value, //!< Default value.
            const char *property_name = nullptr,
            //!< Property name of @obj with value for option.
            QObject *obj = nullptr
            );

    /** Return current value. */
    QVariant value() const;

    /** Set current value. */
    bool setValue(const QVariant &value);

    /** Reset option to default value. */
    void reset();

    /**
     * Tooltip for option.
     * This text is taken from toolTip property of a QObject which was passed
     * to constructor.
     */
    QString tooltip() const;

private:
    QVariant m_default_value;
    QVariant m_value;
    const char *m_property_name;
    QObject *m_obj;
};

#endif // OPTION_H
