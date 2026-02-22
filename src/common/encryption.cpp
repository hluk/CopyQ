// SPDX-License-Identifier: GPL-3.0-or-later

#include "encryption.h"
#include "common/config.h"

#ifdef WITH_QCA_ENCRYPTION

#include <QCoreApplication>
#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QLoggingCategory>
#include <QMessageAuthenticationCode>
#include <QPluginLoader>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QtCrypto>

#include <optional>
#include <utility>

namespace {

Q_DECLARE_LOGGING_CATEGORY(logCategory)
Q_LOGGING_CATEGORY(logCategory, "copyq.encryption")

// Cipher specification for QCA::isSupported() check (full name with mode and padding)
const QLatin1String encryptionCipher("aes256-cbc-pkcs7");
// Algorithm name for QCA::Cipher constructor (without mode/padding)
const QLatin1String encryptionAlgorithm("aes256");
const QByteArray encryptedDataMagic("CPQ1");
constexpr int aes256KeySize = 32;
constexpr int aesBlockSize = 16;
constexpr int hmacTagSize = 32;
constexpr int perMessageSaltSize = 16;
constexpr int hkdfSha256HashLength = 32;
constexpr int hkdfSha256MaxLength = 255 * hkdfSha256HashLength;
constexpr int pbkdf2IterationCount = 100'000;

QString dekFilePath() { return getConfigurationFilePath("wrapped_dek.dat"); }
QString saltFilePath() { return getConfigurationFilePath("kek_salt.dat"); }

void logFeatures()
{
    QStringList providerNames;
    for (const auto *provider : QCA::providers()) {
        providerNames << provider->name();
    }
    qCCritical(logCategory) << "Available providers:" << QCA::providers();
    qCCritical(logCategory) << "Supported features:" << QCA::supportedFeatures().join(QLatin1String(", "));
}

bool initializeQCA()
{
    qCDebug(logCategory) << "Initializing QCA";

#ifdef Q_OS_MACOS
    // Disable automatic QCA plugin loading to prevent loading Homebrew plugins
    // which would cause crashes due to ABI mismatches
    qputenv("QCA_NO_PLUGINS", "1");
    qCDebug(logCategory) << "Disabled automatic QCA plugin loading (QCA_NO_PLUGINS=1)";
#endif

    // QCA Initializer - must be kept alive for the entire application
    static auto qcaInit = new QCA::Initializer();
    Q_UNUSED(qcaInit);

#ifdef Q_OS_MACOS
    // Manually load the bundled ossl plugin since we disabled automatic loading
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString osslPluginPath = appDir + "/../PlugIns/crypto/libqca-ossl.dylib";

    QPluginLoader pluginLoader(osslPluginPath);
    if (!pluginLoader.load()) {
        qCCritical(logCategory) << "Failed to load bundled ossl plugin:" << pluginLoader.errorString();
        return false;
    }

    QObject *plugin = pluginLoader.instance();
    if (!plugin) {
        qCCritical(logCategory) << "Failed to get plugin instance";
        return false;
    }

    QCAPlugin *qcaPlugin = qobject_cast<QCAPlugin*>(plugin);
    if (!qcaPlugin) {
        qCCritical(logCategory) << "Plugin does not implement QCAPlugin interface";
        return false;
    }

    QCA::Provider *provider = qcaPlugin->createProvider();
    if (!provider) {
        qCCritical(logCategory) << "Failed to create provider from plugin";
        return false;
    }

    if (!QCA::insertProvider(provider)) {
        qCCritical(logCategory) << "Failed to insert provider into QCA";
        delete provider;
        return false;
    }

    qCDebug(logCategory) << "Successfully loaded bundled ossl plugin:" << provider->name();
#endif

    if ( !QCA::isSupported(encryptionCipher.data()) ) {
        qCCritical(logCategory) << "Cipher" << encryptionCipher << "not supported by QCA";
        logFeatures();
        return false;
    }

    // Check if PBKDF2 with SHA1 is supported
    // Note: We use SHA1 for PBKDF2 as it's widely supported and still secure for key derivation
    // (SHA1 vulnerabilities don't apply to HMAC-based constructions like PBKDF2)
    if ( !QCA::isSupported("pbkdf2(sha1)") ) {
        qCCritical(logCategory) << "PBKDF2 with SHA1 not supported by QCA";
        logFeatures();
        return false;
    }

    qCDebug(logCategory) << "QCA encryption initialized with" << encryptionCipher;
    return true;
}

bool backupFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.exists(fileName))
        return true;

    QFile backup(QStringLiteral("%1.backup").arg(fileName));
    if (backup.exists() && !backup.remove()) {
        qCCritical(logCategory) << "Failed to remove backup" << backup.fileName()
            << "- error:" << backup.errorString();
        return false;
    }

    if (!file.copy(backup.fileName())) {
        qCCritical(logCategory) << "Failed to backup" << fileName
            << "to" << backup.fileName()
            << "- error:" << file.errorString();
        return false;
    }

    qCDebug(logCategory) << "Back up created" << backup.fileName();
    return true;
}

