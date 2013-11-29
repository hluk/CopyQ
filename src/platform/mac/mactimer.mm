/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#include "mactimer.h"

#include <common/common.h>

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

    m_nsTimer = (void *) timer;

    // Set the tolerance using an NSTimer as there is no CF way of doing so
    NSTimer *nsTimer = (NSTimer *)timer;
    if ([nsTimer respondsToSelector:@selector(setTolerance:)]) {
        double toleranceSeconds = (double)m_tolerance / 1000.0;
        [nsTimer setTolerance:toleranceSeconds];
    }

    // add timer to the hidden run loop
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopDefaultMode);
}
