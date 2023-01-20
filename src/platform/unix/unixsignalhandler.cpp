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

/**
 * Catch Unix signal.
 *
 * Since this can be called at any time, Qt code cannot be handled here. For example,
 * this can be called from QString constructor which can easily deadlock the application on
 * a mutex when trying to create new QString from this handler. Also note that creating
 * QSettings recursively can result in resetting application settings.
 */
void exitSignalHandler(int)
{
    const qint64 pid = QCoreApplication::applicationPid();
    const auto written = ::write(signalFd[SignalAction::Write], &pid, sizeof(pid));
    if (written == -1)
        log("Failed to handle signal!", LogError);
}

void handleSignal()
{
    signalFdNotifier->setEnabled(false);

    qint64 pid;
    if ( ::read(signalFd[SignalAction::Read], &pid, sizeof(pid)) != sizeof(pid) ) {
        COPYQ_LOG("Incorrect number of bytes read from Unix signal socket!");
        signalFdNotifier->setEnabled(true);
    } else if (pid != QCoreApplication::applicationPid()) {
        COPYQ_LOG("Wrong PID written to Unix signal socket!");
        signalFdNotifier->setEnabled(true);
    } else {
        COPYQ_LOG("Terminating application on signal.");
        QCoreApplication::exit();
    }
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
