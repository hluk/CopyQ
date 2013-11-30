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

#include "macplatform.h"

#include <common/common.h>

#include <QMutex>
#include <QMutexLocker>

#include <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

void * MacPlatform::m_currentPasteWindow = 0;

namespace {
    QMutex mutex(QMutex::Recursive);

    template<typename T> inline T* objc_cast(id from)
    {
        if ([from isKindOfClass:[T class]]) {
            return static_cast<T*>(from);
        }
        return nil;
    }

    void raisePasteWindow(WId wid)
    {
        if (wid == 0L)
            return;

        NSRunningApplication *runningApplication = objc_cast<NSRunningApplication>((id) wid);
        if (runningApplication) {
            // Shouldn't need to unhide since we should have just been in this
            // application..
            //[runningApplication unhide];

            // NSApplicationActivateAllWindows
            // By default, activation brings only the main and key windows
            // forward. If you specify NSApplicationActivateAllWindows, all
            // of the application's windows are brought forward.

            // NSApplicationActivateIgnoringOtherApps
            // By default, activation deactivates the calling app (assuming it
            // was active), and then the new app is activated only if there is
            // no currently active application

            //[runningApplication activateWithOptions:NSApplicationActivateAllWindows];
            [runningApplication activateWithOptions:0];
        } else {
            ::log("wid is null...", LogNote);
        }
    }

    void sendCommandV() {
        CGEventSourceRef sourceRef = CGEventSourceCreate(
            kCGEventSourceStateCombinedSessionState);

        CGEventRef commandDown = CGEventCreateKeyboardEvent(sourceRef, kVK_Command, YES);
        CGEventRef VDown = CGEventCreateKeyboardEvent(sourceRef, kVK_ANSI_V, YES);

        CGEventRef VUp = CGEventCreateKeyboardEvent(sourceRef, kVK_ANSI_V, NO);
        CGEventRef commandUp = CGEventCreateKeyboardEvent(sourceRef, kVK_Command, NO);

        // 0x000008 is a hack to fix pasting in Emacs?
        // https://github.com/TermiT/Flycut/pull/18
        CGEventSetFlags(VDown,kCGEventFlagMaskCommand|0x000008);
        CGEventSetFlags(VUp,kCGEventFlagMaskCommand|0x000008);

        CGEventPost(kCGHIDEventTap, commandDown);
        CGEventPost(kCGHIDEventTap, VDown);
        CGEventPost(kCGHIDEventTap, VUp);
        CGEventPost(kCGHIDEventTap, commandUp);

        CFRelease(commandDown);
        CFRelease(VDown);
        CFRelease(VUp);
        CFRelease(commandUp);
        CFRelease(sourceRef);
    }
} // namespace

PlatformPtr createPlatformNativeInterface()
{
    return PlatformPtr(new MacPlatform);
}

MacPlatform::MacPlatform()
{
}

WId MacPlatform::getCurrentWindow()
{
    // NOTE: this is actually currently just the foreground *application*
    NSRunningApplication *runningApp = [[NSWorkspace sharedWorkspace] frontmostApplication];
    WId wid = reinterpret_cast<WId>(runningApp);
    return wid;
}

QString MacPlatform::getWindowTitle(WId wid)
{
    if (wid == 0L)
        return QString();

    NSRunningApplication *runningApplication = objc_cast<NSRunningApplication>((id)wid);
    QString result;
    if (runningApplication) {
        result = QString::fromUtf8([[runningApplication localizedName] UTF8String]);
    } else {
        ::log("Failed to get window title.", LogWarning);
    }

    return result;
}

void MacPlatform::raiseWindow(WId wid)
{
    if (wid == 0L)
        return;

    QMutexLocker lock(&mutex);

    NSView *view = objc_cast<NSView>((id)wid);
    if (((WId) m_currentPasteWindow) == wid) {
        COPYQ_LOG(QString("Raise paste window. wid = %1").arg(wid));
        raisePasteWindow((WId) wid);
    } else if (view) {
        COPYQ_LOG(QString("Raise CopyQ. wid = %1").arg(wid));
        [NSApp activateIgnoringOtherApps:YES];

        NSWindow *window = [view window];
        [window makeKeyAndOrderFront:nil];
    } else {
        ::log(QString("Tried to raise unknown window. wid = %1").arg(wid), LogWarning);
    }
}

void MacPlatform::pasteToWindow(WId wid)
{
    if (wid == 0L)
        return;

    // Window MUST be raised, otherwise we can't send events to it
    raisePasteWindow(wid);

    // Wait 0.15 seconds for the window to finish gaining focus
    usleep(150000);

    // Send Command-V
    sendCommandV();

    // Wait 0.15 seconds for the event to finish being sent to the paste window
    usleep(150000);
}

WId MacPlatform::getPasteWindow()
{
    WId current = getCurrentWindow();

    QMutexLocker lock(&mutex);

    if (((WId) m_currentPasteWindow) != current) {
        if (m_currentPasteWindow) {
            NSObject *obj = objc_cast<NSObject>((id)m_currentPasteWindow);
            if (obj) {
                [obj release];
            } else {
                ::log("Unable to release paste window for some reason!", LogWarning);
            }
        }

        if (current) {
            NSObject *newObj = objc_cast<NSObject>((id)current);
            if (newObj) {
                [newObj retain];
            } else {
                ::log("Unable to retain paste window for some reason!", LogWarning);
            }
        }
    }

    m_currentPasteWindow = (void *) current;

    return (WId) m_currentPasteWindow;
}

long int MacPlatform::getChangeCount()
{
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSInteger changeCount = [pasteboard changeCount];
    return changeCount;
}
