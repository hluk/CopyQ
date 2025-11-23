// SPDX-License-Identifier: GPL-3.0-or-later

#include "encryptionpassword.h"

#ifdef WITH_QCA_ENCRYPTION
#include "common/common.h"
#include "common/config.h"
#include "common/encryption.h"
#include "common/log.h"
#include "common/textdata.h"
#include "item/clipboardmodel.h"
#include "item/itemfactory.h"
#include "item/itemstore.h"
#include "item/itemwidget.h"
#include "item/serialize.h"

#include <QCoreApplication>
#include <QInputDialog>
#include <QIODevice>
#include <QLineEdit>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QProgressDialog>
#include <QWidget>

namespace {

Q_DECLARE_LOGGING_CATEGORY(logCategory)
Q_LOGGING_CATEGORY(logCategory, "copyq.passwd")

Encryption::EncryptionKey setUpPassword(const QByteArray &password)
{
    Encryption::EncryptionKey key;
    if ( !key.generateRandomDEK() ) {
        log("Failed to generate random DEK", LogError);
        return {};
    }

    return Encryption::saveKey(key, password);
}

QByteArray getStoredPassword()
{
    const QByteArray passwordEnv = qgetenv("COPYQ_PASSWORD");
    if ( !passwordEnv.isEmpty() ) {
        qCInfo(logCategory) << "Encryption password loaded from env variable COPYQ_PASSWORD";
        return passwordEnv;
    }

    const QByteArray savedPassword = Encryption::loadPasswordFromKeyring();
    if ( !savedPassword.isEmpty() ) {
        qCInfo(logCategory) << "Encryption password loaded from system keyring";
        return savedPassword;
    }

    return {};
}

QByteArray promptForNewPassword(QWidget *parent)
{
    int attempts = 0;
    const int maxAttempts = 3;

    while (attempts < maxAttempts) {
        bool ok;
        const QString password1 = QInputDialog::getText(
            parent,
            QObject::tr("New Tab Encryption Password"),
            attempts == 0
                ? QObject::tr("Enter new password:")
                : QObject::tr("Passwords did not match. Please try again (%1/%2):").arg(attempts + 1).arg(maxAttempts),
            QLineEdit::Password,
            QString(),
            &ok
        );

        if (!ok) {
            log("New password input cancelled by user", LogNote);
            return {};
        }

        if (password1.isEmpty()) {
            QMessageBox::warning(
                parent,
                QObject::tr("Change Password"),
                QObject::tr("New password cannot be empty.")
            );
            attempts++;
            continue;
        }

        // Ask for password confirmation
        const QString password2 = QInputDialog::getText(
            parent,
            QObject::tr("Confirm Encryption Password"),
            QObject::tr("Re-enter password to confirm:"),
            QLineEdit::Password,
            QString(),
            &ok
        );

        if (!ok) {
            log("Password confirmation cancelled by user", LogNote);
            return {};
        }

        // Check if passwords match
        if (password1 != password2) {
            attempts++;
            continue;
        }

        return password1.toUtf8();
    }

    log("Maximum password setup attempts exceeded", LogError);
    QMessageBox::critical(
        parent,
        QObject::tr("Password Setup Failed"),
        QObject::tr("Maximum password attempts exceeded.")
    );
    return {};
}

Encryption::EncryptionKey firstPasswordSetup(QWidget *parent)
{
    qCInfo(logCategory) << "Setting up encryption for the first time";

    const QByteArray storedPassword = getStoredPassword();
    if (!storedPassword.isEmpty())
        return setUpPassword(storedPassword);

    const QByteArray newPassword = promptForNewPassword(parent);
    if (!newPassword.isEmpty())
        return setUpPassword(newPassword);

    return {};
}

Encryption::EncryptionKey unwrapDEK(
    const QByteArray &password, const QByteArray &wrappedDEK, const QByteArray &kekSalt)
{
    Encryption::EncryptionKey key;
    if ( !key.unwrapDEK(password, wrappedDEK, kekSalt) ) {
        log("Failed to unwrap DEK - this should not happen if password is correct", LogError);
        return {};
    }

    log("DEK unwrapped successfully", LogNote);
    return key;
}

} // namespace

Encryption::EncryptionKey promptForEncryptionPassword(QWidget *parent)
{
    if ( !Encryption::initialize() ) {
        log("Failed to initialize encryption system", LogWarning);
        return {};
    }

    const QString storedHash = Encryption::loadPasswordHash();
    const QByteArray wrappedDEK = Encryption::loadWrappedDEK();
    const QByteArray kekSalt = Encryption::loadKEKSalt();

    const bool isFirstSetup = storedHash.isEmpty() && wrappedDEK.isEmpty();
    if (isFirstSetup)
        return firstPasswordSetup(parent);

    const QByteArray storedPassword = getStoredPassword();
    if ( !storedPassword.isEmpty() ) {
        if ( Encryption::verifyPasswordHash(storedPassword, storedHash) )
            return unwrapDEK(storedPassword, wrappedDEK, kekSalt);
        log("Loaded password does not match the stored hash", LogWarning);
    }

    // Ask for the current password
    int attempts = 0;
    const int maxAttempts = 3;

    while (attempts < maxAttempts) {
        bool ok;
        const QString password = QInputDialog::getText(
            parent,
            QObject::tr("Current Tab Encryption Password"),
            attempts == 0
                ? QObject::tr("Enter current password for encrypting tab data:")
                : QObject::tr("Invalid password. Please try again (%1/%2):").arg(attempts + 1).arg(maxAttempts),
            QLineEdit::Password,
            QString(),
            &ok
        );

        if (!ok || password.isEmpty()) {
            log("Tab encryption password required but not provided", LogWarning);
            return {};
        }

        if ( !Encryption::verifyPasswordHash(password.toUtf8(), storedHash) ) {
            attempts++;
            continue;
        }

        return unwrapDEK(password.toUtf8(), wrappedDEK, kekSalt);
    }

    log("Maximum password attempts exceeded", LogError);
    QMessageBox::critical(
        parent,
        QObject::tr("Password Verification Failed"),
        QObject::tr("Maximum password attempts exceeded. Encryption will not be available.")
    );
    return {};
}

