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

#include "serialize.h"

#include "common/contenttype.h"
#include "common/log.h"
#include "common/mimetypes.h"

#include <QAbstractItemModel>
#include <QByteArray>
#include <QDataStream>
#include <QFile>
#include <QList>
#include <QObject>
#include <QPair>
#include <QStringList>

namespace {

typedef QList< QPair<QString, QString> > MimeToCompressed;

void addMime(MimeToCompressed &m, const QString &mime, int value)
{
    const QString num = QString::number(value, 16);
    Q_ASSERT(num.size() == 1);
    m.append( QPair<QString, QString>(mime, num) );
}

const MimeToCompressed &mimeToCompressedList()
{
    static MimeToCompressed m;
    if ( m.isEmpty() ) {
        int i = 0;
        addMime(m, mimeWindowTitle, ++i);
        addMime(m, mimeItemNotes, ++i);

        addMime(m, COPYQ_MIME_PREFIX, ++i);

        addMime(m, mimeText, ++i);
        addMime(m, mimeHtml, ++i);
        addMime(m, mimeUriList, ++i);

        addMime(m, "image/", ++i);
        addMime(m, "text/", ++i);
        addMime(m, "application/", ++i);
        addMime(m, "audio/", ++i);
        addMime(m, "video/", ++i);
    }
    return m;
}

QString decompressMime(const QString &mime)
{
    const QString num = mime.mid(0, 1);
    const MimeToCompressed &m = mimeToCompressedList();
    for ( MimeToCompressed::const_iterator it = m.begin(); it != m.end(); ++it ) {
        if (num == it->second)
            return it->first + mime.mid(1);
    }

    Q_ASSERT( mime.startsWith("0") );
    return mime.mid(1);
}

QString compressMime(const QString &mime)
{
    const MimeToCompressed &m = mimeToCompressedList();
    for ( MimeToCompressed::const_iterator it = m.begin(); it != m.end(); ++it ) {
        if ( mime.startsWith(it->first) )
            return it->second + mime.mid( it->first.size() );
    }
    return "0" + mime;
}

bool shouldCompress(const QByteArray &bytes, const QString &mime)
{
    return bytes.size() > 256
            && ( !mime.startsWith("image/") || mime.contains("bmp") || mime.contains("xml") || mime.contains("svg") );
}

bool deserializeDataV2(QDataStream *out, QVariantMap *data)
{
    qint32 size;
    *out >> size;

    QString mime;
    QByteArray tmpBytes;
    bool compress;
    for (qint32 i = 0; i < size && out->status() == QDataStream::Ok; ++i) {
        *out >> mime >> compress >> tmpBytes;
        if(compress) {
            tmpBytes = qUncompress(tmpBytes);
            if ( tmpBytes.isEmpty() ) {
                out->setStatus(QDataStream::ReadCorruptData);
                break;
            }
        }
        mime = decompressMime(mime);
        data->insert(mime, tmpBytes);
    }

    return out->status() == QDataStream::Ok;
}

} // namespace

void serializeData(QDataStream *stream, const QVariantMap &data)
{
    *stream << static_cast<qint32>(-2);

    const qint32 size = data.size();
    *stream << size;

    QByteArray bytes;
    foreach (const QString &mime, data.keys()) {
        bytes = data[mime].toByteArray();
        bool compress = shouldCompress(bytes, mime);
        *stream << compressMime(mime) << compress << ( compress ? qCompress(bytes) : bytes );
    }
}

void deserializeData(QDataStream *stream, QVariantMap *data)
{
    try {
        qint32 length;

        *stream >> length;
        if ( stream->status() != QDataStream::Ok )
            return;

        if (length == -2) {
            deserializeDataV2(stream, data);
            return;
        }

        if (length < 0) {
            stream->setStatus(QDataStream::ReadCorruptData);
            return;
        }

        // Deprecated format.
        // TODO: Data should be saved again in new format.
        QString mime;
        QByteArray tmpBytes;
        for (qint32 i = 0; i < length && stream->status() == QDataStream::Ok; ++i) {
            *stream >> mime >> tmpBytes;
            if( !tmpBytes.isEmpty() ) {
                tmpBytes = qUncompress(tmpBytes);
                if ( tmpBytes.isEmpty() ) {
                    stream->setStatus(QDataStream::ReadCorruptData);
                    break;
                }
            }
            data->insert(mime, tmpBytes);
        }
    } catch (const std::exception &e) {
        log( QObject::tr("Data deserialization failed: %1").arg(e.what()), LogError );
        stream->setStatus(QDataStream::ReadCorruptData);
    }
}

QByteArray serializeData(const QVariantMap &data)
{
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    serializeData(&out, data);
    return bytes;
}

bool deserializeData(QVariantMap *data, const QByteArray &bytes)
{
    QDataStream out(bytes);
    deserializeData(&out, data);
    return out.status() == QDataStream::Ok;
}

bool serializeData(const QAbstractItemModel &model, QDataStream *stream)
{
    qint32 length = model.rowCount();
    *stream << length;

    for(qint32 i = 0; i < length && stream->status() == QDataStream::Ok; ++i)
        serializeData( stream, model.data(model.index(i, 0), contentType::data).toMap() );

    return stream->status() == QDataStream::Ok;
}

bool deserializeData(QAbstractItemModel *model, QDataStream *stream)
{
    qint32 length;
    *stream >> length;

    if ( stream->status() != QDataStream::Ok )
        return false;

    if (length < 0) {
        stream->setStatus(QDataStream::ReadCorruptData);
        return false;
    }

    // Limit the loaded number of items to model's maximum.
    const QVariant maxItems = model->property("maxItems");
    Q_ASSERT( maxItems.isValid() );
    length = qMin( length, maxItems.toInt() ) - model->rowCount();

    if ( length != 0 && !model->insertRows(0, length) )
        return false;

    for(qint32 i = 0; i < length && stream->status() == QDataStream::Ok; ++i) {
        QVariantMap data;
        deserializeData(stream, &data);
        model->setData( model->index(i, 0), data, contentType::data );
    }

    return stream->status() == QDataStream::Ok;
}

bool serializeData(const QAbstractItemModel &model, QFile *file)
{
    QDataStream stream(file);
    stream.setVersion(QDataStream::Qt_4_7);
    return serializeData(model, &stream);
}

bool deserializeData(QAbstractItemModel *model, QFile *file)
{
    QDataStream stream(file);
    return deserializeData(model, &stream);
}
