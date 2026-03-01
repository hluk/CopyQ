// SPDX-License-Identifier: GPL-3.0-or-later

#include "passwordprompt.h"

#include <QEventLoop>
#include <QLoggingCategory>
#include <QPointer>
#include <QWidget>

namespace {

Q_DECLARE_LOGGING_CATEGORY(logCategory)
Q_LOGGING_CATEGORY(logCategory, "copyq.passwordprompt")

} // namespace

PasswordPrompt::PasswordPrompt(QWidget *parent)
    : QObject(parent)
    , m_parent(parent)
{
    m_lastSuccessfulPromptTime = std::chrono::steady_clock::now();
}

void PasswordPrompt::prompt(PasswordSource source, Callback callback)
{
    bool done = false;
    QEventLoop waitLoop;

    m_pendingPrompts.append(PendingPrompt{
        source,
        [callback = std::move(callback), &done, &waitLoop](const Encryption::EncryptionKey &key) mutable {
            callback(key);
            done = true;
            waitLoop.quit();
        }
    });

    runPromptQueue();
    if (!done)
        waitLoop.exec();
}

Encryption::EncryptionKey PasswordPrompt::prompt(PasswordSource source)
{
    Encryption::EncryptionKey key;
    prompt(source, [&](const Encryption::EncryptionKey &newKey) { key = newKey; });
    return key;
}

void PasswordPrompt::runPromptQueue()
{
    if (m_promptInProgress || m_pendingPrompts.isEmpty())
        return;

    m_promptInProgress = true;
    const PasswordSource source = m_pendingPrompts.first().source;
    qCDebug(logCategory) << "Prompting for password," << (
            source == PasswordSource::IgnoreEnvAndKeychain ? "ignoring env and keychain"
            : source == PasswordSource::UseEnvOnly ? "including env"
            : "including env and keychain"
    );
    QPointer<PasswordPrompt> self(this);
    promptForEncryptionPasswordAsync(
        m_parent,
        source,
        [self](const Encryption::EncryptionKey &key, bool passwordEnteredManually) {
            if (!self)
                return;

            qCDebug(logCategory) << "Prompted password"
                << (passwordEnteredManually ? "from user" : "from env/keychain")
                << (key.isValid() ? "with success" : "without success");

            self->m_lastPromptKey = key;
            if (key.isValid() && passwordEnteredManually) {
                self->m_lastSuccessfulPromptTime = std::chrono::steady_clock::now();
            }

            if (!self->m_pendingPrompts.isEmpty()) {
                PendingPrompt pendingPrompt = std::move(self->m_pendingPrompts.first());
                self->m_pendingPrompts.removeFirst();
                pendingPrompt.callback(self->m_lastPromptKey);
            }

            while (!self->m_pendingPrompts.isEmpty()) {
                if (self->m_pendingPrompts.first().source == PasswordSource::IgnoreEnvAndKeychain
                        && !passwordEnteredManually)
                {
                    break;
                }
                PendingPrompt pendingPrompt = std::move(self->m_pendingPrompts.first());
                self->m_pendingPrompts.removeFirst();
                pendingPrompt.callback(self->m_lastPromptKey);
            }

            self->m_promptInProgress = false;
            self->runPromptQueue();
        });
}
