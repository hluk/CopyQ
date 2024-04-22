// SPDX-License-Identifier: GPL-3.0-or-later

#include "serialize.h"

#include "common/contenttype.h"
#include "common/log.h"
#include "common/mimetypes.h"

#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QByteArray>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QList>
#include <QPair>
#include <QSaveFile>
#include <QStringList>

#include <unordered_map>

class DataFile {
public:
    DataFile() {}

    explicit DataFile(const QString &path)
        : m_path(path)
    {}

    const QString &path() const { return m_path; }
    void setPath(const QString &path) { m_path = path; }

    qint64 size() const
    {
        return QFileInfo(m_path).size();
    }

    QString toString() const { return m_path; }

    QByteArray readAll() const
    {
        QFile f(m_path);
        if ( !f.open(QIODevice::ReadOnly) ) {
            log( QStringLiteral("Failed to read file \"%1\": %2")
                    .arg(m_path, f.errorString()), LogError );
            return QByteArray();
        }
        return f.readAll();
    }

private:
    QString m_path;
};
Q_DECLARE_METATYPE(DataFile)

QDataStream &operator<<(QDataStream &out, DataFile value)
{
    return out << value.path();
}

QDataStream &operator>>(QDataStream &in, DataFile &value)
{
    QString path;
    in >> path;
    value.setPath(path);
    return in;
}

namespace {

const QLatin1String mimeFilePrefix("FILE:");

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
        {1, QLatin1String(mimeWindowTitle)},
        {2, QLatin1String(mimeItemNotes)},

        {3, QLatin1String(COPYQ_MIME_PREFIX)},

        {4, QLatin1String(mimeText)},
        {5, QLatin1String(mimeHtml)},
        {6, QLatin1String(mimeUriList)},

        {7, QLatin1String("image/")},
        {8, QLatin1String("text/")},
        {9, QLatin1String("application/")},
        {10, QLatin1String("audio/")},
        {11, QLatin1String("video/")}
    });
    return map;
}

QString decompressMime(QDataStream *out)
{
    QString mime;
    if ( !readOrError(out, &mime, "Failed to read MIME type") )
        return QString();

    bool ok;
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    const int id = mime.midRef(0, 1).toInt(&ok, 16);
#else
    const int id = QStringView(mime).mid(0, 1).toInt(&ok, 16);
#endif
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
    bool compress = false;
    for (qint32 i = 0; i < size; ++i) {
        QString mime = decompressMime(out);
        if ( out->status() != QDataStream::Ok )
            return false;

        const bool hasDataFile = mime.startsWith(mimeFilePrefix);

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

        if (hasDataFile) {
            mime = mime.mid( mimeFilePrefix.size() );
            const QString path = QString::fromUtf8(tmpBytes);
            const QVariant value = QVariant::fromValue(DataFile(path));
            Q_ASSERT(value.canConvert<QByteArray>());
            Q_ASSERT(value.value<DataFile>().path() == path);
            data->insert(mime, value);
        } else {
            data->insert(mime, tmpBytes);
        }
    }

    return out->status() == QDataStream::Ok;
}

QString dataFilePath(const QByteArray &bytes, bool create = false)
{
    QDir dir(itemDataPath());
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(QByteArrayLiteral("copyq_salt"));
    hash.addData(bytes);
    const QString sha = QString::fromUtf8( hash.result().toHex() );
    const QString subpath = QStringLiteral("%1/%2/%3").arg(
            sha.mid(0, 16),
            sha.mid(16, 16),
            sha.mid(32, 16)
    );
    if (create && !dir.mkpath(subpath)) {
        log( QStringLiteral("Failed to create data directory: %1")
                .arg(dir.absoluteFilePath(subpath)), LogError );
        return QString();
    }
    return dir.absoluteFilePath(
            QStringLiteral("%1/%2.dat").arg(subpath, sha.mid(48)) );
}

} // namespace

void registerDataFileConverter()
{
    QMetaType::registerConverter(&DataFile::readAll);
    QMetaType::registerConverter(&DataFile::toString);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    qRegisterMetaTypeStreamOperators<DataFile>("DataFile");
#else
    qRegisterMetaType<DataFile>("DataFile");
#endif
}

