// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

struct ClipboardBrowserShared;
class QWidget;
class QAbstractItemModel;
class QIODevice;
class ItemFactory;
class QProgressDialog;
class QSqlDatabase;

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

QSqlDatabase promptForDbPassword(QWidget *parent, PasswordSource prompt, const QString &dbName);
QString promptForNewDbPassword(QWidget *parent, PasswordSource prompt);
void promptForEncryptionPasswordChange(QWidget *parent);
void removePasswordFromKeychain();
