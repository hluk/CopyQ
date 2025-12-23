// SPDX-License-Identifier: GPL-3.0-or-later

#include "encryptionpassword.h"

#include "common/common.h"
#include "common/config.h"
#include "common/keychainaccess.h"
#include "common/log.h"
#include "common/textdata.h"
#include "item/clipboardmodel.h"
#include "item/db.h"
#include "item/itemfactory.h"
#include "item/itemstore.h"
#include "item/itemwidget.h"

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

const QString dbNameTemporary = QStringLiteral("-tmp");

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

bool savePasswordToKeychain(const QString &password)
{
    qCInfo(logCategory) << "Saving password from system keychain";
    QByteArray ba = password.toUtf8();
    const bool result = KeychainAccess::writePassword(
        keychainServiceName(), keychainPasswordKey(), ba);
    ba.fill('\0');

    if (!result)
        return false;

    qCInfo(logCategory) << "Password saved to system keychain";
    return true;
}

QString loadPasswordFromKeychain()
{
    qCInfo(logCategory) << "Loading password from system keychain";
    const QByteArray result = KeychainAccess::readPassword(
        keychainServiceName(), keychainPasswordKey());

    if (result.isEmpty())
        return {};

    qCDebug(logCategory) << "Password loaded from system keychain";
    return QString::fromUtf8(result);
}

void activateWindow(QWidget *parent)
{
    parent->show();
    parent->activateWindow();
    parent->raise();
}

QString getStoredPassword(PasswordSource prompt)
{
    if (prompt == PasswordSource::UseEnvAndKeychain || prompt == PasswordSource::UseEnvOnly) {
        const QByteArray ba = qgetenv("COPYQ_PASSWORD");
        if ( !ba.isEmpty() ) {
            qCInfo(logCategory) << "Encryption password loaded from env variable COPYQ_PASSWORD";
            return QString::fromUtf8(ba);
        }
    }

    if (prompt == PasswordSource::UseEnvAndKeychain) {
        const QString passwd = loadPasswordFromKeychain();
        if ( !passwd.isEmpty() ) {
            qCInfo(logCategory) << "Encryption password loaded from system keychain";
            return passwd;
        }
    }

    return {};
}

QString getPassword(
    QWidget *parent, const QString &title, const QString &label, bool *ok = nullptr)
{
    return QInputDialog::getText(
        parent, title, label, QLineEdit::Password, QString(), ok);
}

QString promptForNewPassword(
    const QString &title, const QString &label, PasswordPromptType pwType, QWidget *parent)
{
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

QString promptForNewDefaultPassword(QWidget *parent)
{
    return promptForNewPassword(
        QObject::tr("New Tab Encryption Password"),
        QObject::tr("Enter new password:"),
        PasswordPromptType::OnlyNonEmpty,
        parent);
}

} // namespace

QSqlDatabase promptForDbPassword(QWidget *parent, PasswordSource prompt, const QString &dbName)
{
    const QString storedPassword = getStoredPassword(prompt);
    if ( !storedPassword.isEmpty() ) {
        QSqlDatabase db = openDb(storedPassword, dbName);
        if (!db.isOpen())
            return db;
    }

    activateWindow(parent);
    int attempts = 0;
    const int maxAttempts = 3;
    while (attempts < maxAttempts) {
        bool ok;
        const QString password = getPassword(
            parent,
            QObject::tr("Current Tab Encryption Password"),
            attempts == 0
                ? QObject::tr("Enter current password for encrypting tab data:")
                : QObject::tr("Invalid password. Please try again (%1/%2):").arg(attempts + 1).arg(maxAttempts),
            &ok
        );

        if (!ok)
            return {};

        QSqlDatabase db = openDb(password, dbName);
        if (!db.isOpen())
            continue;

        if (prompt == PasswordSource::UseEnvAndKeychain)
            savePasswordToKeychain(password);

        return db;
    }

    log("Maximum password attempts exceeded", LogError);
    QMessageBox::critical(
        parent,
        QObject::tr("Password Verification Failed"),
        QObject::tr("Maximum password attempts exceeded. Encryption will not be available.")
    );
    return {};
}

QString promptForNewDbPassword(QWidget *parent, PasswordSource prompt)
{
    const QString newPassword = promptForNewDefaultPassword(parent);
    if (prompt == PasswordSource::UseEnvAndKeychain)
        savePasswordToKeychain(newPassword);
    return newPassword;
}

void promptForEncryptionPasswordChange(QWidget *parent)
{
    auto tmpDb = promptForDbPassword(parent, PasswordSource::IgnoreEnvAndKeychain, dbNameTemporary);
    if (!tmpDb.isOpen())
        return;

    const QString newPassword = promptForNewDefaultPassword(parent);
    if (newPassword.isEmpty())
        return;

    log("Attempting to change password...", LogNote);

    if (!setDbPassword(tmpDb, newPassword)) {
        log("Password change failed", LogError);
        QMessageBox::critical(
            parent,
            QObject::tr("Change Password Failed"),
            QObject::tr("Failed to change password. Your old password is still active. Please check the logs for details.")
        );
        return;
    }

    log("Password changed successfully", LogNote);
    QMessageBox::information(
        parent,
        QObject::tr("Change Password Successful"),
        QObject::tr("Password has been changed successfully.")
    );
}

void removePasswordFromKeychain()
{
    KeychainAccess::deletePassword(keychainServiceName(), keychainPasswordKey());
}