void serializeData(QDataStream *stream, const QVariantMap &data, int itemDataThreshold)
{
    *stream << static_cast<qint32>(-2);

    const qint32 size = data.size();
    *stream << size;

    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        const QString &mime = it.key();
        const QVariant &value = it.value();

        const DataFile dataFile = value.value<DataFile>();
        const int dataLength = dataFile.path().isEmpty()
            ? value.toByteArray().size() : dataFile.size();

        if ( (itemDataThreshold >= 0 && dataLength > itemDataThreshold) || mime.startsWith(mimeFilePrefix) ) {
            QString path = dataFile.path();

            // Already saved into a separate data file?
            if ( path.isEmpty() ) {
                const QByteArray bytes = value.toByteArray();
                path = dataFilePath(bytes, true);
                if ( path.isEmpty() ) {
                    stream->setStatus(QDataStream::WriteFailed);
                    return;
                }

                if ( !QFile::exists(path) ) {
                    QSaveFile f(path);
                    f.setDirectWriteFallback(false);
                    if ( !f.open(QIODevice::WriteOnly) || !f.write(bytes) || !f.commit() ) {
                        log( QStringLiteral("Failed to create data file \"%1\": %2")
                                .arg(path, f.errorString()),
                                LogError );
                        stream->setStatus(QDataStream::WriteFailed);
                        f.cancelWriting();
                        return;
                    }
                }
            }

            if ( mime.startsWith(mimeFilePrefix) )
                *stream << compressMime(mime);
            else
                *stream << compressMime(mimeFilePrefix + mime);

            *stream << /* compressData = */ false
                    << path.toUtf8();
        } else {
            const QByteArray bytes = value.toByteArray();
            *stream << compressMime(mime)
                    << /* compressData = */ false
                    << bytes;
        }
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

bool serializeData(const QAbstractItemModel &model, QDataStream *stream, int itemDataThreshold)
{
    qint32 length = model.rowCount();
    *stream << length;

    for(qint32 i = 0; i < length && stream->status() == QDataStream::Ok; ++i) {
        const QVariantMap data = model.data(model.index(i, 0), contentType::data).toMap();
        serializeData(stream, data, itemDataThreshold);
    }

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

bool serializeData(const QAbstractItemModel &model, QIODevice *file, int itemDataThreshold)
{
    QDataStream stream(file);
    stream.setVersion(QDataStream::Qt_4_7);
    return serializeData(model, &stream, itemDataThreshold);
}

bool deserializeData(QAbstractItemModel *model, QIODevice *file, int maxItems)
{
    QDataStream stream(file);
    stream.setVersion(QDataStream::Qt_4_7);
    return deserializeData(model, &stream, maxItems);
}

bool itemDataFiles(QIODevice *file, QStringList *files)
{
    QDataStream out(file);
    out.setVersion(QDataStream::Qt_4_7);

    qint32 length;
    if ( !readOrError(&out, &length, "Failed to read length") )
        return false;

    if (length < 0) {
        log("Corrupted data: Invalid length", LogError);
        return false;
    }

    for(qint32 i = 0; i < length; ++i) {
        qint32 version;
        if ( !readOrError(&out, &version, "Failed to read version") )
            return false;

        if (version != -2)
            return true;

        qint32 size;
        if ( !readOrError(&out, &size, "Failed to read size (v2)") )
            return false;

        QByteArray tmpBytes;
        bool compress;
        for (qint32 j = 0; j < size; ++j) {
            QString mime = decompressMime(&out);
            if ( out.status() != QDataStream::Ok )
                return false;

            const bool hasDataFile = mime.startsWith(mimeFilePrefix);

            if ( !readOrError(&out, &compress, "Failed to read compression flag (v2)") )
                return false;

            if ( !readOrError(&out, &tmpBytes, "Failed to read item data (v2)") )
                return false;

            if (hasDataFile) {
                const QString path = QString::fromUtf8(tmpBytes);
                files->append(path);
            }
        }
    }

    return out.status() == QDataStream::Ok;
}

QString itemDataPath()
{
    return qApp->property("CopyQ_item_data_path").toString();
}
