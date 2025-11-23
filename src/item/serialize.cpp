// SPDX-License-Identifier: GPL-3.0-or-later

#include "serialize.h"

#include "common/contenttype.h"
#include "common/encryption.h"
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
#include <QLoggingCategory>
#include <QPair>
#include <QSaveFile>
#include <QStringList>

#include <unordered_map>

constexpr const int MAX_ITEMS = 100'000;

constexpr auto dataStreamVersionDefault = QDataStream::Qt_4_7;
constexpr auto dataStreamVersionForV3 = QDataStream::Qt_5_15;

namespace {

Q_DECLARE_LOGGING_CATEGORY(serializeCategory)
Q_LOGGING_CATEGORY(serializeCategory, "copyq.serialize")

} // namespace

class DataFile {
public:
    DataFile() {}

    explicit DataFile(const QString &path, const Encryption::EncryptionKey &encryptionKey = {})
        : m_path(path)
#ifdef WITH_QCA_ENCRYPTION
        , m_encryptionKey(encryptionKey)
#endif
    {
#ifndef WITH_QCA_ENCRYPTION
        Q_UNUSED(encryptionKey)
#endif
    }

    const QString &path() const { return m_path; }
    void setPath(const QString &path) { m_path = path; }

#ifdef WITH_QCA_ENCRYPTION
    void setEncryptionKey(const Encryption::EncryptionKey &key)
    {
        m_encryptionKey = key;
    }

    const Encryption::EncryptionKey &encryptionKey() const { return m_encryptionKey; }
#endif

    qint64 size() const
    {
        return QFileInfo(m_path).size();
    }

    QString toString() const { return m_path; }

    QByteArray readAll() const
    {
        QFile f(m_path);
        if ( !f.open(QIODevice::ReadOnly) ) {
            qCCritical(serializeCategory) << "Failed to read file" << m_path << ":" << f.errorString();
            return QByteArray();
        }
        QByteArray data = f.readAll();

#ifdef WITH_QCA_ENCRYPTION
        // Decrypt if encryption key is present
        if (m_encryptionKey.isValid()) {
            const QByteArray decryptedData = Encryption::decrypt(data, m_encryptionKey);
            if ( decryptedData.isEmpty() ) {
                qCCritical(serializeCategory) << "Failed to decrypt data file" << m_path;
                return QByteArray();
            }
            return decryptedData;
        }
#endif

        return data;
    }

    bool operator==(const DataFile &other) const {
        return m_path == other.m_path;
    }

private:
    QString m_path;
#ifdef WITH_QCA_ENCRYPTION
    Encryption::EncryptionKey m_encryptionKey;
#endif
};
Q_DECLARE_METATYPE(DataFile)

QDataStream &operator<<(QDataStream &out, DataFile value)
{
    out << value.path();
#ifdef WITH_QCA_ENCRYPTION
    const bool hasKey = value.encryptionKey().isValid();
    out << hasKey;
    if (hasKey) {
        out << value.encryptionKey().exportDEK();
    }
#else
    out << false; // No encryption key
#endif
    return out;
}

QDataStream &operator>>(QDataStream &in, DataFile &value)
{
    QString path;
    in >> path;
    value.setPath(path);

    bool hasKey = false;
    in >> hasKey;
#ifdef WITH_QCA_ENCRYPTION
    if (hasKey) {
        QByteArray dekBytes;
        in >> dekBytes;

        // Initialize QCA if not already done
        if (!Encryption::initialize()) {
            qCCritical(serializeCategory) << "Failed to initialize encryption for DataFile deserialization";
            in.setStatus(QDataStream::ReadCorruptData);
            return in;
        }

        Encryption::EncryptionKey key;
        if (key.importDEK(dekBytes)) {
            value.setEncryptionKey(key);
        } else {
            qCCritical(serializeCategory) << "Failed to import encryption key for DataFile";
        }
    }
#else
    if (hasKey) {
        qCCritical(serializeCategory) << "Cannot deserialize encrypted DataFile: encryption support not compiled";
        in.setStatus(QDataStream::ReadCorruptData);
    }
#endif
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

    qCCritical(serializeCategory) << "Corrupted data:" << error;
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
        qCCritical(serializeCategory) << "Corrupted data: Failed to parse MIME type ID";
        out->setStatus(QDataStream::ReadCorruptData);
        return QString();
    }

    if (id == 0)
        return mime.mid(1);

    const auto it = idToMime().find(id);
    if ( it != std::end(idToMime()) )
        return it->second + mime.mid(1);

    qCCritical(serializeCategory) << "Corrupted data: Failed to decompress MIME type";
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

