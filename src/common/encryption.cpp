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

    // QCA Initializer - must be kept alive for the entire application
    static auto qcaInit = new QCA::Initializer();
    Q_UNUSED(qcaInit);

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

bool saveFile(const QString &fileName, const QByteArray &data)
{
    QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        qCCritical(logCategory)
            << "Failed to open for writing:" << fileName
            << "- error:" << file.errorString();
        return false;
    }

    if (file.write(data) != data.size()) {
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

} // namespace

namespace Encryption {

EncryptionKey::~EncryptionKey()
{
    m_dek.clear();
    m_kek.clear();
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

bool EncryptionKey::unwrapDEK(const QByteArray &password, const QByteArray &wrappedDEK, const QByteArray &kekSalt)
{
    if (password.isEmpty()) {
        qCCritical(logCategory) << "Cannot unwrap DEK: empty password";
        return false;
    }

    if (wrappedDEK.isEmpty()) {
        qCCritical(logCategory) << "Cannot unwrap DEK: empty wrapped DEK";
        return false;
    }

    if (kekSalt.isEmpty()) {
        qCCritical(logCategory) << "Cannot unwrap DEK: empty KEK salt";
        return false;
    }

    // Derive KEK from password using the provided salt
    QCA::PBKDF2 pbkdf2(QStringLiteral("sha1"));
    m_kek = pbkdf2.makeKey(
        QCA::SecureArray(password),
        QCA::InitializationVector(kekSalt),
        32, // 256 bits
        100000 // iterations (higher than old format for better security)
    );

    if (m_kek.isEmpty()) {
        qCCritical(logCategory) << "Failed to derive KEK from password";
        return false;
    }

    // Decrypt (unwrap) the DEK using the KEK
    // The wrappedDEK contains: IV (16 bytes) + encrypted DEK
    if (wrappedDEK.size() < 16) {
        qCCritical(logCategory) << "Invalid wrapped DEK: too short";
        return false;
    }

    QCA::InitializationVector iv(wrappedDEK.left(16));
    QByteArray encryptedDEK = wrappedDEK.mid(16);

    QCA::Cipher cipher(
        encryptionAlgorithm,
        QCA::Cipher::CBC,
        QCA::Cipher::DefaultPadding,
        QCA::Decode,
        m_kek,
        iv
    );

    QCA::SecureArray decryptedDEK = cipher.update(QCA::SecureArray(encryptedDEK));
    if (!cipher.ok()) {
        qCCritical(logCategory) << "Failed to unwrap DEK: decryption failed";
        return false;
    }

    decryptedDEK += cipher.final();
    if (!cipher.ok()) {
        qCCritical(logCategory) << "Failed to unwrap DEK: finalization failed - wrong password?";
        return false;
    }

    m_dek = QCA::SymmetricKey(decryptedDEK);

    if (m_dek.isEmpty()) {
        qCCritical(logCategory) << "Unwrapped DEK is empty";
        return false;
    }

    qCDebug(logCategory) << "Successfully unwrapped DEK";
    return true;
}

QByteArray EncryptionKey::wrapDEK(const QByteArray &password, const QByteArray &kekSalt) const
{
    if (m_dek.isEmpty()) {
        qCCritical(logCategory) << "Cannot wrap DEK: DEK is empty";
        return QByteArray();
    }

    if (password.isEmpty()) {
        qCCritical(logCategory) << "Cannot wrap DEK: empty password";
        return QByteArray();
    }

    if (kekSalt.isEmpty()) {
        qCCritical(logCategory) << "Cannot wrap DEK: empty KEK salt";
        return QByteArray();
    }

    // Derive KEK from password using the provided salt
    QCA::PBKDF2 pbkdf2(QStringLiteral("sha1"));
    QCA::SymmetricKey kek = pbkdf2.makeKey(
        QCA::SecureArray(password),
        QCA::InitializationVector(kekSalt),
        32, // 256 bits
        100000 // iterations
    );

    if (kek.isEmpty()) {
        qCCritical(logCategory) << "Failed to derive KEK for wrapping";
        return QByteArray();
    }

    // Encrypt (wrap) the DEK using the KEK
    QCA::InitializationVector iv = QCA::Random::randomArray(16);

    QCA::Cipher cipher(
        encryptionAlgorithm,
        QCA::Cipher::CBC,
        QCA::Cipher::DefaultPadding,
        QCA::Encode,
        kek,
        iv
    );

    QCA::SecureArray encryptedDEK = cipher.update(QCA::SecureArray(m_dek.toByteArray()));
    if (!cipher.ok()) {
        qCCritical(logCategory) << "Failed to wrap DEK: encryption failed";
        return QByteArray();
    }

    encryptedDEK += cipher.final();
    if (!cipher.ok()) {
        qCCritical(logCategory) << "Failed to wrap DEK: finalization failed";
        return QByteArray();
    }

    // Prepend IV to encrypted DEK
    QByteArray result = iv.toByteArray();
    result.append(encryptedDEK.toByteArray());

    qCDebug(logCategory) << "Successfully wrapped DEK";
    return result;
}

bool EncryptionKey::deriveFromPassword(const QByteArray &password)
{
    // This method derives the encryption key directly from the password using a fixed salt.
    // It's used for tab recovery when a tab was encrypted with a different password.
    // For normal operation, unwrapDEK() should be used instead.
    if (password.isEmpty()) {
        qCCritical(logCategory) << "EncryptionKey: Cannot derive key from empty password";
        return false;
    }

    // Derive key from password using PBKDF2 with a fixed salt
    // Using SHA1 as it's widely supported and still secure for PBKDF2
    QCA::SecureArray passwordArray(password);
    QCA::PBKDF2 pbkdf2(QStringLiteral("sha1"));
    QCA::SymmetricKey key = pbkdf2.makeKey(
        passwordArray,
        QCA::InitializationVector(QByteArray("copyq_salt_v1")),
        32, // 256 bits
        10000 // iterations
    );

    if (key.isEmpty()) {
        qCCritical(logCategory) << "EncryptionKey: PBKDF2 key derivation failed";
        return false;
    }

    m_dek = key;
    qCDebug(logCategory) << "Derived key directly from password";
    return true;
}

bool EncryptionKey::isValid() const
{
    return !m_dek.isEmpty();
}

void EncryptionKey::clear()
{
    m_dek.clear();
    m_kek.clear();
}

bool initialize()
{
    static bool initialized = initializeQCA();
    return initialized;
}

QByteArray encrypt(const QByteArray &data, const EncryptionKey &key)
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

    QCA::Cipher cipher(
        encryptionAlgorithm,
        QCA::Cipher::CBC,
        QCA::Cipher::DefaultPadding,
        QCA::Encode,
        key.symmetricKey(),
        iv
    );

    QCA::SecureArray encrypted = cipher.update(QCA::SecureArray(data));
    if ( !cipher.ok() ) {
        qCCritical(logCategory) << "Encryption failed";
        return QByteArray();
    }

    encrypted += cipher.final();
    if ( !cipher.ok() ) {
        qCCritical(logCategory) << "Encryption finalization failed";
        return QByteArray();
    }

    QByteArray result = iv.toByteArray();
    result.append(encrypted.toByteArray());

    return result;
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
    QByteArray ciphertext = encryptedData.mid(16);

    QCA::Cipher cipher(
        encryptionAlgorithm,
        QCA::Cipher::CBC,
        QCA::Cipher::DefaultPadding,
        QCA::Decode,
        key.symmetricKey(),
        iv
    );

    QCA::SecureArray decrypted = cipher.update(QCA::SecureArray(ciphertext));
    if ( !cipher.ok() ) {
        qCCritical(logCategory) << "Decryption failed";
        return QByteArray();
    }

    decrypted += cipher.final();
    if ( !cipher.ok() ) {
        qCCritical(logCategory) << "Decryption finalization failed - wrong password?";
        return QByteArray();
    }

    return decrypted.toByteArray();
}

bool savePasswordToKeyring(const QByteArray &password)
{
    // TODO: Implement keyring integration
    // This would use platform-specific APIs:
    // - KWallet on KDE
    // - Gnome Keyring on GNOME
    // - Credential Manager on Windows
    // - Keychain on macOS

    // For now, just log a warning
    qCWarning(logCategory) << "Keyring integration not yet implemented";
    Q_UNUSED(password)
    return false;
}

QByteArray loadPasswordFromKeyring()
{
    // TODO: Implement keyring integration
    qCDebug(logCategory) << "Keyring integration not yet implemented";
    return QByteArray();
}

QString generatePasswordHash(const QByteArray &password)
{
    if (password.isEmpty()) {
        qCWarning(logCategory) << "Cannot generate hash from empty password";
        return QString();
    }

    // Generate a random salt (16 bytes = 128 bits)
    QCA::SecureArray salt = QCA::Random::randomArray(16);

    // Derive hash from password using PBKDF2
    // Using 100,000 iterations for security (more than the 10,000 used for encryption key)
    // Using SHA1 as it's widely supported and still secure for PBKDF2
    // (SHA1 vulnerabilities don't apply to HMAC-based constructions like PBKDF2)
    QCA::PBKDF2 pbkdf2(QStringLiteral("sha1"));
    QCA::SymmetricKey hash = pbkdf2.makeKey(
        QCA::SecureArray(password),
        QCA::InitializationVector(salt.toByteArray()),
        32, // 256 bits
        100000 // iterations
    );

    if (hash.isEmpty()) {
        qCCritical(logCategory) << "Failed to generate password hash";
        return QString();
    }

    QByteArray combined = salt.toByteArray() + hash.toByteArray();
    return QString::fromLatin1(combined.toBase64());
}

bool verifyPasswordHash(const QByteArray &password, const QString &hash)
{
    if (password.isEmpty() || hash.isEmpty()) {
        return false;
    }

    // Decode the hash
    QByteArray combined = QByteArray::fromBase64(hash.toLatin1());
    if (combined.size() != 48) { // 16 bytes salt + 32 bytes hash
        qCWarning(logCategory) << "Invalid password hash format";
        return false;
    }

    QByteArray salt = combined.left(16);
    QByteArray storedHash = combined.mid(16);

    // Derive hash from provided password using the same salt
    // Using SHA1 as it's widely supported and still secure for PBKDF2
    // (SHA1 vulnerabilities don't apply to HMAC-based constructions like PBKDF2)
    QCA::PBKDF2 pbkdf2(QStringLiteral("sha1"));
    QCA::SymmetricKey derivedHash = pbkdf2.makeKey(
        QCA::SecureArray(password),
        QCA::InitializationVector(salt),
        32, // 256 bits
        100000 // iterations
    );

    if (derivedHash.isEmpty()) {
        qCCritical(logCategory) << "Failed to derive password hash for verification";
        return false;
    }

    return storedHash == derivedHash.toByteArray();
}

bool savePasswordHash(const QString &hash)
{
    if (hash.isEmpty()) {
        qCWarning(logCategory) << "Cannot save empty password hash";
        return false;
    }

    const QString hashFilePath = ::hashFilePath();
    QFile file(hashFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCCritical(logCategory) << "Failed to open password hash file" << hashFilePath;
        return false;
    }

    QByteArray hashData = hash.toUtf8();
    if (file.write(hashData) != hashData.size()) {
        qCCritical(logCategory) << "Failed to write password hash to file";
        return false;
    }

    file.close();
    qCDebug(logCategory) << "Saved password hash file" << hashFilePath;

#ifndef Q_OS_WIN
    // Set restrictive permissions on Unix systems (owner read/write only)
    if (!file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        qCWarning(logCategory)
            << "Failed to set restrictive permissions on password hash file"
            << "- error:" << file.errorString();
    }
#endif

    return true;
}

QString loadPasswordHash()
{
    const QString hashFilePath = ::hashFilePath();
    if (!QFileInfo::exists(hashFilePath)) {
        return QString();
    }

    QFile file(hashFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCCritical(logCategory)
            << "Failed to open password hash file" << hashFilePath
            << "- error:" << file.errorString();
        return QString();
    }

    const QByteArray hashData = file.readAll();
    file.close();

    if (hashData.isEmpty()) {
        qCWarning(logCategory) << "Password hash file is empty:" << hashFilePath;
        return QString();
    }

    const QString hash = QString::fromUtf8(hashData);
    qCDebug(logCategory) << "Password hash loaded from secure location:" << hashFilePath;
    return hash;
}

QByteArray generateKEKSalt()
{
    // Generate a random 16-byte salt
    QCA::SecureArray salt = QCA::Random::randomArray(16);
    if (salt.isEmpty()) {
        qCCritical(logCategory) << "Failed to generate KEK salt";
        return QByteArray();
    }
    return salt.toByteArray();
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

EncryptionKey saveKey(const EncryptionKey &key, const QByteArray &newPassword)
{
    if (newPassword.isEmpty()) {
        qCCritical(logCategory) << "Password cannot be empty";
        return {};
    }

    if (!key.isValid()) {
        qCCritical(logCategory) << "Encryption key is not valid";
        return {};
    }

    const QByteArray newKekSalt = generateKEKSalt();
    if (newKekSalt.isEmpty()) {
        qCCritical(logCategory) << "Failed to generate new KEK salt";
        return {};
    }

    // Re-wrap DEK with new password
    // The DEK itself remains unchanged, we just encrypt it with a new KEK
    const QByteArray newWrappedDEK = key.wrapDEK(newPassword, newKekSalt);
    if (newWrappedDEK.isEmpty()) {
        qCCritical(logCategory) << "Failed to wrap DEK with new password";
        return {};
    }

    qCDebug(logCategory) << "Successfully wrapped DEK with new password";

    const QString newHash = generatePasswordHash(newPassword);
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
      || !saveFile(::hashFilePath(), newHash.toUtf8()) )
    {
        qCCritical(logCategory) << "Failed to save new key - rolling back";
        restoreEncryptionFiles();
        return {};
    }

    EncryptionKey newKey;
    if (!newKey.unwrapDEK(newPassword, newWrappedDEK, newKekSalt)) {
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

QByteArray loadWrappedDEK()
{
    const QString dekFilePath = ::dekFilePath();
    if (!QFileInfo::exists(dekFilePath)) {
        return QByteArray();
    }

    QFile file(dekFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCCritical(logCategory)
            << "Failed to open wrapped DEK file" << dekFilePath
            << "- error:" << file.errorString();
        return QByteArray();
    }

    const QByteArray wrappedDEK = file.readAll();
    file.close();

    if (wrappedDEK.isEmpty()) {
        qCWarning(logCategory) << "Wrapped DEK file is empty";
        return QByteArray();
    }

    qCDebug(logCategory) << "Wrapped DEK loaded from settings directory:" << dekFilePath;
    return wrappedDEK;
}

QByteArray loadKEKSalt()
{
    const QString saltFilePath = ::saltFilePath();
    if (!QFileInfo::exists(saltFilePath)) {
        return QByteArray();
    }

    QFile file(saltFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCCritical(logCategory)
            << "Failed to open KEK salt file" << saltFilePath
            << "- error:" << file.errorString();
        return QByteArray();
    }

    const QByteArray kekSalt = file.readAll();
    file.close();

    if (kekSalt.isEmpty()) {
        qCWarning(logCategory) << "KEK salt file is empty";
        return QByteArray();
    }

    qCDebug(logCategory) << "KEK salt loaded from settings directory:" << saltFilePath;
    return kekSalt;
}

} // namespace Encryption

#else // !WITH_QCA_ENCRYPTION

// Stub implementations when QCA is not available

namespace Encryption {

EncryptionKey::~EncryptionKey()
{
}

bool EncryptionKey::generateRandomDEK()
{
    return false;
}

bool EncryptionKey::unwrapDEK(const QByteArray &, const QByteArray &, const QByteArray &)
{
    return false;
}

QByteArray EncryptionKey::wrapDEK(const QByteArray &, const QByteArray &) const
{
    return QByteArray();
}

bool EncryptionKey::deriveFromPassword(const QByteArray &)
{
    return false;
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

bool savePasswordToKeyring(const QByteArray &)
{
    return false;
}

QByteArray loadPasswordFromKeyring()
{
    return QByteArray();
}

QString generatePasswordHash(const QByteArray &)
{
    return QString();
}

bool verifyPasswordHash(const QByteArray &, const QString &)
{
    return false;
}

bool savePasswordHash(const QString &)
{
    return false;
}

QString loadPasswordHash()
{
    return QString();
}

QByteArray generateKEKSalt()
{
    return QByteArray();
}

EncryptionKey saveKey(const EncryptionKey &, const QByteArray &)
{
    return {};
}

QByteArray loadWrappedDEK()
{
    return QByteArray();
}

QByteArray loadKEKSalt()
{
    return QByteArray();
}

void removeEncryptionKeys()
{
}

bool changePassword(const QByteArray &, const QByteArray &, EncryptionKey &)
{
    return false;
}

} // namespace Encryption

#endif // WITH_QCA_ENCRYPTION
