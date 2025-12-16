// SPDX-License-Identifier: GPL-3.0-or-later

#include "keychainaccess.h"

#ifdef WITH_KEYCHAIN
#   if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#       include <qt6keychain/keychain.h>
#   else
#       include <qt5keychain/keychain.h>
#   endif
#include <QCoreApplication>
#include <QEventLoop>
#include <QLoggingCategory>
#include <QTimer>

namespace {

constexpr int keychainTimeoutMs = 30000;

Q_DECLARE_LOGGING_CATEGORY(logCategory)
Q_LOGGING_CATEGORY(logCategory, "copyq.keychain")

bool waitOrTimeout(QKeychain::Job &job)
{
    QEventLoop loop;
    bool finished = false;
    QObject::connect(&job, &QKeychain::Job::finished, &loop, [&](){
        finished = true;
        loop.quit();
    });

    // Set a timeout to prevent hanging indefinitely
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(keychainTimeoutMs);

    job.start();
    loop.exec();

    return finished;
}

} // namespace

namespace KeychainAccess {

QByteArray readPassword(const QString &service, const QString &key)
{
    QKeychain::ReadPasswordJob job(service);
    job.setAutoDelete(false);
    job.setKey(key);

    if (!waitOrTimeout(job)) {
        qCWarning(logCategory) << "Reading keychain timed out";
        return {};
    }

    if (job.error()) {
        qCWarning(logCategory) << "Reading keychain failed:" << job.errorString();
        return {};
    }

    return job.binaryData();
}

bool writePassword(const QString &service, const QString &key, const QByteArray &password)
{
    QKeychain::WritePasswordJob job(service);
    job.setAutoDelete(false);
    job.setKey(key);
    job.setBinaryData(password);

    if (!waitOrTimeout(job)) {
        qCWarning(logCategory) << "Writing keychain timed out";
        return false;
    }

    if (job.error()) {
        qCWarning(logCategory) << "Writing keychain failed:" << job.errorString();
        return false;
    }

    return true;
}

void deletePassword(const QString &service, const QString &key)
{
    const auto job = new QKeychain::DeletePasswordJob(service);
    job->setAutoDelete(true);
    job->setKey(key);
    QObject::connect(job, &QKeychain::Job::finished, [](QKeychain::Job *job) {
        const auto error = job->error();
        if (error != QKeychain::NoError && error != QKeychain::EntryNotFound)
            qCWarning(logCategory) << "Deleting keychain failed:" << job->errorString();
    });
    job->start();
}

} // namespace KeychainAccess

#else // !WITH_KEYCHAIN

// Stub implementations when QtKeychain is not available
namespace KeychainAccess {

QByteArray readPassword(const QString &, const QString &)
{
    return {};
}

bool writePassword(const QString &, const QString &, const QByteArray &)
{
    return false;
}

bool deletePassword(const QString &, const QString &)
{
    return false;
}

} // namespace KeychainAccess

#endif // WITH_KEYCHAIN
