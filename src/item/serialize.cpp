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

#include "serialize.h"

#include "common/contenttype.h"
#include "common/log.h"
#include "common/mimetypes.h"

#include <QAbstractItemModel>
#include <QByteArray>
#include <QDataStream>
#include <QIODevice>
#include <QList>
#include <QPair>
#include <QStringList>

#include <unordered_map>

namespace {

template <typename T>
bool readOrError(QDataStream *out, T *value, const char *error)
{
    *out >> *value;
    if ( out->status() == QDataStream::Ok )
        return true;

    log( QString("Corrupted data: %1").arg(error), LogError );
    return false;
}

const std::unordered_map<int, QString> &idToMime()
{
    static const std::unordered_map<int, QString> map({
        {1, QString(mimeWindowTitle)},
        {2, QString(mimeItemNotes)},

        {3, QString(COPYQ_MIME_PREFIX)},

        {4, QString(mimeText)},
        {5, QString(mimeHtml)},
        {6, QString(mimeUriList)},

        {7, QString("image/")},
        {8, QString("text/")},
        {9, QString("application/")},
        {10, QString("audio/")},
        {11, QString("video/")}
    });
    return map;
}

QString decompressMime(QDataStream *out)
{
    QString mime;
    if ( !readOrError(out, &mime, "Failed to read MIME type") )
        return QString();

    bool ok;
    const int id = mime.midRef(0, 1).toInt(&ok, 16);
    if (!ok) {
        log("Corrupted data: Failed to parse MIME type ID", LogError);
        out->setStatus(QDataStream::ReadCorruptData);
        return QString();
    }

    if (id == 0)
        return mime.mid(1);

    const auto it = idToMime().find(id);
    if ( it != std::end(idToMime()) )
        return it->second + mime.mid(1);

    log("Corrupted data: Failed to decompress MIME type", LogError);
    out->setStatus(QDataStream::ReadCorruptData);
    return QString();
}

QString compressMime(const QString &mime)
{
    for (const auto &idMime : idToMime()) {
        if ( mime.startsWith(idMime.second) ) {
            const auto prefixSize = idMime.second.size();
            return QString::number(idMime.first, 16) + mime.mid(prefixSize);
        }
    }

    return "0" + mime;
}

bool deserializeDataV2(QDataStream *out, QVariantMap *data)
{
    qint32 size;
    if ( !readOrError(out, &size, "Failed to read size (v2)") )
        return false;

    QByteArray tmpBytes;
    bool compress;
    for (qint32 i = 0; i < size; ++i) {
        const QString mime = decompressMime(out);
        if ( out->status() != QDataStream::Ok )
            return false;

        if ( !readOrError(out, &compress, "Failed to read compression flag (v2)") )
            return false;

        if ( !readOrError(out, &tmpBytes, "Failed to read item data (v2)") )
            return false;

        if (compress) {
            tmpBytes = qUncompress(tmpBytes);
            if ( tmpBytes.isEmpty() ) {
                log("Corrupted data: Failed to decompress data (v2)", LogError);
                out->setStatus(QDataStream::ReadCorruptData);
                return false;
            }
        }
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
    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        const auto &mime = it.key();
        bytes = data[mime].toByteArray();
        *stream << compressMime(mime)
                << /* compressData = */ false
                << bytes;
    }
}

bool deserializeData(QDataStream *stream, QVariantMap *data)
{
    try {
        qint32 length;
        if ( !readOrError(stream, &length, "Failed to read length") )
            return false;

        if (length == -2)
            return deserializeDataV2(stream, data);

        if (length < 0) {
            log("Corrupted data: Invalid length (v1)", LogError);
            stream->setStatus(QDataStream::ReadCorruptData);
            return false;
        }

        // Deprecated format.
        // TODO: Data should be saved again in new format.
        QString mime;
        QByteArray tmpBytes;
        for (qint32 i = 0; i < length; ++i) {
            if ( !readOrError(stream, &mime, "Failed to read MIME type (v1)") )
                return false;

            if ( !readOrError(stream, &tmpBytes, "Failed to read item data (v1)") )
                return false;

            if( !tmpBytes.isEmpty() ) {
                tmpBytes = qUncompress(tmpBytes);
                if ( tmpBytes.isEmpty() ) {
                    log("Corrupted data: Failed to decompress data (v1)", LogError);
                    stream->setStatus(QDataStream::ReadCorruptData);
                    return false;
                }
            }
            data->insert(mime, tmpBytes);
        }
    } catch (const std::exception &e) {
        log( QString("Data deserialization failed: %1").arg(e.what()), LogError );
        stream->setStatus(QDataStream::ReadCorruptData);
        return false;
    }

    return stream->status() == QDataStream::Ok;
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
    return deserializeData(&out, data);
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
    if ( !readOrError(stream, &length, "Failed to read length") )
        return false;

    if (length < 0) {
        log("Corrupted data: Invalid length", LogError);
        stream->setStatus(QDataStream::ReadCorruptData);
        return false;
    }

    // Limit the loaded number of items to model's maximum.
    length = qMin(length, maxItems) - model->rowCount();

    if ( length != 0 && !model->insertRows(0, length) )
        return false;

    for(qint32 i = 0; i < length; ++i) {
        QVariantMap data;
        if ( !deserializeData(stream, &data) )
            return false;

        if ( !model->setData(model->index(i, 0), data, contentType::data) ) {
            log("Failed to set model data", LogError);
            stream->setStatus(QDataStream::ReadCorruptData);
            return false;
        }
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
    stream.setVersion(QDataStream::Qt_4_7);
    return deserializeData(model, &stream, maxItems);
}