bool deserializeDataV2(QDataStream *out, QVariantMap *data, const Encryption::EncryptionKey *encryptionKey = nullptr)
{
#ifndef WITH_QCA_ENCRYPTION
    Q_UNUSED(encryptionKey)
#endif

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

        if (hasDataFile) {
            mime = mime.mid( mimeFilePrefix.size() );

            // When encryption key is provided, data file info includes the encryption key
            // Otherwise, it's just the path for backward compatibility
            if (encryptionKey && encryptionKey->isValid()) {
                DataFile dataFile;
                *out >> dataFile;
                if (out->status() != QDataStream::Ok) {
                    qCCritical(serializeCategory) << "Failed to deserialize encrypted DataFile";
                    return false;
                }
                const QVariant value = QVariant::fromValue(dataFile);
                Q_ASSERT(value.canConvert<QByteArray>());
                data->insert(mime, value);
            } else {
                if ( !readOrError(out, &tmpBytes, "Failed to read item data (v2)") )
                    return false;

                const QString path = QString::fromUtf8(tmpBytes);
                const QVariant value = QVariant::fromValue(DataFile(path));
                Q_ASSERT(value.canConvert<QByteArray>());
                Q_ASSERT(value.value<DataFile>().path() == path);
                data->insert(mime, value);
            }
        } else {
            if ( !readOrError(out, &tmpBytes, "Failed to read item data (v2)") )
                return false;

            if (compress) {
                tmpBytes = qUncompress(tmpBytes);
                if ( tmpBytes.isEmpty() ) {
                    qCCritical(serializeCategory) << "Corrupted data: Failed to decompress data (v2)";
                    out->setStatus(QDataStream::ReadCorruptData);
                    return false;
                }
            }

            data->insert(mime, tmpBytes);
        }
    }

    return out->status() == QDataStream::Ok;
}

bool deserializeDataV3(QDataStream *out, QVariantMap *data, const Encryption::EncryptionKey *encryptionKey = nullptr)
{
#ifdef WITH_QCA_ENCRYPTION
    qint32 encryptedSize;
    if ( !readOrError(out, &encryptedSize, "Failed to read encrypted data size (v3)") )
        return false;

    if (encryptedSize < 0 || encryptedSize > 100'000'000) {
        qCCritical(serializeCategory) << "Corrupted data: Invalid encrypted data size (v3)";
        out->setStatus(QDataStream::ReadCorruptData);
        return false;
    }

    QByteArray encryptedBytes;
    if ( !readOrError(out, &encryptedBytes, "Failed to read encrypted data (v3)") )
        return false;

    if ( encryptedBytes.size() != encryptedSize ) {
        qCCritical(serializeCategory) << "Corrupted data: Encrypted data size mismatch (v3)";
        out->setStatus(QDataStream::ReadCorruptData);
        return false;
    }

    if ( !encryptionKey || !encryptionKey->isValid() ) {
        qCCritical(serializeCategory) << "Cannot decrypt data (v3): no valid encryption key provided";
        out->setStatus(QDataStream::ReadCorruptData);
        return false;
    }

    const QByteArray decryptedBytes = Encryption::decrypt(
        encryptedBytes,
        *encryptionKey
    );

    if ( decryptedBytes.isEmpty() ) {
        qCCritical(serializeCategory) << "Failed to decrypt data (v3) - wrong password or corrupted data";
        out->setStatus(QDataStream::ReadCorruptData);
        return false;
    }

    QDataStream decryptedStream(decryptedBytes);
    decryptedStream.setVersion(dataStreamVersionForV3);
    return deserializeDataV2(&decryptedStream, data, encryptionKey);
#else
    Q_UNUSED(out)
    Q_UNUSED(data)
    Q_UNUSED(encryptionKey)
    qCCritical(serializeCategory) << "Cannot deserialize encrypted data: encryption support not compiled";
    out->setStatus(QDataStream::ReadCorruptData);
    return false;
#endif
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
        qCCritical(serializeCategory) << "Failed to create data directory:"
            << dir.absoluteFilePath(subpath);
        return QString();
    }
    return dir.absoluteFilePath(
            QStringLiteral("%1/%2.dat").arg(subpath, sha.mid(48)) );
}