bool restoreBackupFile(const QString &fileName)
{
    QFile backup(QStringLiteral("%1.backup").arg(fileName));
    if (!backup.exists())
        return true;

    if (!backup.isReadable()) {
        qCCritical(logCategory) << "Backup file is not readable" << fileName;
        return false;
    }

    QFile file(fileName);
    if (file.exists() && !file.remove()) {
        qCCritical(logCategory) << "Failed to restore backup, cannot remove original" << fileName
            << "- error:" << file.errorString();
        return false;
    }

    if (!backup.copy(fileName)) {
        qCCritical(logCategory) << "Failed to restore backup" << backup.fileName()
            << "to" << fileName
            << "- error:" << backup.errorString();
        return false;
    }

    qCDebug(logCategory) << "Restored from backup" << fileName;

    if (!backup.remove()) {
        qCCritical(logCategory) << "Failed to remove restored backup" << backup.fileName()
            << "- error:" << backup.errorString();
    }

    return true;
}

bool removeBackup(const QString &fileName)
{
    QFile backup(QStringLiteral("%1.backup").arg(fileName));
    if (!backup.exists())
        return true;

    if (backup.remove()) {
        qCDebug(logCategory) << "Removed backup" << backup.fileName();
        return true;
    }

    qCCritical(logCategory) << "Failed to remove backup" << backup.fileName()
        << "- error:" << backup.errorString();
    return false;
}

bool backupEncryptionFiles()
{
    return backupFile(::dekFilePath())
        && backupFile(::saltFilePath());
}

bool restoreEncryptionFiles()
{
    return restoreBackupFile(::dekFilePath())
        && restoreBackupFile(::saltFilePath());
}

void removeEncryptionBackups()
{
    removeBackup(::dekFilePath());
    removeBackup(::saltFilePath());
}

bool saveFile(const QString &fileName, const Encryption::SecureArray &data)
{
    QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        qCCritical(logCategory)
            << "Failed to open for writing:" << fileName
            << "- error:" << file.errorString();
        return false;
    }

    const Encryption::Cleared<QByteArray> bytes(data.toByteArray());
    if (file.write(bytes.value()) != bytes.size()) {
        qCCritical(logCategory)
            << "Failed to write" << fileName
            << "- error:" << file.errorString();
        file.cancelWriting();
        return false;
    }

    if (!file.commit()) {
        qCCritical(logCategory)
            << "Failed to commit" << fileName
            << "- error:" << file.errorString();
        return false;
    }

#ifndef Q_OS_WIN
    // Set restrictive permissions on Unix systems
    QFile storedFile(fileName);
    if (!storedFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        qCWarning(logCategory)
            << "Failed to restrict permissions" << fileName
            << "- error:" << storedFile.errorString();
    }
#endif

    qCDebug(logCategory) << "Saved" << fileName;
    return true;
}

bool canUseEncryptionKey(const char *operation, const QCA::SymmetricKey &key)
{
    if ( !QCA::isSupported(encryptionCipher.data()) ) {
        qCCritical(logCategory) << operation << "cipher not supported";
        return false;
    }

    if ( key.isEmpty() ) {
        qCCritical(logCategory) << operation << "DEK is empty";
        return false;
    }

    return true;
}

