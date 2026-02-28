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
#include <QLoggingCategory>
#include <QMessageBox>
#include <QProgressDialog>
#include <QWidget>

namespace {

Q_DECLARE_LOGGING_CATEGORY(logCategory)
Q_LOGGING_CATEGORY(logCategory, "copyq.passwd")

constexpr int maxPasswordAttempts = 3;

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

void getPasswordAsync(
    QWidget *parent,
    const QString &title,
    const QString &label,
    std::function<void(const Encryption::SecureArray &, bool)> callback)
{
    auto *dialog = new QInputDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(title);
    dialog->setLabelText(label);
    dialog->setTextEchoMode(QLineEdit::Password);
    dialog->setTextValue(QString());
    dialog->setModal(true);
    dialog->setWindowModality(Qt::ApplicationModal);
    QObject::connect(dialog, &QInputDialog::finished, dialog, [dialog, callback = std::move(callback)](int result) mutable {
        const Encryption::Cleared<QByteArray> ba(dialog->textValue().toUtf8());
        callback(Encryption::SecureArray(ba.value()), result == QDialog::Accepted);
    });
    dialog->open();
}

void promptForNewDefaultPasswordAttemptAsync(
    QWidget *parent,
    int attempts,
    std::function<void(const Encryption::SecureArray &password)> callback)
{
    getPasswordAsync(
        parent,
        QObject::tr("New Tab Encryption Password"),
        attempts == 0
            ? QObject::tr("Enter new password:")
            : QObject::tr("Passwords did not match. Please try again (%1/%2):")
                  .arg(attempts + 1)
                  .arg(maxPasswordAttempts),
        [=](const Encryption::SecureArray &password1, bool ok) mutable {
            if (!ok) {
                log("New password input cancelled by user", LogNote);
                callback({});
                return;
            }

            if (password1.isEmpty()) {
                QMessageBox::warning(
                    parent,
                    QObject::tr("Change Password"),
                    QObject::tr("New password cannot be empty."));
                if (attempts >= maxPasswordAttempts - 1) {
                    log("Maximum password setup attempts exceeded");
                    QMessageBox::critical(
                        parent,
                        QObject::tr("Password Setup Failed"),
                        QObject::tr("Maximum password attempts exceeded."));
                    callback({});
                    return;
                }
                promptForNewDefaultPasswordAttemptAsync(parent, attempts + 1, std::move(callback));
                return;
            }

            getPasswordAsync(
                parent,
                QObject::tr("Confirm Encryption Password"),
                QObject::tr("Re-enter password to confirm:"),
                [=](const Encryption::SecureArray &password2, bool ok2) mutable {
                    if (!ok2) {
                        log("Password confirmation cancelled by user", LogNote);
                        callback({});
                        return;
                    }

                    if (password1 == password2) {
                        callback(password1);
                        return;
                    }

                    if (attempts >= maxPasswordAttempts - 1) {
                        log("Maximum password setup attempts exceeded");
                        QMessageBox::critical(
                            parent,
                            QObject::tr("Password Setup Failed"),
                            QObject::tr("Maximum password attempts exceeded."));
                        callback({});
                        return;
                    }
                    promptForNewDefaultPasswordAttemptAsync(parent, attempts + 1, std::move(callback));
                });
        });
}

void promptForEncryptionPasswordAttemptAsync(
    QWidget *parent,
    PasswordSource prompt,
    Encryption::SecureArray wrappedDEK,
    Encryption::Salt kekSalt,
    int attempts,
    std::function<void(const Encryption::EncryptionKey &key, bool passwordEnteredManually)> callback)
{
    getPasswordAsync(
        parent,
        QObject::tr("Current Tab Encryption Password"),
        attempts == 0
            ? QObject::tr("Enter current password for encrypting tab data:")
            : QObject::tr("Invalid password. Please try again (%1/%2):")
                  .arg(attempts + 1)
                  .arg(maxPasswordAttempts),
        [=](const Encryption::SecureArray &password, bool ok) mutable {
            if (!ok || password.isEmpty()) {
                log("Tab encryption password required but not provided", LogWarning);
                callback({}, true);
                return;
            }

            const Encryption::EncryptionKey key(password, wrappedDEK, kekSalt);
            if (!key.isValid()) {
                if (attempts >= maxPasswordAttempts - 1) {
                    log("Maximum password attempts exceeded", LogError);
                    QMessageBox::critical(
                        parent,
                        QObject::tr("Password Verification Failed"),
                        QObject::tr("Maximum password attempts exceeded."));
                    callback({}, true);
                    return;
                }
                promptForEncryptionPasswordAttemptAsync(
                    parent, prompt, std::move(wrappedDEK), std::move(kekSalt), attempts + 1, std::move(callback));
                return;
            }

            if (prompt == PasswordSource::UseEnvAndKeychain)
                savePasswordToKeychain(password);

            callback(key, true);
        });
}

Encryption::SecureArray promptForNewDefaultPassword(QWidget *parent)
{
    return promptForNewPassword(
        QObject::tr("New Tab Encryption Password"),
        QObject::tr("Enter new password:"),
        PasswordPromptType::OnlyNonEmpty,
        parent);
}

void promptForNewDefaultPasswordAsync(
    QWidget *parent,
    std::function<void(const Encryption::SecureArray &password)> callback)
{
    activateWindow(parent);
    promptForNewDefaultPasswordAttemptAsync(parent, 0, std::move(callback));
}

void firstPasswordSetupAsync(
    QWidget *parent,
    PasswordSource prompt,
    std::function<void(const Encryption::EncryptionKey &key, bool passwordEnteredManually)> callback)
{
    qCInfo(logCategory) << "Setting up encryption for the first time";

    const Encryption::SecureArray storedPassword = getStoredPassword(prompt);
    if (!storedPassword.isEmpty()) {
        callback(setUpPassword(storedPassword, prompt), false);
        return;
    }

    promptForNewDefaultPasswordAsync(parent, [prompt, callback = std::move(callback)](const Encryption::SecureArray &newPassword) mutable {
        if (newPassword.isEmpty()) {
            callback({}, true);
            return;
        }
        callback(setUpPassword(newPassword, prompt), true);
    });
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
    while (attempts < maxPasswordAttempts) {
        bool ok;
        const auto password1 = getPassword(
            parent,
            title,
            attempts == 0
                ? label
                : QObject::tr("Passwords did not match. Please try again (%1/%2):").arg(attempts + 1).arg(maxPasswordAttempts),
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

void promptForEncryptionPasswordAsync(
    QWidget *parent,
    PasswordSource prompt,
    std::function<void(const Encryption::EncryptionKey &key, bool passwordEnteredManually)> callback)
{
    if ( !Encryption::initialize() ) {
        log("Failed to initialize encryption system", LogWarning);
        callback({}, true);
        return;
    }

    const Encryption::SecureArray wrappedDEK = Encryption::loadWrappedDEK();
    const Encryption::Salt kekSalt = Encryption::loadKEKSalt();

    const bool isFirstSetup = wrappedDEK.isEmpty() && kekSalt.isEmpty();
    if (isFirstSetup) {
        firstPasswordSetupAsync(parent, prompt, std::move(callback));
        return;
    }

    if (wrappedDEK.isEmpty() || kekSalt.isEmpty()) {
        log("Encryption key files are missing or corrupted; refusing to unlock encrypted tabs in strict mode", LogError);
        QMessageBox::critical(
            parent,
            QObject::tr("Encryption Files Corrupted"),
            QObject::tr(
                "Encryption files are missing or corrupted. "
                "Strict mode cannot recover encrypted tabs automatically."));
        callback({}, true);
        return;
    }

    const Encryption::SecureArray storedPassword = getStoredPassword(prompt);
    if (!storedPassword.isEmpty()) {
        const Encryption::EncryptionKey key(storedPassword, wrappedDEK, kekSalt);
        if (key.isValid()) {
            callback(key, false);
            return;
        }
        log("Loaded password does not unlock wrapped key", LogWarning);
    }

    activateWindow(parent);
    promptForEncryptionPasswordAttemptAsync(
        parent, prompt, wrappedDEK, kekSalt, 0, std::move(callback));
}

Encryption::EncryptionKey promptForEncryptionPasswordChange(
    const Encryption::EncryptionKey &currentKey, QWidget *parent)
{
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
        ClipboardModel model;

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

void promptForEncryptionPasswordAsync(
    QWidget *,
    PasswordSource,
    std::function<void(const Encryption::EncryptionKey &key, bool passwordEnteredManually)> callback)
{
    callback({}, true);
}

Encryption::EncryptionKey promptForEncryptionPasswordChange(
    const Encryption::EncryptionKey &, QWidget *)
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
