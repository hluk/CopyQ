// SPDX-License-Identifier: GPL-3.0-or-later

#include "macactivity.h"

#include "common/log.h"

#include <QString>

#include <Cocoa/Cocoa.h>

MacActivity::MacActivity(const QString &reason)
    : m_activity(0)
{
    const NSActivityOptions options = NSActivityBackground;;
    id act = [[NSProcessInfo processInfo]
        beginActivityWithOptions:options
        reason:reason.toNSString()];
    if (act) {
        m_activity = reinterpret_cast<void*>(act);
        COPYQ_LOG_VERBOSE(QString("Started Background activity for: %1").arg(reason));
    } else {
        ::log("Failed to create activity", LogWarning);
    }
}

MacActivity::~MacActivity() {
    id act = reinterpret_cast<id>(m_activity);
    if (act) {
        [[NSProcessInfo processInfo] endActivity:act];
        COPYQ_LOG_VERBOSE("Ended activity");
    } else {
        ::log("Failed to stop activity", LogWarning);
    }
}
