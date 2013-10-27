/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#include "serialize.h"

#include "common/common.h"

#include <QByteArray>
#include <QDataStream>
#include <QObject>
#include <QStringList>

QDataStream &operator<<(QDataStream &stream, const QVariantMap &data)
{
    QByteArray bytes;
    stream << data.size();
    foreach (const QString &mime, data.keys()) {
        bytes = data[mime].toByteArray();
        if ( !bytes.isEmpty() )
            bytes = qCompress(bytes);
        stream << mime << bytes;
    }

    return stream;
}

QDataStream &operator>>(QDataStream &stream, QVariantMap &data)
{
    int length;

    stream >> length;

    QString mime;
    QByteArray bytes;
    for (int i = 0; i < length && stream.status() == QDataStream::Ok; ++i) {
        stream >> mime >> bytes;
        if( !bytes.isEmpty() ) {
            bytes = qUncompress(bytes);
            if (bytes.isEmpty()) {
                stream.setStatus(QDataStream::ReadCorruptData);
                break;
            }
        }
        data.insert(mime, bytes);
    }

    return stream;
}

QByteArray serializeData(const QVariantMap &data)
{
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out << data;
    return bytes;
}

bool deserializeData(QVariantMap *data, const QByteArray &bytes)
{
    QDataStream out(bytes);
    out >> *data;
    return out.status() == QDataStream::Ok;
}
