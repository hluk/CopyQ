// SPDX-License-Identifier: GPL-3.0-or-later

#include "macplatformwindow.h"

#include "common/appconfig.h"
#include "common/log.h"
#include "platform/platformcommon.h"

#include <AppKit/NSGraphics.h>
#include <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>

#include <QApplication>
#include <QSet>

namespace {
    // Original from: https://stackoverflow.com/a/33584460/454171
    NSString* keyCodeToString(CGKeyCode keyCode, const UCKeyboardLayout *keyboardLayout)
    {
        UInt32 deadKeyState = 0;
        UniCharCount maxStringLength = 255;
        UniCharCount actualStringLength = 0;
        UniChar unicodeString[maxStringLength];

        OSStatus status = UCKeyTranslate(
            keyboardLayout,
            keyCode, kUCKeyActionDown, 0,
            LMGetKbdType(), 0,
            &deadKeyState,
            maxStringLength,
            &actualStringLength, unicodeString);

        if (actualStringLength == 0 && deadKeyState) {
            status = UCKeyTranslate(
                keyboardLayout,
                kVK_Space, kUCKeyActionDown, 0,
                LMGetKbdType(), 0,
                &deadKeyState,
                maxStringLength,
                &actualStringLength, unicodeString);
        }

        if (actualStringLength > 0 && status == noErr) {
            return [[NSString stringWithCharacters:unicodeString
                    length:(NSUInteger)actualStringLength] lowercaseString];
        }

        return nil;
    }

    NSNumber* charToKeyCode(const char c)
    {
        TISInputSourceRef currentKeyboard = TISCopyCurrentKeyboardLayoutInputSource();
        CFDataRef layoutData = (CFDataRef)TISGetInputSourceProperty(
            currentKeyboard, kTISPropertyUnicodeKeyLayoutData);

        if (layoutData != nil) {
            const UCKeyboardLayout *keyboardLayout =
                (const UCKeyboardLayout*)CFDataGetBytePtr(layoutData);

            COPYQ_LOG( QStringLiteral("Searching key code for '%1'").arg(c) );
            NSString *keyChar = [NSString stringWithFormat:@"%c" , c];
            for (size_t i = 0; i < 128; ++i) {
                NSString* str = keyCodeToString((CGKeyCode)i, keyboardLayout);
                if (str != nil && [str isEqualToString:keyChar]) {
                    COPYQ_LOG( QStringLiteral("KeyCode for '%1' is %2").arg(c).arg(i) );
                    CFRelease(currentKeyboard);
                    return [NSNumber numberWithInt:i];
                }
            }
        }

        CFRelease(currentKeyboard);
        return nil;
    }

    CGKeyCode keyCodeFromChar(const char c, CGKeyCode fallback)
    {
        const auto keyCode = charToKeyCode(c);
        return keyCode == nil
            ? fallback
            : (CGKeyCode)[keyCode intValue];
    }

    template<typename T> inline T* objc_cast(id from)
    {
        if (from && [from isKindOfClass:[T class]]) {
            return static_cast<T*>(from);
        }
        return nil;
    }

