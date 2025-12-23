// SPDX-License-Identifier: GPL-3.0-or-later

#include "encryptionpassword.h"

#ifdef WITH_QCA_ENCRYPTION
#include "common/common.h"
#include "common/config.h"
#include "common/encryption.h"
#include "common/keychainaccess.h"
#include "common/log.h"
#include "common/textdata.h"
#include "item/clipboardmodel.h"
#include "item/itemfactory.h"
#include "item/itemstore.h"
#include "item/itemwidget.h"
#include "item/serialize.h"
#include "gui/clipboardbrowsershared.h"

#include <QCoreApplication>
#include <QInputDialog>
#include <QIODevice>
#include <QLineEdit>
#include <QSqlDatabase>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QProgressDialog>
#include <QWidget>

namespace {

Q_DECLARE_LOGGING_CATEGORY(logCategory)
Q_LOGGING_CATEGORY(logCategory, "copyq.passwd")

QString keychainServiceName()
{
    return QStringLiteral("com.github.hluk.copyq");
}

QString keychainPasswordKey()
{
    const QString sessionName = qApp->property("CopyQ_session_name").toString();
    if (sessionName.isEmpty())
        return QStringLiteral("Password");
    return QStringLiteral("PasswordForSession-%1").arg(sessionName);
}

bool savePasswordToKeychain(const Encryption::SecureArray &password)
{
    qCInfo(logCategory) << "Saving password from system keychain";
    const Encryption::Cleared<QByteArray> ba = password.toByteArray();
    const bool result = KeychainAccess::writePassword(
        keychainServiceName(), keychainPasswordKey(), ba.value());

    if (!result)
        return false;

    qCInfo(logCategory) << "Password saved to system keychain";
    return true;
}

QByteArray loadPasswordFromKeychain()
{
    qCInfo(logCategory) << "Loading password from system keychain";
    const QByteArray result = KeychainAccess::readPassword(
        keychainServiceName(), keychainPasswordKey());

    if (result.isEmpty())
        return {};

    qCDebug(logCategory) << "Password loaded from system keychain";
    return result;
}

void activateWindow(QWidget *parent)
{
    parent->show();
    parent->activateWindow();
    parent->raise();
}

Encryption::EncryptionKey setUpPassword(const Encryption::SecureArray &password, PasswordSource prompt)
{
    Encryption::EncryptionKey key;
    if ( !key.generateRandomDEK() ) {
        log("Failed to generate random DEK", LogError);
        return {};
    }

    const Encryption::EncryptionKey newKey = Encryption::saveKey(key, password);
    if (prompt == PasswordSource::UseEnvAndKeychain && newKey.isValid())
        savePasswordToKeychain(password);

    return newKey;
}

Encryption::SecureArray getStoredPassword(PasswordSource prompt)
{
    if (prompt == PasswordSource::UseEnvAndKeychain || prompt == PasswordSource::UseEnvOnly) {
        const Encryption::Cleared<QByteArray> ba = qgetenv("COPYQ_PASSWORD");
        if ( !ba.isEmpty() ) {
            qCInfo(logCategory) << "Encryption password loaded from env variable COPYQ_PASSWORD";
            return Encryption::SecureArray(ba.value());
        }
    }

    if (prompt == PasswordSource::UseEnvAndKeychain) {
        const Encryption::Cleared<QByteArray> ba = loadPasswordFromKeychain();
        if ( !ba.isEmpty() ) {
            qCInfo(logCategory) << "Encryption password loaded from system keychain";
            return Encryption::SecureArray(ba.value());
        }
    }

    return {};
}

Encryption::SecureArray getPassword(
    QWidget *parent, const QString &title, const QString &label, bool *ok = nullptr)
{
    const Encryption::Cleared<QString> str = QInputDialog::getText(
        parent, title, label, QLineEdit::Password, QString(), ok);
    const Encryption::Cleared<QByteArray> ba(str.value().toUtf8());
    return Encryption::SecureArray(ba.value());
}

Encryption::SecureArray promptForNewDefaultPassword(QWidget *parent)
{
    return promptForNewPassword(
        QObject::tr("New Tab Encryption Password"),
        QObject::tr("Enter new password:"),
        PasswordPromptType::OnlyNonEmpty,
        parent);
}

Encryption::EncryptionKey firstPasswordSetup(QWidget *parent, PasswordSource prompt)
{
    qCInfo(logCategory) << "Setting up encryption for the first time";

    const Encryption::SecureArray storedPassword = getStoredPassword(prompt);
    if (!storedPassword.isEmpty())
        return setUpPassword(storedPassword, prompt);

    const Encryption::SecureArray newPassword = promptForNewDefaultPassword(parent);
    if (!newPassword.isEmpty())
        return setUpPassword(newPassword, prompt);

    return {};
}

} // namespace

