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
#include <QLoggingCategory>
#include <QPluginLoader>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QtCrypto>

namespace {

Q_DECLARE_LOGGING_CATEGORY(logCategory)
Q_LOGGING_CATEGORY(logCategory, "copyq.encryption")

// Cipher specification for QCA::isSupported() check (full name with mode and padding)
const QLatin1String encryptionCipher("aes256-cbc-pkcs7");
// Algorithm name for QCA::Cipher constructor (without mode/padding)
const QLatin1String encryptionAlgorithm("aes256");

QString dekFilePath() { return getConfigurationFilePath("wrapped_dek.dat"); }
QString saltFilePath() { return getConfigurationFilePath("kek_salt.dat"); }
QDir dataDir() {
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(path);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        qCCritical(logCategory) << "Failed to create data directory" << path;
    }
    return dir;
}
QString hashFilePath() { return dataDir().filePath(QStringLiteral(".keydata")); }

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
        && backupFile(::saltFilePath())
        && backupFile(::hashFilePath());
}

bool restoreEncryptionFiles()
{
    return restoreBackupFile(::dekFilePath())
        && restoreBackupFile(::saltFilePath())
        && restoreBackupFile(::hashFilePath());
}

void removeEncryptionBackups()
{
    removeBackup(::dekFilePath());
    removeBackup(::saltFilePath());
    removeBackup(::hashFilePath());
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
    const QCA::SymmetricKey symKey = pbkdf2.makeKey(
        password,
        salt,
        32, // 256 bits
        100'000 // iterations
    );

    if (symKey.isEmpty()) {
        qCCritical(logCategory) << "EncryptionKey: PBKDF2 key derivation failed";
        return {};
    }

    return symKey;
}

QCA::SecureArray encrypt(
    const char *label,
    QCA::Direction direction,
    const Encryption::SecureArray &data,
    const QCA::SymmetricKey &key,
    const QCA::InitializationVector &iv,
    bool *ok = nullptr)
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
        if (ok)
            *ok = false;
        return {};
    }

    encrypted += cipher.final();
    if (!cipher.ok()) {
        qCCritical(logCategory) << label << "finalization failed";
        if (ok)
            *ok = false;
        return {};
    }

    if (ok)
        *ok = true;

    if (direction == QCA::Encode) {
        Encryption::SecureArray result(iv);
        result.append(encrypted.toByteArray());
        return result;
    }

    return encrypted;
}

Encryption::SecureArray generatePasswordHash(const Encryption::SecureArray &password)
{
    if (password.isEmpty()) {
        qCWarning(logCategory) << "Cannot generate hash from empty password";
        return {};
    }

    // Generate a random salt (16 bytes = 128 bits)
    QCA::SecureArray salt = QCA::Random::randomArray(16);

    // Derive hash from password using PBKDF2
    // Using 100,000 iterations for security (more than the 10,000 used for encryption key)
    // Using SHA1 as it's widely supported and still secure for PBKDF2
    // (SHA1 vulnerabilities don't apply to HMAC-based constructions like PBKDF2)
    const QCA::SymmetricKey hash = deriveFromPasswordAndSalt(password, salt);
    if (hash.isEmpty()) {
        qCCritical(logCategory) << "Failed to generate password hash";
        return {};
    }

    Encryption::SecureArray combined(salt);
    combined.append(hash);
    const Encryption::Cleared<QByteArray> result(combined.toByteArray());
    const Encryption::Cleared<QByteArray> base64(result.value().toBase64());
    return Encryption::SecureArray(base64.value());
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
    // Generate a random 256-bit key
    m_dek = QCA::SymmetricKey(32);  // 32 bytes = 256 bits

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

    // Decrypt (unwrap) the DEK using the KEK
    // The wrappedDEK contains: IV (16 bytes) + encrypted DEK
    if (wrappedDEK.size() < 16) {
        qCCritical(logCategory) << "Invalid wrapped DEK: too short";
        return;
    }

    const Cleared<QByteArray> ba(wrappedDEK.toByteArray());
    const QCA::InitializationVector iv(Cleared<QByteArray>(ba.value().left(16)).value());
    const QCA::SecureArray encryptedDEK(Cleared<QByteArray>(ba.value().mid(16)).value());

    bool ok;
    const Encryption::SecureArray decryptedDEK =
        ::encrypt("Unwrap DEK:", QCA::Decode, encryptedDEK, kek, iv, &ok);
    if (!ok)
        return;

    m_dek = QCA::SymmetricKey(decryptedDEK);
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

    // Encrypt (wrap) the DEK using the KEK
    const QCA::InitializationVector iv = QCA::Random::randomArray(16);
    return ::encrypt("Wrap DEK:", QCA::Encode, m_dek, kek, iv);
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
    if ( !QCA::isSupported(encryptionCipher.data()) ) {
        qCCritical(logCategory) << "Encryption cipher not supported";
        return QByteArray();
    }

    if ( key.symmetricKey().isEmpty() ) {
        qCCritical(logCategory) << "Cannot encrypt: DEK is empty";
        return QByteArray();
    }

    QCA::InitializationVector iv = QCA::Random::randomArray(16);
    const Encryption::SecureArray encrypted =
        ::encrypt("Encrypt data:", QCA::Encode, data, key.symmetricKey(), iv);
    return encrypted.toByteArray();
}

QByteArray decrypt(const QByteArray &encryptedData, const EncryptionKey &key)
{
    if ( encryptedData.size() < 16 ) {
        qCCritical(logCategory) << "Invalid encrypted data: too short";
        return QByteArray();
    }

    if ( !QCA::isSupported(encryptionCipher.data()) ) {
        qCCritical(logCategory) << "Decryption cipher not supported";
        return QByteArray();
    }

    if ( key.symmetricKey().isEmpty() ) {
        qCCritical(logCategory) << "Cannot decrypt: DEK is empty";
        return QByteArray();
    }

    QCA::InitializationVector iv(encryptedData.left(16));
    SecureArray ciphertext(encryptedData.mid(16));
    const Encryption::SecureArray decrypted =
        ::encrypt("Decrypt data:", QCA::Decode, ciphertext, key.symmetricKey(), iv);
    return decrypted.toByteArray();
}

