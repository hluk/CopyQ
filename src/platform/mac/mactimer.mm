// SPDX-License-Identifier: GPL-3.0-or-later

#include "mactimer.h"

#include <Cocoa/Cocoa.h>
#include <QPointer>

MacTimer::MacTimer(QObject *parent) :
    QObject(parent),
    m_interval(0),
    m_tolerance(0),
    m_singleShot(false),
    m_nsTimer(0)
{
}

MacTimer::~MacTimer()
{
    stop();
}

void MacTimer::stop()
{
    if (m_nsTimer) {
        CFRunLoopTimerInvalidate((CFRunLoopTimerRef) m_nsTimer);
    }
    m_nsTimer = 0;
}

void MacTimer::restart()
{
    if (m_nsTimer) {
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
    double intervalSeconds = (float)m_interval / 1000.0;

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

    // Set the tolerance using an NSTimer as there is no CF way of doing so
    if ([m_nsTimer respondsToSelector:@selector(setTolerance:)]) {
        double toleranceSeconds = (double)m_tolerance / 1000.0;
        [m_nsTimer setTolerance:toleranceSeconds];
    }

    // add timer to the hidden run loop
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopDefaultMode);
}
