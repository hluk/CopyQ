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

namespace {

// add command line argument - handle escape sequences
void addArgumentFromCommandLine(QVector<QByteArray> &args, const char *arg,
                                int i)
{
    args.resize( args.size() + 1 );
    QByteArray &bytes = args[i];
    bool escape = false;
    for (const char *ch = arg; *ch != '\0'; ++ch) {
        if (escape) {
            escape = false;
            switch(*ch) {
            case 'n':
                bytes.append('\n');
                break;
            case 't':
                bytes.append('\t');
                break;
            case '\\':
                bytes.append('\\');
                break;
            default:
                bytes.append(*ch);
            }
        } else if (*ch == '\\') {
            escape = true;
        } else {
            bytes.append(*ch);
        }
    }
}

} // namespace

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
    /* Special arguments:
     * "-"  read this argument from stdin
     * "--" read all following arguments without control sequences
     */
    bool readRaw = false;
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (readRaw) {
            m_args.append( QByteArray(arg) );
        } else {
            if ( arg[0] == '-' ) {
                if ( arg[1] == '\0' ) {
                    QFile in;
                    in.open(stdin, QIODevice::ReadOnly);
                    m_args.append( in.readAll() );
                    continue;
                } else if ( arg[1] == '-' ) {
                    readRaw = true;
                    continue;
                }
            }
            addArgumentFromCommandLine(m_args, arg, i - 1);
        }
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
