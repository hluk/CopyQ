// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/encryption.h"

class ClipboardBrowserShared;
class QWidget;
class QAbstractItemModel;
class QIODevice;
class ItemFactory;
class QProgressDialog;

#include <QtContainerFwd>

enum class PasswordSource {
    UseEnvOnly,
    UseEnvAndKeychain,
    IgnoreEnvAndKeychain
};

enum class PasswordPromptType {
    OnlyNonEmpty,
    AllowEmpty,
    AskOnce
};

Encryption::SecureArray promptForNewPassword(
    const QString &title, const QString &label, PasswordPromptType pwType, QWidget *parent);

/**
 * Prompt for encryption password and derive encryption key.
 * This function handles loading password from environment or keyring,
 * prompting the user for password (with confirmation on first setup),
 * and verifying against stored hash.
 */
Encryption::EncryptionKey promptForEncryptionPassword(QWidget *parent, PasswordSource prompt);

/**
 * Change encryption password with user dialogs for verification and confirmation.
 */
Encryption::EncryptionKey promptForEncryptionPasswordChange(QWidget *parent);

/**
 * Re-encrypt tabs when enabling or disabling encryption.
 * This function iterates through tabs and re-saves them with the new encryption key.
 */
bool reencryptTabs(
    const QStringList &tabNames,
    ClipboardBrowserShared *sharedData,
    const Encryption::EncryptionKey &oldKey,
    const Encryption::EncryptionKey &newKey,
    int maxItems,
    QWidget *parent
);

void removePasswordFromKeychain();
