/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "unixsignalhandler.h"

#include "common/log.h"

#include <QCoreApplication>

#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

int UnixSignalHandler::signalFd[UnixSignalHandler::Count];

bool UnixSignalHandler::create(QObject *parent)
{
    static UnixSignalHandler *handler = NULL;

    if (handler)
        return true;

    // Safely quit application on TERM and HUP signals.
    struct sigaction sigact;

    sigact.sa_handler = UnixSignalHandler::exitSignalHandler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigact.sa_flags |= SA_RESTART;

    if ( sigaction(SIGHUP, &sigact, 0) > 0
         || sigaction(SIGINT, &sigact, 0) > 0
         || sigaction(SIGTERM, &sigact, 0) > 0 )
    {
        log("sigaction() failed!", LogError);
        return false;
    }

    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, signalFd)) {
        log("socketpair() failed!", LogError);
        return false;
    }

    new UnixSignalHandler(parent);
    return true;
}

void UnixSignalHandler::exitSignalHandler(int)
{
    const qint64 pid = QCoreApplication::applicationPid();
    ::write(signalFd[Write], &pid, sizeof(pid));
}

void UnixSignalHandler::handleSignal()
{
    m_signalFdNotifier.setEnabled(false);

    qint64 pid;
    if ( ::read(signalFd[Read], &pid, sizeof(pid)) != sizeof(pid) ) {
        COPYQ_LOG("Incorrect number of bytes read from Unix signal socket!");
        m_signalFdNotifier.setEnabled(true);
    } else if (pid != QCoreApplication::applicationPid()) {
        COPYQ_LOG("Wrong PID written to Unix signal socket!");
        m_signalFdNotifier.setEnabled(true);
    } else {
        COPYQ_LOG("Terminating application on signal.");
        QCoreApplication::exit();
    }
}

UnixSignalHandler::UnixSignalHandler(QObject *parent)
    : QObject(parent)
    , m_signalFdNotifier(signalFd[Read], QSocketNotifier::Read, this)
{
    connect(&m_signalFdNotifier, SIGNAL(activated(int)), this, SLOT(handleSignal()));
}