Encryption::SecureArray promptForNewPassword(
    const QString &title, const QString &label, PasswordPromptType pwType, QWidget *parent)
{
    if ( !Encryption::initialize() ) {
        log("Failed to initialize encryption system", LogWarning);
        return {};
    }

    activateWindow(parent);
    int attempts = 0;
    const int maxAttempts = 3;
    while (attempts < maxAttempts) {
        bool ok;
        const auto password1 = getPassword(
            parent,
            title,
            attempts == 0
                ? label
                : QObject::tr("Passwords did not match. Please try again (%1/%2):").arg(attempts + 1).arg(maxAttempts),
            &ok
        );

        if (!ok) {
            log("New password input cancelled by user", LogNote);
            return {};
        }

        if (pwType == PasswordPromptType::AskOnce)
            return password1;

        if (password1.isEmpty()) {
            if (pwType == PasswordPromptType::AllowEmpty)
                return {};
            QMessageBox::warning(
                parent,
                QObject::tr("Change Password"),
                QObject::tr("New password cannot be empty.")
            );
            attempts++;
            continue;
        }

        // Ask for password confirmation
        const auto password2 = getPassword(
            parent,
            QObject::tr("Confirm Encryption Password"),
            QObject::tr("Re-enter password to confirm:"),
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

        return password1;
    }

    log("Maximum password setup attempts exceeded");
    QMessageBox::critical(
        parent,
        QObject::tr("Password Setup Failed"),
        QObject::tr("Maximum password attempts exceeded.")
    );
    return {};
}

Encryption::EncryptionKey promptForEncryptionPassword(QWidget *parent, PasswordSource prompt)
{
    if ( !Encryption::initialize() ) {
        log("Failed to initialize encryption system", LogWarning);
        return {};
    }

    const Encryption::SecureArray storedHash = Encryption::loadPasswordHash();
    const Encryption::SecureArray wrappedDEK = Encryption::loadWrappedDEK();
    const Encryption::Salt kekSalt = Encryption::loadKEKSalt();

    const bool isFirstSetup = storedHash.isEmpty() && wrappedDEK.isEmpty();
    if (isFirstSetup)
        return firstPasswordSetup(parent, prompt);

    const Encryption::SecureArray storedPassword = getStoredPassword(prompt);
    if ( !storedPassword.isEmpty() ) {
        if ( Encryption::verifyPasswordHash(storedPassword, storedHash) )
            return Encryption::EncryptionKey(storedPassword, wrappedDEK, kekSalt);
        log("Loaded password does not match the stored hash", LogWarning);
    }

    // Ask for the current password
    activateWindow(parent);
    int attempts = 0;
    const int maxAttempts = 3;
    while (attempts < maxAttempts) {
        bool ok;
        const Encryption::SecureArray password = getPassword(
            parent,
            QObject::tr("Current Tab Encryption Password"),
            attempts == 0
                ? QObject::tr("Enter current password for encrypting tab data:")
                : QObject::tr("Invalid password. Please try again (%1/%2):").arg(attempts + 1).arg(maxAttempts),
            &ok
        );

        if (!ok || password.isEmpty()) {
            log("Tab encryption password required but not provided", LogWarning);
            return {};
        }

        if ( !Encryption::verifyPasswordHash(password, storedHash) ) {
            attempts++;
            continue;
        }

        const Encryption::EncryptionKey key(password, wrappedDEK, kekSalt);

        if (prompt == PasswordSource::UseEnvAndKeychain && key.isValid())
            savePasswordToKeychain(password);

        return key;
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
    const Encryption::EncryptionKey currentKey = promptForEncryptionPassword(parent, PasswordSource::IgnoreEnvAndKeychain);
    if (!currentKey.isValid())
        return {};

    const Encryption::SecureArray newPassword = promptForNewDefaultPassword(parent);
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
    ClipboardBrowserShared *sharedData,
    const Encryption::EncryptionKey &oldKey,
    const Encryption::EncryptionKey &newKey,
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

    // Skip plugins that do not support encryption
    ItemLoaderList skipLoaders;
    for (auto &loader : sharedData->itemFactory->loaders()) {
        if (loader->isEnabled() && !loader->supportsEncryption()) {
            skipLoaders.append(loader);
            loader->setEnabled(false);
        }
    }

    QStringList failedTabs;

    for (int i = 0; i < tabNames.size(); ++i) {
        if (progress.wasCanceled()) {
            log("Tab re-encryption cancelled by user", LogWarning);
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
        QSqlDatabase db = QSqlDatabase::database("copyq_main");
        ClipboardModel model(db);

        // Set old encryption key temporarily and load items
        sharedData->encryptionKey = oldKey;
        ItemSaverPtr saver = loadItems(tabName, model, sharedData->itemFactory, maxItems);
        sharedData->encryptionKey = newKey;

        if (!saver) {
            COPYQ_LOG(QStringLiteral("Skipping encryption on unsupported tab: %1").arg(tabName));
            continue;
        }

        progress.setValue(i * 2 + 1);
        QCoreApplication::processEvents();

        const int itemCount = model.rowCount();
        COPYQ_LOG(QStringLiteral("Loaded %1 items from tab: %2").arg(itemCount).arg(tabName));

        if (!saveItems(tabName, model, saver)) {
            log(QStringLiteral("Failed to re-encrypt tab: %1").arg(tabName), LogError);
            failedTabs.append(tabName);
            continue;
        }

        COPYQ_LOG(QStringLiteral("Successfully re-encrypted tab: %1").arg(tabName));
    }

    for (auto &loader : skipLoaders)
        loader->setEnabled(true);

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

    log("Successfully re-encrypted tabs");
    return true;
}

void removePasswordFromKeychain()
{
    KeychainAccess::deletePassword(keychainServiceName(), keychainPasswordKey());
}

#else // !WITH_QCA_ENCRYPTION

Encryption::SecureArray promptForNewPassword(
    const QString &, const QString &, PasswordPromptType , QWidget *)
{
    return {};
}

Encryption::EncryptionKey promptForEncryptionPassword(QWidget *, PasswordSource)
{
    return {};
}

Encryption::EncryptionKey promptForEncryptionPasswordChange(QWidget *)
{
    return {};
}

bool reencryptTabs(
    const QStringList &,
    ClipboardBrowserShared *,
    const Encryption::EncryptionKey &,
    const Encryption::EncryptionKey &,
    int,
    QWidget *)
{
    return false;
}

void removePasswordFromKeychain()
{
}

#endif // WITH_QCA_ENCRYPTION