Encryption::SecureArray loadNonEmptySecureFile(const QString &path, const char *label)
{
    if (!QFileInfo::exists(path)) {
        return {};
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qCCritical(logCategory)
            << "Failed to open" << label << "file" << path
            << "- error:" << file.errorString();
        return {};
    }

    const Encryption::Cleared<QByteArray> data(file.readAll());
    file.close();

    if (data.isEmpty()) {
        qCWarning(logCategory) << label << "file is empty";
        return {};
    }

    qCDebug(logCategory) << label << "loaded from settings directory:" << path;
    return Encryption::SecureArray(data.value());
}

QCA::SymmetricKey deriveFromPasswordAndSalt(
    const Encryption::SecureArray &password, const Encryption::Salt &salt)
{
    if (password.isEmpty()) {
        qCCritical(logCategory) << "EncryptionKey: Cannot derive key from empty password";
        return {};
    }

    // Derive key from password using PBKDF2 with a fixed salt
    // Using SHA1 as it's widely supported and still secure for PBKDF2
    QCA::PBKDF2 pbkdf2(QStringLiteral("sha1"));
    QCA::SymmetricKey symKey =
        pbkdf2.makeKey(password, salt, aes256KeySize, pbkdf2IterationCount);

    if (symKey.isEmpty()) {
        qCCritical(logCategory) << "EncryptionKey: PBKDF2 key derivation failed";
        return {};
    }

    return QCA::SymmetricKey(std::move(symKey));
}

std::optional<QCA::SecureArray> encrypt(
    const char *label,
    QCA::Direction direction,
    const Encryption::SecureArray &data,
    const QCA::SymmetricKey &key,
    const QCA::InitializationVector &iv)
{
    QCA::Cipher cipher(
        encryptionAlgorithm,
        QCA::Cipher::CBC,
        QCA::Cipher::DefaultPadding,
        direction,
        key,
        iv
    );

    QCA::SecureArray encrypted = cipher.update(data);
    if (!cipher.ok()) {
        qCCritical(logCategory) << label << "encryption failed";
        return std::nullopt;
    }

    encrypted += cipher.final();
    if (!cipher.ok()) {
        qCCritical(logCategory) << label << "finalization failed";
        return std::nullopt;
    }

    if (direction == QCA::Encode) {
        Encryption::SecureArray result(iv);
        result.append(encrypted);
        return QCA::SecureArray(result);
    }

    return encrypted;
}

bool constantTimeEquals(const QByteArray &left, const QByteArray &right)
{
    if (left.size() != right.size())
        return false;

    unsigned char diff = 0;
    for (int i = 0; i < left.size(); ++i)
        diff |= static_cast<unsigned char>(left[i]) ^ static_cast<unsigned char>(right[i]);

    return diff == 0;
}

QByteArray hkdfExpandSha256SingleBlock(const QByteArray &prk, const QByteArray &info, int length)
{
    // HKDF-Expand with a single block (N = 1), suitable for up to 32 bytes with SHA-256.
    // T(1) = HMAC-Hash(PRK, T(0) | info | 0x01) with T(0) = empty.
    Q_ASSERT(length > 0 && length <= 32);

    QByteArray data;
    data.reserve(info.size() + 1);
    data.append(info);
    data.append(char(0x01));

    const QByteArray okm = QMessageAuthenticationCode::hash(
        data,
        prk,
        QCryptographicHash::Sha256
    );

    return okm.left(length);
}

QByteArray hmacKeyFor(const QCA::SymmetricKey &key)
{
    // Derive a separate HMAC key from the encryption key using HKDF-SHA256.
    static const QByteArray info = QByteArrayLiteral("copyq-hmac-v1");
    const Encryption::Cleared<QByteArray> ikm = key.toByteArray();
    const QByteArray prk = QMessageAuthenticationCode::hash(
        ikm.value(),
        QByteArray(hkdfSha256HashLength, '\0'),
        QCryptographicHash::Sha256
    );
    return hkdfExpandSha256SingleBlock(prk, info, hkdfSha256HashLength);
}

