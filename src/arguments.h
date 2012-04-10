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

#ifndef ARGUMENTS_H
#define ARGUMENTS_H

#include <QVector>
#include <QByteArray>
#include <QVariant>

/**
 * Class for processing program arguments.
 *
 * Arguments object can be constructed using standard arguments of main()
 * (i.e. argc and argv). Arguments can be added using append().
 *
 * User can read argument using one of convert functions (toInt(), toString(),
 * toByteArray(), toVariant()) or using @ref arguments_convert_operators
 * If conversion is not possible and default value was not set or cannot
 * be converted error flag is set. In such case back() or reset() must be
 * called before another convert operation.
 */
class Arguments
{
public:
    Arguments();
    Arguments(int &argc, char **argv);

    /** Append argument. */
    void append(const QByteArray &argument);

    /** Get argument by @a index. */
    const QByteArray &at(int index) const;

    /** Return one argument back and reset error. */
    void back();

    /** Convert current argument to integer. */
    int toInt();
    /** Convert current argument to string. */
    QString toString();
    /** Convert current argument to byte array. */
    QByteArray toByteArray();
    /** Convert current argument to variant. */
    QVariant toVariant();

    /**
     * Set default value for next conversion.
     *
     * If next conversion fails the default value returned without setting
     * the error flag.
     */
    void setDefault(const QVariant &default_value);

    /** Total number of arguments. */
    int length() const { return m_args.size(); }

    /** Return true only if there are no more arguments to process. */
    bool atEnd() const { return m_current == length(); }

    /** Return true only if atEnd() and no error occurred. */
    bool finished() const { return !error() && atEnd(); }

    /**
     * Return true only if an error occurred during conversion.
     *
     * If an error occurred user must call back() or reset() functions
     * otherwise convert methods will fail.
     */
    bool error() const { return m_error; }

    /** Set current argument to the first one and reset error. */
    void reset();

private:
    QVector<QByteArray> m_args;
    int m_current;
    bool m_error;
    QVariant m_default_value;
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

/**
 * @defgroup arguments_convert_operators Arguments Conversion Operators
 * @{
 * Get next argument.
 *
 * If no arguments left or argument cannot converted error flag is set.
 *
 * Default value can be set if the right side of expression is a constant.
 *
 * For example read @a cmd, @a row (default value is 0) and @a mime (default
 * value is "text/plain") from Arguments object @a args:
 * @code
 *   QString txt, mime;
 *   int row;
 *   args >> txt >> 0 >> row >> "text/plain" >> mime;
 * @endcode
 */
Arguments &operator >>(Arguments &args, int &dest);
Arguments &operator >>(Arguments &args, const int &dest);
Arguments &operator >>(Arguments &args, QString &dest);
Arguments &operator >>(Arguments &args, const QString &dest);
Arguments &operator >>(Arguments &args, QByteArray &dest);
Arguments &operator >>(Arguments &args, const QByteArray &dest);
///@}

#endif // ARGUMENTS_H
