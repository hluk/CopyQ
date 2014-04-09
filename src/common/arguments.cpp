/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

// add command line argument - handle escape sequences
void addArgumentFromCommandLine(QVector<QByteArray> &args, const char *arg, int i)
{
    args.resize(i + 1);
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
{
    reset();
}

Arguments::Arguments(int argc, char **argv, int skipArgc)
    : m_args()
{
    reset();

    /* Special arguments:
     * "-"  read this argument from stdin
     * "--" read all following arguments without control sequences
     */
    bool readRaw = false;
    for (int i = skipArgc; i < argc; ++i) {
        const char *arg = argv[i];
        if (readRaw) {
            m_args.append( QString::fromUtf8(arg).toUtf8() );
        } else {
            if ( arg[0] == '-' ) {
                if ( arg[1] == '\0' ) {
                    QFile in;
                    in.open(stdin, QIODevice::ReadOnly);
                    m_args.append( in.readAll() );
                    continue;
                } else if ( arg[2] == '\0' ) {
                    // single-char option
                    if ( arg[1] == '-' ) {
                        readRaw = true;
                        continue;
                    } else if ( arg[1] == 'e' ) {
                        m_args.append("eval");
                        continue;
                    }
                }
            }
            addArgumentFromCommandLine(m_args, arg, m_args.size());
        }
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
        stream.writeBytes(arg.constData(), (uint)arg.length());
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
