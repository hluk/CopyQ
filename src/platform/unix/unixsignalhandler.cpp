// SPDX-License-Identifier: GPL-3.0-or-later

#include "unixsignalhandler.h"

#include "common/log.h"

#include <QCoreApplication>
#include <QSocketNotifier>

#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

namespace {

namespace SignalAction {
enum SignalAction { Write, Read, Count };
}

int signalFd[SignalAction::Count];
QSocketNotifier *signalFdNotifier = nullptr;

struct SignalData {
    qint64 pid;
    int code;
};

/**
 * Catch Unix signal.
 *
 * Since this can be called at any time, Qt code cannot be handled here. For example,
 * this can be called from QString constructor which can easily deadlock the application on
 * a mutex when trying to create new QString from this handler. Also note that creating
 * QSettings recursively can result in resetting application settings.
 */
void exitSignalHandler(int code)
{
    const SignalData data{QCoreApplication::applicationPid(), code};
    const auto written = ::write(signalFd[SignalAction::Write], &data, sizeof(data));
    if (written == -1)
        log("Failed to write a Unix signal data", LogError);
}

void handleSignal()
{
    signalFdNotifier->setEnabled(false);

    SignalData data;

    if ( ::read(signalFd[SignalAction::Read], &data, sizeof(data)) != sizeof(data) ) {
        log("Failed to read a Unix signal data", LogError);
    } else if (data.pid != QCoreApplication::applicationPid()) {
        log("PID not matching on a Unix signal", LogError);
    } else {
        log( QStringLiteral("Terminating application on signal %1").arg(data.code) );
        QCoreApplication::exit(128 + data.code);
    }

    signalFdNotifier->setEnabled(true);
}

} // namespace

bool initUnixSignalHandler()
{
    // Safely quit application on TERM and HUP signals.
    struct sigaction sigact{};

    sigact.sa_handler = exitSignalHandler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigact.sa_flags |= SA_RESTART;

    if ( sigaction(SIGINT, &sigact, nullptr) > 0
         || sigaction(SIGTERM, &sigact, nullptr) > 0 )
    {
        log("sigaction() failed!", LogError);
        return false;
    }

    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, signalFd)) {
        log("socketpair() failed!", LogError);
        return false;
    }

    return true;
}

void startUnixSignalHandler()
{
    signalFdNotifier = new QSocketNotifier(signalFd[SignalAction::Read], QSocketNotifier::Read);
    QObject::connect(signalFdNotifier, &QSocketNotifier::activated, handleSignal);
}