// HKDF with HMAC-SHA256 as specified in RFC 5869.
// ikm  - input keying material
// salt - optional salt (here: per-message salt)
// info - context and application specific information
// length - number of bytes to generate
QByteArray hkdfSha256(const QByteArray &ikm,
                       const QByteArray &salt,
                       const QByteArray &info,
                       int length)
{
    if (length <= 0) {
        return QByteArray();
    }
    if (length > hkdfSha256MaxLength) {
        return QByteArray();
    }

    // HKDF-Extract: PRK = HMAC(salt, IKM)
    QByteArray effectiveSalt = salt;
    if (effectiveSalt.isEmpty()) {
        // If salt is not provided, use a zero-filled salt of HashLen bytes.
        effectiveSalt = QByteArray(hkdfSha256HashLength, '\0');
    }

    const QByteArray prk = QMessageAuthenticationCode::hash(
        ikm,
        effectiveSalt,
        QCryptographicHash::Sha256
    );

    // HKDF-Expand
    QByteArray okm;
    okm.reserve(length);

    QByteArray previousBlock;
    const int iterations =
        (length + hkdfSha256HashLength - 1) / hkdfSha256HashLength; // ceil(length / HashLen)
    for (int i = 1; i <= iterations; ++i) {
        QByteArray data = previousBlock;
        data += info;
        data += static_cast<char>(i);

        previousBlock = QMessageAuthenticationCode::hash(
            data,
            prk,
            QCryptographicHash::Sha256
        );

        okm += previousBlock;
    }

    return okm.left(length);
}

QCA::SymmetricKey derivePerMessageKey(const QCA::SymmetricKey &key, const QByteArray &salt)
{
    // Derive a 256-bit per-message key using HKDF-SHA256.
    const Encryption::Cleared<QByteArray> ikm = key.toByteArray();
    const QByteArray info = QByteArrayLiteral("copyq-msg-key-v1");
    const int derivedKeyLength = aes256KeySize;

    const QByteArray okm = hkdfSha256(ikm.value(), salt, info, derivedKeyLength);
    return QCA::SymmetricKey(okm);
}

QByteArray hmacTagFor(const QByteArray &payload, const QCA::SymmetricKey &key)
{
    return QMessageAuthenticationCode::hash(
        payload,
        hmacKeyFor(key),
        QCryptographicHash::Sha256
    );
}

std::optional<Encryption::SecureArray> encryptAuthenticated(
    const char *label,
    const Encryption::SecureArray &data,
    const QCA::SymmetricKey &key)
{
    const QByteArray salt = QCA::Random::randomArray(perMessageSaltSize).toByteArray();
    const QCA::SymmetricKey messageKey = derivePerMessageKey(key, salt);
    if (messageKey.isEmpty())
        return std::nullopt;

    const QCA::InitializationVector iv = QCA::Random::randomArray(aesBlockSize);
    const auto encryptedData = ::encrypt(label, QCA::Encode, data, messageKey, iv);
    if (!encryptedData)
        return std::nullopt;

    const Encryption::Cleared<QByteArray> payloadData(encryptedData->toByteArray());
    QByteArray payload = encryptedDataMagic + salt + payloadData.value();
    payload += hmacTagFor(payload, messageKey);

    return Encryption::SecureArray(payload);
}

std::optional<Encryption::SecureArray> decryptAuthenticated(
    const char *label,
    const QByteArray &encryptedData,
    const QCA::SymmetricKey &key)
{
    if (!encryptedData.startsWith(encryptedDataMagic)) {
        qCCritical(logCategory) << label << "rejected: unsupported legacy encryption format";
        return std::nullopt;
    }

    const int minSize = encryptedDataMagic.size() + perMessageSaltSize + aesBlockSize + aesBlockSize + hmacTagSize;
    if (encryptedData.size() < minSize) {
        qCCritical(logCategory) << label << "rejected: encrypted payload is too short";
        return std::nullopt;
    }

    const QByteArray payload = encryptedData.left(encryptedData.size() - hmacTagSize);
    const QByteArray salt = payload.mid(encryptedDataMagic.size(), perMessageSaltSize);
    const QCA::SymmetricKey messageKey = derivePerMessageKey(key, salt);
    if (messageKey.isEmpty()) {
        qCCritical(logCategory) << label << "failed to derive per-message key";
        return std::nullopt;
    }

    const QByteArray expectedTag = hmacTagFor(payload, messageKey);
    const QByteArray actualTag = encryptedData.right(hmacTagSize);
    if (!constantTimeEquals(expectedTag, actualTag)) {
        qCCritical(logCategory) << label << "authentication failed";
        return std::nullopt;
    }

    const QByteArray legacyEncrypted =
        payload.mid(encryptedDataMagic.size() + perMessageSaltSize);
    const QCA::InitializationVector iv(legacyEncrypted.left(aesBlockSize));
    const QCA::SecureArray ciphertext(legacyEncrypted.mid(aesBlockSize));
    return ::encrypt(label, QCA::Decode, ciphertext, messageKey, iv);
}

} // namespace