    void sendShortcut(int modifier, int key) {
        COPYQ_LOG( QStringLiteral("Sending key codes %1 and %2")
                   .arg(modifier)
                   .arg(key) );

        CGEventSourceRef sourceRef = CGEventSourceCreate(
            kCGEventSourceStateCombinedSessionState);

        CGEventRef commandDown = CGEventCreateKeyboardEvent(sourceRef, modifier, YES);
        CGEventRef VDown = CGEventCreateKeyboardEvent(sourceRef, key, YES);

        CGEventRef VUp = CGEventCreateKeyboardEvent(sourceRef, key, NO);
        CGEventRef commandUp = CGEventCreateKeyboardEvent(sourceRef, modifier, NO);

        // 0x000008 is a hack to fix pasting in Emacs?
        // https://github.com/TermiT/Flycut/pull/18
        CGEventSetFlags(VDown,CGEventFlags(kCGEventFlagMaskCommand|0x000008));
        CGEventSetFlags(VUp,CGEventFlags(kCGEventFlagMaskCommand|0x000008));

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

    /**
     * Delays a sending Command+key operation for delayInMS, and retries up to 'tries' times.
     *
     * This function is necessary in order to check that the intended window has come
     * to the foreground. The "isActive" property only changes when the app gets back
     * to the "run loop", so we can't block and check it.
     */
    void delayedSendShortcut(int modifier, int key, int64_t delayInMS, uint tries, NSWindow *window) {
        COPYQ_LOG( QStringLiteral("Sending key codes %1 and %2 with delay %3ms (%4 retries to go)")
                   .arg(modifier)
                   .arg(key)
                   .arg(delayInMS)
                   .arg(tries) );

        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, delayInMS * NSEC_PER_MSEC), dispatch_get_main_queue(), ^(void){
            if (window && ![window isKeyWindow]) {
                if (tries > 0) {
                    delayedSendShortcut(modifier, key, delayInMS, tries - 1, window);
                } else {
                    log("Failed to raise application, will not paste.", LogWarning);
                }
            } else {
                sendShortcut(modifier, key);
            }
        });
    }

    pid_t getPidForWid(WId find_wid) {
        // Build a set of "normal" windows. This is necessary as "NSWindowList" gets things like the
        // menubar (which can be "owned" by various apps).
        NSArray *array = (__bridge NSArray*) CGWindowListCopyWindowInfo(kCGWindowListOptionAll | kCGWindowListExcludeDesktopElements, kCGNullWindowID);
        for (NSDictionary* dict in array) {
            long int pid = [(NSNumber*)[dict objectForKey:@"kCGWindowOwnerPID"] longValue];
            unsigned long int wid = (unsigned long) [(NSNumber*)[dict objectForKey:@"kCGWindowNumber"] longValue];

            if (wid == find_wid) {
                CFRelease(array);
                return pid;
            }
        }
        CFRelease(array);

        return 0;
    }


    long int getTopWindow(pid_t process_pid) {
        // Build a set of "normal" windows. This is necessary as "NSWindowList" gets things like the
        // menubar (which can be "owned" by various apps).
        QSet<long int> widsForProcess;
        NSArray *array = (__bridge NSArray*) CGWindowListCopyWindowInfo(kCGWindowListOptionAll | kCGWindowListExcludeDesktopElements, kCGNullWindowID);
        for (NSDictionary* dict in array) {
            long int pid = [(NSNumber*)[dict objectForKey:@"kCGWindowOwnerPID"] longValue];
            long int wid = [(NSNumber*)[dict objectForKey:@"kCGWindowNumber"] longValue];
            long int layer = [(NSNumber*)[dict objectForKey:@"kCGWindowLayer"] longValue];

            if (pid == process_pid && layer == 0) {
                widsForProcess.insert(wid);
            }
        }
        CFRelease(array);

        // Now look through the windows in NSWindowList (which are ordered from front to back)
        // the first window in this list which is also in widsForProcess is our frontmost "normal" window
        long int wid = -1;
        for (NSNumber *window : [NSWindow windowNumbersWithOptions:0]) {
            const long int wid2 = [window longValue];
            if (widsForProcess.contains(wid2)) {
                wid = wid2;
                break;
            }
        }
        return wid;
    }

    QString getTitleFromWid(long int wid) {
        QString title;

        if (wid < 0) {
            return title;
        }

        uint32_t windowid[1] = {static_cast<uint32_t>(wid)};
        CFArrayRef windowArray = CFArrayCreate ( NULL, (const void **)windowid, 1 ,NULL);
        NSArray *array = (__bridge NSArray*) CGWindowListCreateDescriptionFromArray(windowArray);

        // Should only be one
        for (NSDictionary* dict in array) {
            title = QString::fromNSString([dict objectForKey:@"kCGWindowName"]);
        }

        CFRelease(array);
        CFRelease(windowArray);

        return title;
    }
} // namespace

MacPlatformWindow::MacPlatformWindow(NSRunningApplication *runningApp):
    m_windowNumber(-1)
    , m_window(0)
    , m_runningApplication(0)
{
    if (runningApp) {
        m_runningApplication = runningApp;
        [runningApp retain];
        m_windowNumber = getTopWindow(runningApp.processIdentifier);
        COPYQ_LOG_VERBOSE("Created platform window for non-copyq");
    } else {
        log("Failed to convert runningApplication to application", LogWarning);
    }
}

