// SPDX-License-Identifier: GPL-3.0-or-later

#include "process.h"

#include <QProcess>

void terminateProcess(QProcess *p)
{
    if (p->state() == QProcess::NotRunning)
        return;

    p->terminate();
    if ( p->state() != QProcess::NotRunning && !p->waitForFinished(5000) ) {
        p->kill();
        p->waitForFinished(5000);
    }
}
