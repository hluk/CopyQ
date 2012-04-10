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

#include "arguments.h"
#include <QDataStream>
#include <QFile>

Arguments::Arguments()
    : m_args()
    , m_current(0)
    , m_error(false)
    , m_default_value()
{
}

Arguments::Arguments(int &argc, char **argv)
    : m_args()
    , m_current(0)
    , m_error(false)
    , m_default_value()
{
    for (int i = 1; i < argc; ++i)
        m_args.append( QByteArray(argv[i]) );

    // dash character means 'read from stdin'
    int len = length();
    int i = -1;
    if ( (len == 1 || len == 2) && m_args.first() == QString("-") ) {
        i = 0;
    } else if ( (len == 3 || len == 4) && m_args.first() == QString("tab") &&
                m_args.at(2) == QString("-") )
    {
        i = 2;
    }

    if ( i >= 0 ) {
        m_args[i] = "write";

        if ( i == m_args.size()-1 )
            m_args.append( QByteArray("text/plain") );

        // read text from stdin
        QFile in;
        in.open(stdin, QIODevice::ReadOnly);
        m_args.append( in.readAll() );
    }
}

QVariant Arguments::toVariant()
{
    QVariant default_value = m_default_value;
    m_default_value.clear();

    if (m_error)
        return QVariant();

    if ( m_current >= length() ) {
        if ( default_value.isValid() ) {
            return default_value;
        }
        m_error = true;
        return QVariant();
    }

    return m_args.at(m_current++);
}

QByteArray Arguments::toByteArray()
{
    QVariant res = toVariant();
    return res.toByteArray();
}

QString Arguments::toString()
{
    QVariant res = toVariant();
    return res.toString();
}

int Arguments::toInt()
{
    QVariant res = toVariant();

    if (m_error)
        return 0;

    bool ok;
    int n = res.toInt(&ok);
    if (ok) {
        return n;
    } else {
        m_error = true;
        return 0;
    }
}

void Arguments::reset()
{
    m_current = 0;
    m_error = false;
}

void Arguments::append(const QByteArray &argument)
{
    m_args.append(argument);
}

const QByteArray &Arguments::at(int index) const
{
    return m_args.at(index);
}

void Arguments::back()
{
    m_error = false;
    if(m_current > 0)
        --m_current;
}

void Arguments::setDefault(const QVariant &default_value)
{
    m_default_value = default_value;
}

QDataStream &operator <<(QDataStream &stream, const Arguments &args)
{
    int len = args.length();

    stream << len;
    for( int i = 0; i<len; ++i ) {
        const QByteArray &arg = args.at(i);
        stream.writeBytes(arg.constData(), (uint)arg.length());
    }

    return stream;
}

QDataStream &operator>>(QDataStream &stream, Arguments &args)
{
    int len;
    uint arg_len;
    char *buffer;

    stream >> len;
    for( int i = 0; i<len; ++i ) {
        stream.readBytes(buffer, arg_len);
        if (buffer) {
            QByteArray arg(buffer, arg_len);
            delete[] buffer;
            args.append(arg);
        } else {
            args.append(QByteArray());
        }
    }

    return stream;
}

Arguments &operator >>(Arguments &args, int &dest)
{
    dest = args.toInt();
    return args;
}

Arguments &operator >>(Arguments &args, const int &dest)
{
    args.setDefault( QVariant(dest) );
    return args;
}

Arguments &operator >>(Arguments &args, QString &dest)
{
    dest = args.toString();
    return args;
}

Arguments &operator >>(Arguments &args, const QString &dest)
{
    args.setDefault( QVariant(dest) );
    return args;
}

Arguments &operator >>(Arguments &args, QByteArray &dest)
{
    dest = args.toByteArray();
    return args;
}

Arguments &operator >>(Arguments &args, const QByteArray &dest)
{
    args.setDefault( QVariant(dest) );
    return args;
}