namespace Encryption {

EncryptionKey::EncryptionKey(const SecureArray &password)
    : m_dek(deriveFromPasswordAndSalt(password, Salt(QByteArray("copyq_salt_v1"))))
{
}

EncryptionKey::~EncryptionKey()
{
    m_dek.clear();
}

bool EncryptionKey::generateRandomDEK()
{
    m_dek = QCA::SymmetricKey(aes256KeySize);

    if (m_dek.isEmpty()) {
        qCCritical(logCategory) << "Failed to generate random DEK";
        return false;
    }

    qCDebug(logCategory) << "Generated new random DEK";
    return true;
}

EncryptionKey::EncryptionKey(
    const SecureArray &password,
    const SecureArray &wrappedDEK,
    const Salt &kekSalt)
{
    if (wrappedDEK.isEmpty()) {
        qCCritical(logCategory) << "Cannot unwrap DEK: empty wrapped DEK";
        return;
    }

    if (kekSalt.isEmpty()) {
        qCCritical(logCategory) << "Cannot unwrap DEK: empty KEK salt";
        return;
    }

    const QCA::SymmetricKey kek = deriveFromPasswordAndSalt(password, kekSalt);
    if (kek.isEmpty()) {
        qCCritical(logCategory) << "Failed to derive KEK from password";
        return;
    }

    const auto decryptedDEK = decryptAuthenticated(
        "Unwrap DEK:",
        wrappedDEK.toByteArray(),
        kek
    );
    if (!decryptedDEK)
        return;

    m_dek = QCA::SymmetricKey(*decryptedDEK);
    if (m_dek.isEmpty()) {
        qCCritical(logCategory) << "Unwrapped DEK is empty";
        return;
    }
}

SecureArray EncryptionKey::wrapDEK(const SecureArray &password, const Salt &kekSalt) const
{
    if (m_dek.isEmpty()) {
        qCCritical(logCategory) << "Cannot wrap DEK: DEK is empty";
        return {};
    }

    if (kekSalt.isEmpty()) {
        qCCritical(logCategory) << "Cannot wrap DEK: empty KEK salt";
        return {};
    }

    const QCA::SymmetricKey kek = deriveFromPasswordAndSalt(password, kekSalt);
    if (kek.isEmpty()) {
        qCCritical(logCategory) << "Failed to derive KEK for wrapping";
        return {};
    }

    const auto wrapped = encryptAuthenticated("Wrap DEK:", m_dek.toByteArray(), kek);
    return wrapped.value_or(SecureArray());
}

bool EncryptionKey::isValid() const
{
    return !m_dek.isEmpty();
}

void EncryptionKey::clear()
{
    m_dek.clear();
}

bool initialize()
{
    static bool initialized = initializeQCA();
    return initialized;
}

QByteArray encrypt(const SecureArray &data, const EncryptionKey &key)
{
    if (!canUseEncryptionKey("Cannot encrypt:", key.symmetricKey())) {
        return QByteArray();
    }

    const auto encrypted =
        encryptAuthenticated("Encrypt data:", data, key.symmetricKey());
    return encrypted ? encrypted->toByteArray() : QByteArray();
}

QByteArray decrypt(const QByteArray &encryptedData, const EncryptionKey &key)
{
    if (!canUseEncryptionKey("Cannot decrypt:", key.symmetricKey())) {
        return QByteArray();
    }

    const auto decrypted =
        decryptAuthenticated("Decrypt data:", encryptedData, key.symmetricKey());
    return decrypted ? decrypted->toByteArray() : QByteArray();
}

void removeEncryptionKeys()
{
    const QString dekFilePath = ::dekFilePath();
    if (QFile::exists(dekFilePath)) {
        if (QFile::remove(dekFilePath)) {
            qCInfo(logCategory) << "Removed encryption key file:" << dekFilePath;
        } else {
            qCWarning(logCategory) << "Failed to remove encryption key file:" << dekFilePath;
        }
    }

    const QString saltFilePath = ::saltFilePath();
    if (QFile::exists(saltFilePath)) {
        if (QFile::remove(saltFilePath)) {
            qCInfo(logCategory) << "Removed KEK salt file:" << saltFilePath;
        } else {
            qCWarning(logCategory) << "Failed to remove KEK salt file:" << saltFilePath;
        }
    }
}

EncryptionKey saveKey(const EncryptionKey &key, const SecureArray &newPassword)
{
    if (newPassword.isEmpty()) {
        qCCritical(logCategory) << "Password cannot be empty";
        return {};
    }

    if (!key.isValid()) {
        qCCritical(logCategory) << "Encryption key is not valid";
        return {};
    }

    const QCA::SecureArray newKekSalt = QCA::Random::randomArray(perMessageSaltSize);
    if (newKekSalt.isEmpty()) {
        qCCritical(logCategory) << "Failed to generate new KEK salt";
        return {};
    }

    // Re-wrap DEK with new password
    // The DEK itself remains unchanged, we just encrypt it with a new KEK
    const SecureArray newWrappedDEK = key.wrapDEK(newPassword, newKekSalt);
    if (newWrappedDEK.isEmpty()) {
        qCCritical(logCategory) << "Failed to wrap DEK with new password";
        return {};
    }

    qCDebug(logCategory) << "Successfully wrapped DEK with new password";

    if (!backupEncryptionFiles()) {
        qCCritical(logCategory) << "Failed to backup encryption files";
        return {};
    }

    if ( !saveFile(::dekFilePath(), newWrappedDEK)
      || !saveFile(::saltFilePath(), newKekSalt) )
    {
        qCCritical(logCategory) << "Failed to save new key - rolling back";
        restoreEncryptionFiles();
        return {};
    }

    const EncryptionKey verifiedKey(newPassword, newWrappedDEK, newKekSalt);
    if (!verifiedKey.isValid()) {
        qCCritical(logCategory)
            << "Verification failed: cannot unwrap DEK with new password - rolling back";
        restoreEncryptionFiles();
        return {};
    }

    // Verify the unwrapped DEK matches the original
    if (verifiedKey.exportDEK() != key.exportDEK()) {
        qCCritical(logCategory)
            << "Verification failed: unwrapped DEK does not match original - rolling back";
        restoreEncryptionFiles();
        return {};
    }

    removeEncryptionBackups();

    return EncryptionKey(newPassword, newWrappedDEK, newKekSalt);
}

SecureArray loadWrappedDEK()
{
    return loadNonEmptySecureFile(::dekFilePath(), "Wrapped DEK");
}

Salt loadKEKSalt()
{
    return Salt(loadNonEmptySecureFile(::saltFilePath(), "KEK salt"));
}

} // namespace Encryption

#else // !WITH_QCA_ENCRYPTION

// Stub implementations when QCA is not available

namespace Encryption {

EncryptionKey::EncryptionKey(const SecureArray &)
{
}

EncryptionKey::EncryptionKey(const SecureArray &, const SecureArray &, const Salt &)
{
}

EncryptionKey::~EncryptionKey()
{
}

bool EncryptionKey::generateRandomDEK()
{
    return false;
}

QByteArray EncryptionKey::wrapDEK(const QByteArray &, const QByteArray &) const
{
    return QByteArray();
}

bool EncryptionKey::isValid() const
{
    return false;
}

void EncryptionKey::clear()
{
}

bool initialize()
{
    return false;
}

QByteArray encrypt(const QByteArray &, const EncryptionKey &)
{
    return QByteArray();
}

QByteArray decrypt(const QByteArray &, const EncryptionKey &)
{
    return QByteArray();
}

EncryptionKey saveKey(const EncryptionKey &, const SecureArray &)
{
    return {};
}

SecureArray loadWrappedDEK()
{
    return {};
}

QByteArray loadKEKSalt()
{
    return QByteArray();
}

void removeEncryptionKeys()
{
}

} // namespace Encryption

#endif // WITH_QCA_ENCRYPTION
