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

#include "common/arguments.h"

#include <QByteArray>
#include <QDataStream>
#include <QFile>
#include <QDir>

namespace {

QString parseCommandLineArgument(const QString &arg)
{
    QString result;
    bool escape = false;

    foreach (const QChar &c, arg) {
        if (escape) {
            escape = false;

            if (c == 'n')
                result.append('\n');
            else if (c == 't')
                result.append('\t');
            else if (c == '\\')
                result.append('\\');
            else
                result.append(c);
        } else if (c == '\\') {
            escape = true;
        } else {
            result.append(c);
        }
    }

    return result;
}

} // namespace

Arguments::Arguments()
    : m_args()
{
    reset();
}

Arguments::Arguments(const QStringList &arguments)
    : m_args()
{
    reset();

    bool readRaw = false;
    foreach (const QString &arg, arguments) {
        readRaw = readRaw || arg == "--";
        if (readRaw)
            m_args.append( arg.toUtf8() );
        else if (arg == "-e")
            m_args.append("eval");
        else
            m_args.append( parseCommandLineArgument(arg).toUtf8() );
    }
}

Arguments::~Arguments()
{
}

void Arguments::reset()
{
    m_args.resize(Rest);
    m_args[CurrentPath] = QDir::currentPath().toUtf8();
    m_args[ActionId] = qgetenv("COPYQ_ACTION_ID");
}

void Arguments::append(const QByteArray &argument)
{
    m_args.append(argument);
}

const QByteArray &Arguments::at(int index) const
{
    return m_args.at(index);
}

void Arguments::setArgument(int index, const QByteArray &argument)
{
    m_args[index] = argument;
}

void Arguments::removeAllArguments()
{
    m_args.clear();
}

QDataStream &operator <<(QDataStream &stream, const Arguments &args)
{
    qint32 len = args.length();

    stream << len;
    for( int i = 0; i<len; ++i ) {
        const QByteArray &arg = args.at(i);
        stream.writeBytes(arg.constData(), static_cast<uint>(arg.length()));
    }

    return stream;
}

QDataStream &operator>>(QDataStream &stream, Arguments &args)
{
    qint32 len;
    uint arg_len;
    char *buffer;

    args.removeAllArguments();
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
