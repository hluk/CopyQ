// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/encryption.h"

class QWidget;
class QAbstractItemModel;
class QIODevice;
class ItemFactory;
class QProgressDialog;

#include <QtContainerFwd>

/**
 * Prompt for encryption password and derive encryption key.
 * This function handles loading password from environment or keyring,
 * prompting the user for password (with confirmation on first setup),
 * and verifying against stored hash.
 */
Encryption::EncryptionKey promptForEncryptionPassword(QWidget *parent);

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
    ItemFactory *itemFactory,
    const Encryption::EncryptionKey &oldKey,
    Encryption::EncryptionKey &newKey,
    int maxItems,
    QWidget *parent
);
