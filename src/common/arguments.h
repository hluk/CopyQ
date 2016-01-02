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

#ifndef ARGUMENTS_H
#define ARGUMENTS_H

#include <QString>
#include <QVector>

class QByteArray;

/**
 * Class for processing program arguments.
 *
 * Arguments object can be constructed using standard arguments of main()
 * (i.e. argc and argv). Arguments can be added using append().
 */
class Arguments
{
public:
    enum {
        CurrentPath,
        ActionId,
        Rest
    };

    Arguments();
    Arguments(const QStringList &arguments);

    ~Arguments();

    /** Clear arguments and set current path and action id. */
    void reset();

    /** Append argument. */
    void append(const QByteArray &argument);

    /** Get argument by @a index. */
    const QByteArray &at(int index) const;

    /** Set argument at @a index. */
    void setArgument(int index, const QByteArray &argument);

    /** Total number of arguments. */
    int length() const { return m_args.size(); }

    /** Check for emptiness. */
    bool isEmpty() const { return m_args.empty(); }

    /** Clear arguments. */
    void removeAllArguments();

private:
    QVector<QByteArray> m_args;
};

/**
 * @defgroup arguments_operators Arguments Serialization Operators
 * @{
 */

/** Serialize arguments. */
QDataStream &operator <<(QDataStream &stream, const Arguments &args);
/** Deserialize arguments. */
QDataStream &operator >>(QDataStream &stream, Arguments &args);
///@}

#endif // ARGUMENTS_H