void serializeDataItems(QDataStream *stream, const QVariantMap &data, int itemDataThreshold, const Encryption::EncryptionKey *encryptionKey = nullptr)
{
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
            const bool shouldEncrypt = encryptionKey && encryptionKey->isValid();

            if ( path.isEmpty() ) {
                QByteArray bytes = value.toByteArray();

#ifdef WITH_QCA_ENCRYPTION
                // Encrypt data if encryption key is provided
                if (shouldEncrypt) {
                    const QByteArray encryptedBytes = Encryption::encrypt(bytes, *encryptionKey);
                    if ( encryptedBytes.isEmpty() ) {
                        qCCritical(serializeCategory) << "Failed to encrypt data file content";
                        stream->setStatus(QDataStream::WriteFailed);
                        return;
                    }
                    bytes = encryptedBytes;
                }
#endif

                path = dataFilePath(bytes, true);
                if ( path.isEmpty() ) {
                    stream->setStatus(QDataStream::WriteFailed);
                    return;
                }

                if ( !QFile::exists(path) ) {
                    QSaveFile f(path);
                    f.setDirectWriteFallback(false);
                    if ( !f.open(QIODevice::WriteOnly) || !f.write(bytes) || !f.commit() ) {
                        qCCritical(serializeCategory) << "Failed to create data file" << path << ":"
                            << f.errorString();
                        stream->setStatus(QDataStream::WriteFailed);
                        f.cancelWriting();
                        return;
                    }
                }
            }

            // Always use FILE prefix in MIME type
            if ( mime.startsWith(mimeFilePrefix) )
                *stream << compressMime(mime);
            else
                *stream << compressMime(mimeFilePrefix + mime);

            *stream << /* compressData = */ false;

            // When encryption is enabled, serialize DataFile with encryption key
            // Otherwise, serialize just the path for backward compatibility
            if (shouldEncrypt) {
                const DataFile df(path, *encryptionKey);
                *stream << df;
            } else {
                *stream << path.toUtf8();
            }
        } else {
            const QByteArray bytes = value.toByteArray();
            *stream << compressMime(mime)
                    << /* compressData = */ false
                    << bytes;
        }
    }
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

void serializeData(QDataStream *stream, const QVariantMap &data, int itemDataThreshold, const Encryption::EncryptionKey *encryptionKey)
{
#ifdef WITH_QCA_ENCRYPTION
    // Check if encryption should be used
    // Use encryption if a valid key is provided, or if encryption is enabled and we have a key
    const bool useEncryption = encryptionKey && encryptionKey->isValid();

    if (useEncryption) {
        // Use version -3 for encrypted data
        *stream << static_cast<qint32>(-3);

        // Serialize data to a temporary buffer using V2 format
        QByteArray unencryptedBytes;
        {
            QDataStream tempStream(&unencryptedBytes, QIODevice::WriteOnly);
            tempStream.setVersion(dataStreamVersionForV3);
            serializeDataItems(&tempStream, data, itemDataThreshold, encryptionKey);
            if (tempStream.status() != QDataStream::Ok) {
                stream->setStatus(tempStream.status());
                return;
            }
        }

        const QByteArray encryptedBytes = Encryption::encrypt(
            unencryptedBytes,
            *encryptionKey
        );

        if ( encryptedBytes.isEmpty() ) {
            qCCritical(serializeCategory) << "Failed to encrypt data";
            stream->setStatus(QDataStream::WriteFailed);
            return;
        }

        *stream << static_cast<qint32>(encryptedBytes.size());
        *stream << encryptedBytes;
        return;
    }
#else
    Q_UNUSED(encryptionKey)
#endif

    // Use version -2 for unencrypted data (original format)
    *stream << static_cast<qint32>(-2);
    serializeDataItems(stream, data, itemDataThreshold);
}

