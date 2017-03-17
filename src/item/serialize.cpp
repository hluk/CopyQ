/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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
#include <QIODevice>
#include <QList>
#include <QObject>
#include <QPair>
#include <QStringList>

#include <cstring>

namespace {

template <typename Fn>
bool mimeIdApply(Fn fn)
{
    return fn(1, mimeWindowTitle)
        || fn(2, mimeItemNotes)

        || fn(3, COPYQ_MIME_PREFIX)

        || fn(4, mimeText)
        || fn(5, mimeHtml)
        || fn(6, mimeUriList)

        || fn(7, "image/")
        || fn(8, "text/")
        || fn(9, "application/")
        || fn(10, "audio/")
        || fn(11, "video/");
}

QString decompressMime(const QString &mime)
{
    bool ok;
    const int id = mime.mid(0, 1).toInt(&ok, 16);
    Q_ASSERT(ok);

    QString decompressedMimePrefix;
    const bool found = mimeIdApply(
        [id, &decompressedMimePrefix](int mimeId, const char *mimePrefix) {
            if (id == mimeId) {
                decompressedMimePrefix = mimePrefix;
                return true;
            }
            return false;
        });

    if (found)
        return decompressedMimePrefix + mime.mid(1);

    Q_ASSERT( mime.startsWith("0") );
    return mime.mid(1);
}

QString compressMime(const QString &mime)
{
    QString compressedMime;
    const bool found = mimeIdApply(
        [&mime, &compressedMime](int mimeId, const char *mimePrefix) {
            if ( mime.startsWith(mimePrefix) ) {
                const auto prefixSize = static_cast<int>( strlen(mimePrefix) );
                compressedMime = QString::number(mimeId, 16) + mime.mid(prefixSize);
                return true;
            }
            return false;
        });

    if (found)
        return compressedMime;

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
    for (const auto &mime : data.keys()) {
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

bool deserializeData(QAbstractItemModel *model, QDataStream *stream, int maxItems)
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
    length = qMin(length, maxItems) - model->rowCount();

    if ( length != 0 && !model->insertRows(0, length) )
        return false;

    for(qint32 i = 0; i < length && stream->status() == QDataStream::Ok; ++i) {
        QVariantMap data;
        deserializeData(stream, &data);
        model->setData( model->index(i, 0), data, contentType::data );
    }

    return stream->status() == QDataStream::Ok;
}

bool serializeData(const QAbstractItemModel &model, QIODevice *file)
{
    QDataStream stream(file);
    stream.setVersion(QDataStream::Qt_4_7);
    return serializeData(model, &stream);
}

bool deserializeData(QAbstractItemModel *model, QIODevice *file, int maxItems)
{
    QDataStream stream(file);
    return deserializeData(model, &stream, maxItems);
}
