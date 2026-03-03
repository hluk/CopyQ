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

    std::chrono::steady_clock::time_point lastSuccessfulPasswordPromptTime() const
    { return m_lastSuccessfulPromptTime; }

private:
    struct PendingPrompt {
        PasswordSource source;
        Callback callback;
    };

    void runPromptQueue();

    QWidget *m_parent = nullptr;
    bool m_promptInProgress = false;
    Encryption::EncryptionKey m_lastPromptKey;
    std::chrono::steady_clock::time_point m_lastSuccessfulPromptTime;
    QList<PendingPrompt> m_pendingPrompts;
};
