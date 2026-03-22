// SPDX-License-Identifier: GPL-3.0-or-later

#include "mactimer.h"

#include <QPointer>

MacTimer::MacTimer(QObject *parent) :
    QObject(parent),
    m_interval(0),
    m_tolerance(0),
    m_singleShot(false)
{
}

MacTimer::~MacTimer()
{
    stop();
}

void MacTimer::stop()
{
    if (m_cfTimer) {
        CFRunLoopTimerInvalidate(m_cfTimer);
        m_cfTimer = nullptr;
    }
}

void MacTimer::restart()
{
    if (m_cfTimer) {
        stop();
        start();
    }
}

void MacTimer::setInterval(int msec)
{
    if (msec != m_interval) {
        m_interval = msec;
        restart();
    }
}

void MacTimer::setTolerance(int msec)
{
    if (msec != m_tolerance) {
        m_tolerance = msec;
        restart();
    }
}

void MacTimer::setSingleShot(bool singleShot)
{
    if (singleShot != m_singleShot) {
        m_singleShot = singleShot;
        restart();
    }
}

void MacTimer::start() {
    stop();

    // Use QPointer to make it auto-null, and use a 'weak reference' in the
    // block
    QPointer<MacTimer> weakThis(this);
    double intervalSeconds = (double)m_interval / 1000.0;

    // Create a timer which kicks into a block
    CFRunLoopTimerRef timer = CFRunLoopTimerCreateWithHandler(NULL,
        CFAbsoluteTimeGetCurrent() + intervalSeconds,
        m_singleShot ? 0 : intervalSeconds,
        0, 0, // These are both ignored
        ^(CFRunLoopTimerRef) {
            if (weakThis) {
                emit weakThis->timeout();
            }
        }
    );

    if (!timer)
        return;

    // Set tolerance so macOS can coalesce timer firings for power efficiency.
    double toleranceSeconds = (double)m_tolerance / 1000.0;
    CFRunLoopTimerSetTolerance(timer, toleranceSeconds);

    // Add timer to the current run loop. The run loop retains its own
    // reference; CFRef adopts the Create-rule +1 for stop()/invalidate.
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopDefaultMode);
    m_cfTimer = timer;
}
