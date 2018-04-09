/*
    Copyright (c) 2018, Lukas Holecek <hluk@email.cz>

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

#include <unordered_map>

namespace {

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
    *out >> mime;
    if ( out->status() != QDataStream::Ok )
        return QString();

    bool ok;
    const int id = mime.mid(0, 1).toInt(&ok, 16);
    if (!ok) {
        out->setStatus(QDataStream::ReadCorruptData);
        return QString();
    }

    if (id == 0)
        return mime.mid(1);

    const auto it = idToMime().find(id);
    if ( it != std::end(idToMime()) )
        return it->second + mime.mid(1);

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
    *out >> size;

    QByteArray tmpBytes;
    bool compress;
    for (qint32 i = 0; i < size && out->status() == QDataStream::Ok; ++i) {
        const QString mime = decompressMime(out);
        if ( out->status() != QDataStream::Ok )
            return false;

        *out >> compress >> tmpBytes;
        if ( out->status() != QDataStream::Ok )
            return false;

        if (compress) {
            tmpBytes = qUncompress(tmpBytes);
            if ( tmpBytes.isEmpty() ) {
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
    stream.setVersion(QDataStream::Qt_4_7);
    return deserializeData(model, &stream, maxItems);
}
