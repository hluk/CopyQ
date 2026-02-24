// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/encryption.h"
#include "gui/encryptionpassword.h"

#include <QElapsedTimer>
#include <QList>
#include <QObject>

#include <functional>

class QWidget;

class PasswordPrompt final : public QObject
{
public:
    explicit PasswordPrompt(QWidget *parent);

    using Callback = std::function<void(const Encryption::EncryptionKey &)>;

    void prompt(PasswordSource source, Callback callback);
    Encryption::EncryptionKey prompt(PasswordSource source);

    qint64 elapsedMsSinceLastSuccessfulPasswordPrompt() const;

private:
    struct PendingPrompt {
        PasswordSource source;
        Callback callback;
    };

    void runPromptQueue();

    QWidget *m_parent = nullptr;
    bool m_promptInProgress = false;
    Encryption::EncryptionKey m_lastPromptKey;
    QElapsedTimer m_lastSuccessfulPromptElapsed;
    QList<PendingPrompt> m_pendingPrompts;
};