Encryption::EncryptionKey promptForEncryptionPasswordChange(QWidget *parent)
{
    if ( !Encryption::initialize() ) {
        log("Failed to initialize encryption system", LogWarning);
        return {};
    }

    // Always verify old password
    const Encryption::EncryptionKey currentKey = promptForEncryptionPassword(parent);
    if (!currentKey.isValid())
        return {};

    const QByteArray newPassword = promptForNewPassword(parent);
    if (newPassword.isEmpty())
        return {};

    log("Attempting to change password...", LogNote);

    const Encryption::EncryptionKey key = Encryption::saveKey(currentKey, newPassword);
    if (!key.isValid()) {
        log("Password change failed", LogError);
        QMessageBox::critical(
            parent,
            QObject::tr("Change Password Failed"),
            QObject::tr("Failed to change password. Your old password is still active. Please check the logs for details.")
        );
        return {};
    }

    log("Password changed successfully", LogNote);
    QMessageBox::information(
        parent,
        QObject::tr("Change Password Successful"),
        QObject::tr("Password has been changed successfully.")
    );

    return key;
}

bool reencryptTabs(
    const QStringList &tabNames,
    ItemFactory *itemFactory,
    const Encryption::EncryptionKey &oldKey,
    Encryption::EncryptionKey &newKey,
    int maxItems,
    QWidget *parent)
{
    QProgressDialog progress(
        QObject::tr("Re-encrypting tabs..."),
        QObject::tr("Cancel"),
        0,
        tabNames.size() * 2,
        parent
    );
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(500);  // Show after 500ms if not done
    progress.setValue(0);

    const Encryption::EncryptionKey newKeyBackup = newKey;

    QStringList failedTabs;
    for (int i = 0; i < tabNames.size(); ++i) {
        if (progress.wasCanceled()) {
            log("Tab re-encryption cancelled by user", LogWarning);
            failedTabs.append(QObject::tr("(Cancelled by user)"));
            break;
        }

        const QString &tabName = tabNames[i];

        progress.setLabelText(
            QObject::tr("Re-encrypting tab %1 of %2: %3")
                .arg(i + 1)
                .arg(tabNames.size())
                .arg(quoteString(tabName))
        );
        progress.setValue(i * 2);
        QCoreApplication::processEvents();

        COPYQ_LOG(QStringLiteral("Re-encrypting tab: %1").arg(tabName));

        // Temporary model to hold the tab's items
        ClipboardModel model;

        // Set old encryption key temporarily and load items
        newKey = oldKey;
        ItemSaverPtr saver = loadItems(tabName, model, itemFactory, maxItems);
        newKey = newKeyBackup;

        if (!saver) {
            const QString error = QStringLiteral("Failed to load tab: %1").arg(tabName);
            log(error, LogError);
            failedTabs.append(tabName);
            continue;
        }

        progress.setValue(i * 2 + 1);
        QCoreApplication::processEvents();

        const int itemCount = model.rowCount();
        COPYQ_LOG(QStringLiteral("Loaded %1 items from tab: %2").arg(itemCount).arg(tabName));

        if (!saveItems(tabName, model, saver)) {
            const QString error = QStringLiteral("Failed to save tab: %1").arg(tabName);
            log(error, LogError);
            failedTabs.append(tabName);
            continue;
        }

        COPYQ_LOG(QStringLiteral("Successfully re-encrypted tab: %1").arg(tabName));
    }

    progress.setValue(tabNames.size());

    if (!failedTabs.isEmpty()) {
        log(QStringLiteral("Failed to re-encrypt %1 out of %2 tabs")
            .arg(failedTabs.size())
            .arg(tabNames.size()), LogError);

        const QString errorMsg = QObject::tr(
            "Failed to encrypt the following tabs:\n\n%1\n\n"
            "Please check the logs for details."
        ).arg(failedTabs.join("\n"));
        QMessageBox::critical(parent, QObject::tr("Encryption Failed"), errorMsg);
        return false;
    }

    log(QStringLiteral("Successfully re-encrypted all %1 tabs").arg(tabNames.size()), LogNote);
    return true;
}

#else // !WITH_QCA_ENCRYPTION

Encryption::EncryptionKey promptForEncryptionPassword(QWidget*)
{
    return {};
}

Encryption::EncryptionKey promptForEncryptionPasswordChange(QWidget *)
{
    return {};
}

bool reencryptTabs(
    const QStringList &,
    ItemFactory *,
    const Encryption::EncryptionKey &,
    Encryption::EncryptionKey &,
    int,
    QWidget *)
{
    return false;
}

void removeEncryptionKeys()
{
}

#endif // WITH_QCA_ENCRYPTION