bool deserializeData(QDataStream *stream, QVariantMap *data, const Encryption::EncryptionKey *encryptionKey)
{
    try {
        qint32 length;
        if ( !readOrError(stream, &length, "Failed to read length") )
            return false;

        if (length == -3)
            return deserializeDataV3(stream, data, encryptionKey);

        if (length == -2)
            return deserializeDataV2(stream, data, encryptionKey);

        if (length < 0) {
            qCCritical(serializeCategory) << "Corrupted data: Invalid length (v1)";
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
                    qCCritical(serializeCategory) << "Corrupted data: Failed to decompress data (v1)";
                    stream->setStatus(QDataStream::ReadCorruptData);
                    return false;
                }
            }
            data->insert(mime, tmpBytes);
        }
    } catch (const std::exception &e) {
        qCCritical(serializeCategory) << "Data deserialization failed:" << e.what();
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

bool deserializeData(QVariantMap *data, const QByteArray &bytes, const Encryption::EncryptionKey *encryptionKey)
{
    QDataStream out(bytes);
    return deserializeData(&out, data, encryptionKey);
}

bool serializeData(const QAbstractItemModel &model, QDataStream *stream, int itemDataThreshold, const Encryption::EncryptionKey *encryptionKey)
{
    qint32 length = model.rowCount();
    *stream << length;

    for(qint32 i = 0; i < length && stream->status() == QDataStream::Ok; ++i) {
        const QVariantMap data = model.data(model.index(i, 0), contentType::data).toMap();
        serializeData(stream, data, itemDataThreshold, encryptionKey);
    }

    return stream->status() == QDataStream::Ok;
}

bool deserializeData(QAbstractItemModel *model, QDataStream *stream, const Encryption::EncryptionKey *encryptionKey)
{
    qint32 length;
    if ( !readOrError(stream, &length, "Failed to read length") )
        return false;

    if (length < 0) {
        qCCritical(serializeCategory) << "Corrupted data: Invalid length";
        stream->setStatus(QDataStream::ReadCorruptData);
        return false;
    }

    // Limit the loaded number of items to model's maximum.
    length = qMin(length, MAX_ITEMS) - model->rowCount();

    if ( length != 0 && !model->insertRows(0, length) )
        return false;

    for(qint32 i = 0; i < length; ++i) {
        QVariantMap data;
        if ( !deserializeData(stream, &data, encryptionKey) ) {
            model->removeRows(i, length);
            return false;
        }

        if ( !model->setData(model->index(i, 0), data, contentType::data) ) {
            qCCritical(serializeCategory) << "Failed to set model data";
            stream->setStatus(QDataStream::ReadCorruptData);
            return false;
        }
    }

    return stream->status() == QDataStream::Ok;
}

bool serializeData(const QAbstractItemModel &model, QIODevice *file, int itemDataThreshold, const Encryption::EncryptionKey *encryptionKey)
{
    QDataStream stream(file);
    stream.setVersion(dataStreamVersionDefault);
    return serializeData(model, &stream, itemDataThreshold, encryptionKey);
}

bool deserializeData(QAbstractItemModel *model, QIODevice *file, const Encryption::EncryptionKey *encryptionKey)
{
    QDataStream stream(file);
    stream.setVersion(dataStreamVersionDefault);
    return deserializeData(model, &stream, encryptionKey);
}

bool itemDataFiles(QIODevice *file, QStringList *files, const Encryption::EncryptionKey *encryptionKey)
{
#ifndef WITH_QCA_ENCRYPTION
    Q_UNUSED(encryptionKey)
#endif

    QDataStream out(file);
    out.setVersion(dataStreamVersionDefault);

    qint32 length;
    if ( !readOrError(&out, &length, "Failed to read length") )
        return false;

    if (length < 0) {
        qCCritical(serializeCategory) << "Corrupted data: Invalid length";
        return false;
    }

    for(qint32 i = 0; i < length; ++i) {
        qint32 version;
        if ( !readOrError(&out, &version, "Failed to read version") )
            return false;

        if (version == -3) {
#ifdef WITH_QCA_ENCRYPTION
            // Encrypted data - decrypt and enumerate files
            qint32 encryptedSize;
            if ( !readOrError(&out, &encryptedSize, "Failed to read encrypted size") )
                return false;
            QByteArray encryptedBytes;
            if ( !readOrError(&out, &encryptedBytes, "Failed to read encrypted data") )
                return false;

            if ( !encryptionKey || !encryptionKey->isValid() ) {
                // Cannot enumerate files without valid encryption key - skip this item
                qCDebug(serializeCategory) << "Skipping encrypted item in file enumeration (no key)";
                continue;
            }

            const QByteArray decryptedBytes = Encryption::decrypt(encryptedBytes, *encryptionKey);
            if ( decryptedBytes.isEmpty() ) {
                qCWarning(serializeCategory) << "Failed to decrypt item for file enumeration - skipping";
                continue;
            }

            // Parse decrypted data to extract file paths
            QDataStream decryptedStream(decryptedBytes);
            decryptedStream.setVersion(dataStreamVersionForV3);

            qint32 size;
            if ( !readOrError(&decryptedStream, &size, "Failed to read size (v3)") )
                continue;

            for (qint32 j = 0; j < size; ++j) {
                QString mime = decompressMime(&decryptedStream);
                if ( decryptedStream.status() != QDataStream::Ok )
                    continue;

                const bool hasDataFile = mime.startsWith(mimeFilePrefix);

                bool compress;
                if ( !readOrError(&decryptedStream, &compress, "Failed to read compression flag (v3)") )
                    continue;

                if (hasDataFile) {
                    // For encrypted data files, we need to deserialize the DataFile object
                    DataFile dataFile;
                    decryptedStream >> dataFile;
                    if (decryptedStream.status() == QDataStream::Ok && !dataFile.path().isEmpty()) {
                        files->append(dataFile.path());
                    }
                } else {
                    // Skip non-file data
                    QByteArray tmpBytes;
                    if ( !readOrError(&decryptedStream, &tmpBytes, "Failed to read item data (v3)") )
                        continue;
                }
            }
            continue;
#else
            // Cannot handle encrypted data without QCA - skip
            qint32 encryptedSize;
            if ( !readOrError(&out, &encryptedSize, "Failed to read encrypted size") )
                return false;
            QByteArray encryptedBytes;
            if ( !readOrError(&out, &encryptedBytes, "Failed to read encrypted data") )
                return false;
            continue;
#endif
        }

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
