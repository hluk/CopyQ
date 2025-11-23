// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QString>

class QDataStream;

#ifdef WITH_QCA_ENCRYPTION
#include <QtCrypto>
#endif

namespace Encryption {

/**
 * Wrapper class for encryption key using envelope encryption (key wrapping).
 *
 * Architecture:
 * - DEK (Data Encryption Key): Random 256-bit key that encrypts all tab data
 * - KEK (Key Encryption Key): Derived from user password using PBKDF2 with random salt
 * - The DEK is encrypted by the KEK and stored on disk (envelope encryption)
 *
 * Benefits:
 * - Password changes only require re-encrypting the small DEK, not all data
 * - Each user gets a unique random salt for password derivation
 * - DEK is cryptographically random, stronger than password-derived keys
 *
 * When QCA encryption is not available, this class acts as a stub
 * that always returns invalid/empty results.
 */
class EncryptionKey {
public:
    ~EncryptionKey();

    /**
     * Generate a new random Data Encryption Key (DEK).
     * Should be called once during initial setup.
     * @return true if DEK generation succeeded (always false without QCA)
     */
    bool generateRandomDEK();

    /**
     * Derive KEK from password and unwrap (decrypt) the DEK.
     * @param password Password to derive KEK from
     * @param wrappedDEK Encrypted DEK (previously encrypted with a KEK)
     * @param kekSalt Salt for KEK derivation
     * @return true if KEK derivation and DEK unwrapping succeeded
     */
    bool unwrapDEK(const QByteArray &password, const QByteArray &wrappedDEK, const QByteArray &kekSalt);

    /**
     * Wrap (encrypt) the DEK using a KEK derived from password.
     * Used when setting or changing passwords.
     * @param password Password to derive KEK from
     * @param kekSalt Salt for KEK derivation
     * @return Encrypted DEK, or empty array on failure
     */
    QByteArray wrapDEK(const QByteArray &password, const QByteArray &kekSalt) const;

    /**
     * Derive DEK directly from password without key wrapping.
     * This is used for tab recovery when a tab was encrypted with a different password.
     * For normal operation, use unwrapDEK() instead.
     * @param password Password to derive key from
     * @return true if key derivation succeeded
     */
    bool deriveFromPassword(const QByteArray &password);

    /**
     * Check if DEK is valid and ready to use.
     * @return true if valid (always false without QCA)
     */
    bool isValid() const;

    /**
     * Clear the keys from memory.
     */
    void clear();

#ifdef WITH_QCA_ENCRYPTION
    // Internal access for encryption/decryption operations
    // Returns the DEK for data encryption/decryption
    const QCA::SymmetricKey &symmetricKey() const { return m_dek; }

    /**
     * Export DEK as raw bytes for serialization.
     * WARNING: This exposes the raw encryption key - use only for secure serialization.
     * @return DEK as byte array, or empty if invalid
     */
    QByteArray exportDEK() const { return m_dek.toByteArray(); }

    /**
     * Import DEK from raw bytes (for deserialization).
     * @param dekBytes Raw DEK bytes
     * @return true if import succeeded
     */
    bool importDEK(const QByteArray &dekBytes)
    {
        m_dek = QCA::SymmetricKey(dekBytes);
        return !m_dek.isEmpty();
    }

private:
    QCA::SymmetricKey m_dek;  // Data Encryption Key (used for actual data encryption)
    QCA::SymmetricKey m_kek;  // Key Encryption Key (derived from password, used to wrap/unwrap DEK)
#endif
};

/**
 * Initialize encryption system.
 * Returns true if encryption is available and initialized successfully.
 * Always returns false when QCA is not available.
 */
bool initialize();

/**
 * Encrypt data using AES-256.
 * @param data Data to encrypt
 * @param key Encryption key to use
 * @return Encrypted data, or empty array on failure (always empty without QCA)
 */
QByteArray encrypt(const QByteArray &data, const EncryptionKey &key);

/**
 * Decrypt data that was encrypted with encrypt().
 * @param encryptedData Data to decrypt
 * @param key Encryption key to use
 * @return Decrypted data, or empty array on failure (always empty without QCA)
 */
QByteArray decrypt(const QByteArray &encryptedData, const EncryptionKey &key);

/**
 * Save password to system keyring (KWallet, Gnome Keyring, etc.)
 * Always returns false when QCA is not available.
 */
bool savePasswordToKeyring(const QByteArray &password);

/**
 * Load password from system keyring.
 * Always returns empty when QCA is not available.
 */
QByteArray loadPasswordFromKeyring();

/**
 * Generate a salted hash of the password for verification purposes.
 * Uses PBKDF2 with a random salt to create a secure hash.
 * @param password Password to hash
 * @return Base64-encoded hash string containing salt and hash, or empty on failure
 */
QString generatePasswordHash(const QByteArray &password);

/**
 * Verify a password against a previously generated hash.
 * @param password Password to verify
 * @param hash Hash string generated by generatePasswordHash()
 * @return true if password matches the hash
 */
bool verifyPasswordHash(const QByteArray &password, const QString &hash);

/**
 * Save password hash to a secure location using QStandardPaths.
 * The hash is stored in AppDataLocation with restricted permissions.
 * @param hash Password hash to save
 * @return true if saved successfully
 */
bool savePasswordHash(const QString &hash);

/**
 * Load password hash from the secure location.
 * @return Password hash, or empty string if not found or on error
 */
QString loadPasswordHash();

/** Save encryption key safely. */
EncryptionKey saveKey(const EncryptionKey &key, const QByteArray &newPassword);

/**
 * Load wrapped DEK from secure storage.
 * @return Wrapped DEK, or empty array if not found or on error
 */
QByteArray loadWrappedDEK();

/**
 * Load KEK salt from secure storage.
 * @return KEK salt, or empty array if not found or on error
 */
QByteArray loadKEKSalt();

/**
 * Generate a random salt for KEK derivation.
 * @return Random salt (16 bytes), or empty on failure
 */
QByteArray generateKEKSalt();

/**
 * Remove all encryption key files from disk.
 * This should only be called after successfully disabling encryption for all tabs.
 *
 * Removes:
 * - wrapped_dek.dat (wrapped Data Encryption Key)
 * - kek_salt.dat (Key Encryption Key salt)
 * - .keydata (password hash)
 */
void removeEncryptionKeys();

} // namespace Encryption
