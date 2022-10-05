// SPDX-License-Identifier: GPL-3.0-or-later

#include "applicationexceptionhandler.h"

#include "common/log.h"

#include <QCoreApplication>
#include <QFile>
#include <QMetaObject>

void logException(const char *what)
{
    if ( canUseStandardOutput() ) {
        QFile f;
        f.open(stderr, QIODevice::WriteOnly);
        f.write(what ? what : "Unknown exception");
        f.write("\n");
        f.close();
    }

    log( QString("Exception: ") + what, LogError );
}

void detail::ApplicationExceptionHandlerBase::exit(int exitCode)
{
    QMetaObject::invokeMethod(this, "exitSlot", Q_ARG(int, exitCode));
}

void detail::ApplicationExceptionHandlerBase::exitSlot(int exitCode)
{
    qApp->exit(exitCode);
}