MacPlatformWindow::MacPlatformWindow(WId wid):
    m_windowNumber(-1)
    , m_window(0)
    , m_runningApplication(0)
{
    // Try using wid as an actual window ID
    pid_t pid = getPidForWid(wid);
    if (pid != 0) {
        m_runningApplication = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
        m_windowNumber = wid;
        // This will return 'nil' unless this process owns the window
        m_window = [NSApp windowWithWindowNumber: wid];
    } else if (NSView *view = objc_cast<NSView>((id)wid)) {
        // If given a view, its ours
        m_runningApplication = [NSRunningApplication currentApplication];
        m_window = [view window];
        [m_runningApplication retain];
        [m_window retain];
        m_windowNumber = [m_window windowNumber];
        COPYQ_LOG_VERBOSE("Created platform window for copyq");
    } else {
        log("Failed to convert WId to window", LogWarning);
    }
}

MacPlatformWindow::MacPlatformWindow():
    m_windowNumber(-1)
    , m_window(0)
    , m_runningApplication(0)
{
}

MacPlatformWindow::~MacPlatformWindow() {
    // Releasing '0' or 'nil' is fine
    [m_runningApplication release];
    [m_window release];
}

QString MacPlatformWindow::getTitle()
{
    QString appTitle;
    if (m_runningApplication)
        appTitle = QString::fromNSString([m_runningApplication localizedName]);

    QString windowTitle;
    if (m_window)
        windowTitle = QString::fromNSString([m_window title]);
    if (windowTitle.isEmpty() && m_windowNumber >= 0)
        windowTitle = getTitleFromWid(m_windowNumber);

    // We have two separate titles, the application title (shown at the top
    // left in the menu bar and the window title (shown in the window bar).
    // Some apps put the app name into the window title as well, some don't. We
    // want to be able to match on the app name (e.g. Firefox, Safari, or
    // Terminal) when writing commands, so we'll ensure that the app name is at
    // the beginning of the returned window title.
    QString result;
    if (windowTitle.isEmpty()) {
        result = appTitle;
    } else if (appTitle.isEmpty() || windowTitle.startsWith(appTitle)) {
        result = windowTitle;
    } else {
        result = QString("%1 - %2").arg(appTitle, windowTitle);
    }
    return result;
}

void MacPlatformWindow::raise()
{
    if (m_window && m_runningApplication &&
            [m_runningApplication isEqual:[NSRunningApplication currentApplication]]) {
        COPYQ_LOG( QString("Raise own window \"%1\"").arg(getTitle()) );
        [NSApp activateIgnoringOtherApps:YES];
        [m_window makeKeyAndOrderFront:nil];
    } else if (m_runningApplication) {
        // Shouldn't need to unhide since we should have just been in this
        // application..
        //[m_runningApplication unhide];

        COPYQ_LOG( QString("Raise a window \"%1\"").arg(getTitle()) );
        [m_runningApplication activateWithOptions:NSApplicationActivateIgnoringOtherApps];
        if (m_windowNumber != -1) {
            auto window = [NSApp windowWithWindowNumber: m_windowNumber];
            [window makeKeyAndOrderFront:nil];
        }
    } else {
        ::log(QString("Tried to raise unknown window"), LogWarning);
    }
}

void MacPlatformWindow::pasteClipboard()
{
    if (!m_runningApplication) {
        log("Failed to paste to unknown window", LogWarning);
        return;
    }

    const AppConfig config;

    const auto keyCodeV = keyCodeFromChar('v', kVK_ANSI_V);
    if (m_window != nullptr) {
        // Window MUST be raised, otherwise we can't send events to it
        waitMs(config.option<Config::window_wait_before_raise_ms>());
        raise();
        waitMs(config.option<Config::window_wait_after_raised_ms>());

        // Paste after after a delay, try 5 times
        const int keyPressTimeMs = config.option<Config::window_key_press_time_ms>();
        delayedSendShortcut(kVK_Command, keyCodeV, keyPressTimeMs, 5, m_window);
    } else {
        sendShortcut(kVK_Command, keyCodeV);
    }
}

void MacPlatformWindow::copy()
{
    if (!m_runningApplication) {
        log("Failed to copy from unknown window", LogWarning);
        return;
    }

    const AppConfig config;

    // Window MUST be raised, otherwise we can't send events to it
    waitMs(config.option<Config::window_wait_before_raise_ms>());
    raise();
    waitMs(config.option<Config::window_wait_after_raised_ms>());

    // Copy after after a delay, try 5 times
    const int keyPressTimeMs = config.option<Config::window_key_press_time_ms>();
    const auto keyCodeC = keyCodeFromChar('c', kVK_ANSI_C);
    delayedSendShortcut(kVK_Command, keyCodeC, keyPressTimeMs, 5, m_window);
}