bool verifyPasswordHash(const SecureArray &password, const SecureArray &hash)
{
    if (password.isEmpty() || hash.isEmpty()) {
        return false;
    }

    // Decode the hash
    const Encryption::Cleared<QByteArray> base64(hash.toByteArray());
    const Encryption::Cleared<QByteArray> combined(QByteArray::fromBase64(base64.value()));
    if (combined.size() != 48) { // 16 bytes salt + 32 bytes hash
        qCWarning(logCategory) << "Invalid password hash format";
        return false;
    }

    const Encryption::Cleared<QByteArray> salt(combined.value().left(16));
    const Encryption::Cleared<QByteArray> storedHash(combined.value().mid(16));

    // Derive hash from provided password using the same salt
    // Using SHA1 as it's widely supported and still secure for PBKDF2
    // (SHA1 vulnerabilities don't apply to HMAC-based constructions like PBKDF2)
    const QCA::SymmetricKey derivedHash = deriveFromPasswordAndSalt(password, Salt(salt.value()));
    if (derivedHash.isEmpty()) {
        qCCritical(logCategory) << "Failed to derive password hash for verification";
        return false;
    }

    return storedHash.value() == Encryption::Cleared<QByteArray>(derivedHash.toByteArray()).value();
}

SecureArray loadPasswordHash()
{
    const QString hashFilePath = ::hashFilePath();
    if (!QFileInfo::exists(hashFilePath)) {
        return {};
    }

    QFile file(hashFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCCritical(logCategory)
            << "Failed to open password hash file" << hashFilePath
            << "- error:" << file.errorString();
        return {};
    }

    const Cleared<QByteArray> hashData(file.readAll());
    file.close();

    if (hashData.isEmpty()) {
        qCWarning(logCategory) << "Password hash file is empty:" << hashFilePath;
        return {};
    }

    qCDebug(logCategory) << "Password hash loaded from secure location:" << hashFilePath;
    return SecureArray(hashData.value());
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

    const QString hashFilePath = ::hashFilePath();
    if (QFile::exists(hashFilePath)) {
        if (QFile::remove(hashFilePath)) {
            qCInfo(logCategory) << "Removed password hash file:" << hashFilePath;
        } else {
            qCWarning(logCategory) << "Failed to remove password hash file:" << hashFilePath;
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

    const QCA::SecureArray newKekSalt = QCA::Random::randomArray(16);
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

    const SecureArray newHash = generatePasswordHash(newPassword);
    if (newHash.isEmpty()) {
        qCCritical(logCategory) << "Failed to generate new password hash";
        return {};
    }

    if (newKekSalt.isEmpty()) {
        qCWarning(logCategory) << "Cannot save empty password hash";
        return {};
    }

    if (!backupEncryptionFiles()) {
        qCCritical(logCategory) << "Failed to backup encryption files";
        return {};
    }

    if ( !saveFile(::dekFilePath(), newWrappedDEK)
      || !saveFile(::saltFilePath(), newKekSalt)
      || !saveFile(::hashFilePath(), newHash) )
    {
        qCCritical(logCategory) << "Failed to save new key - rolling back";
        restoreEncryptionFiles();
        return {};
    }

    EncryptionKey newKey(newPassword, newWrappedDEK, newKekSalt);
    if (!newKey.isValid()) {
        qCCritical(logCategory)
            << "Verification failed: cannot unwrap DEK with new password - rolling back";
        restoreEncryptionFiles();
        return {};
    }

    // Verify the unwrapped DEK matches the original
    if (newKey.exportDEK() != key.exportDEK()) {
        qCCritical(logCategory)
            << "Verification failed: unwrapped DEK does not match original - rolling back";
        restoreEncryptionFiles();
        return {};
    }

    removeEncryptionBackups();

    return newKey;
}

SecureArray loadWrappedDEK()
{
    const QString dekFilePath = ::dekFilePath();
    if (!QFileInfo::exists(dekFilePath)) {
        return {};
    }

    QFile file(dekFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCCritical(logCategory)
            << "Failed to open wrapped DEK file" << dekFilePath
            << "- error:" << file.errorString();
        return {};
    }

    const Cleared<QByteArray> wrappedDEK(file.readAll());
    file.close();

    if (wrappedDEK.isEmpty()) {
        qCWarning(logCategory) << "Wrapped DEK file is empty";
        return {};
    }

    qCDebug(logCategory) << "Wrapped DEK loaded from settings directory:" << dekFilePath;
    return SecureArray(wrappedDEK.value());
}

Salt loadKEKSalt()
{
    const QString saltFilePath = ::saltFilePath();
    if (!QFileInfo::exists(saltFilePath)) {
        return {};
    }

    QFile file(saltFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCCritical(logCategory)
            << "Failed to open KEK salt file" << saltFilePath
            << "- error:" << file.errorString();
        return {};
    }

    const Cleared<QByteArray> kekSalt(file.readAll());
    file.close();

    if (kekSalt.isEmpty()) {
        qCWarning(logCategory) << "KEK salt file is empty";
        return {};
    }

    qCDebug(logCategory) << "KEK salt loaded from settings directory:" << saltFilePath;
    return Salt(kekSalt.value());
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

bool verifyPasswordHash(const QByteArray &, const QString &)
{
    return false;
}

SecureArray loadPasswordHash()
{
    return {};
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
