/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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

#include "app/applicationexceptionhandler.h"
#include "common/log.h"
#include "copyqpasteboardmime.h"
#include "foregroundbackgroundfilter.h"
#include "macplatformwindow.h"
#include "platform/mac/macactivity.h"
#include "urlpasteboardmime.h"
#include "macclipboard.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QScopedPointer>
#include <QStringList>

#include <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>

namespace {
    class ClipboardApplication : public QApplication
    {
    public:
        ClipboardApplication(int &argc, char **argv)
            : QApplication(argc, argv)
            , m_pasteboardMime()
            , m_pasteboardMimeUrl(QLatin1String("public.url"))
            , m_pasteboardMimeFileUrl(QLatin1String("public.file-url"))
        {
        }

    private:
        CopyQPasteboardMime m_pasteboardMime;
        UrlPasteboardMime m_pasteboardMimeUrl;
        UrlPasteboardMime m_pasteboardMimeFileUrl;
    };

    template<typename T> inline T* objc_cast(id from)
    {
        if (from && [from isKindOfClass:[T class]]) {
            return static_cast<T*>(from);
        }
        return nil;
    }

    bool isApplicationInItemList(LSSharedFileListRef list) {
        bool flag = false;
        UInt32 seed;
        CFArrayRef items = LSSharedFileListCopySnapshot(list, &seed);
        if (items) {
            CFURLRef url = (__bridge CFURLRef)[NSURL fileURLWithPath:[[NSBundle mainBundle] bundlePath]];
            if (url) {
                for (id item in(__bridge NSArray *) items) {
                    LSSharedFileListItemRef itemRef = (__bridge LSSharedFileListItemRef)item;
                    if (LSSharedFileListItemResolve(itemRef, 0, &url, NULL) == noErr) {
                        if ([[(__bridge NSURL *) url path] hasPrefix:[[NSBundle mainBundle] bundlePath]]) {
                            flag = true;
                            break;
                        }
                    }
                }
            }
            CFRelease(items);
        }
        return flag;
    }

    void addToLoginItems()
    {
        LSSharedFileListRef list = LSSharedFileListCreate(kCFAllocatorDefault, kLSSharedFileListSessionLoginItems, NULL);
        if (list) {
            if (!isApplicationInItemList(list)) {
                CFURLRef url = (__bridge CFURLRef)[NSURL fileURLWithPath:[[NSBundle mainBundle] bundlePath]];
                if (url) {
                    // Don't "Hide on Launch", as we don't have a window to show anyway
                    NSDictionary *properties = [NSDictionary
                        dictionaryWithObject: [NSNumber numberWithBool:NO]
                        forKey: @"com.apple.loginitem.HideOnLaunch"];
                    LSSharedFileListItemRef item = LSSharedFileListInsertItemURL(list, kLSSharedFileListItemLast, NULL, NULL, url, (__bridge CFDictionaryRef)properties, NULL);
                    if (item)
                        CFRelease(item);
                } else {
                    ::log("Unable to find url for bundle, can't auto-load app", LogWarning);
                }
            }
            CFRelease(list);
        } else {
            ::log("Unable to access shared file list, can't auto-load app", LogWarning);
        }
    }

    void removeFromLoginItems()
    {
        LSSharedFileListRef list = LSSharedFileListCreate(kCFAllocatorDefault, kLSSharedFileListSessionLoginItems, NULL);
        if (list) {
            if (isApplicationInItemList(list)) {
                CFURLRef url = (__bridge CFURLRef)[NSURL fileURLWithPath:[[NSBundle mainBundle] bundlePath]];
                if (url) {
                    UInt32 seed;
                    CFArrayRef items = LSSharedFileListCopySnapshot(list, &seed);
                    if (items) {
                        for (id item in(__bridge NSArray *) items) {
                            LSSharedFileListItemRef itemRef = (__bridge LSSharedFileListItemRef)item;
                            if (LSSharedFileListItemResolve(itemRef, 0, &url, NULL) == noErr)
                                if ([[(__bridge NSURL *) url path] hasPrefix:[[NSBundle mainBundle] bundlePath]])
                                    LSSharedFileListItemRemove(list, itemRef);
                        }
                        CFRelease(items);
                    } else {
                        ::log("No items in list of auto-loaded apps, can't stop auto-load of app", LogWarning);
                    }
                } else {
                    ::log("Unable to find url for bundle, can't stop auto-load of app", LogWarning);
                }
            }
            CFRelease(list);
        } else {
            ::log("Unable to access shared file list, can't stop auto-load of app", LogWarning);
        }
    }

    QString absoluteResourcesePath(const QString &path)
    {
        return QCoreApplication::applicationDirPath() + "/../Resources/" + path;
    }

    template <typename QtApplication>
    class Activity
        : public MacActivity
        , public ApplicationExceptionHandler<QtApplication>
    {
    public:
        Activity(int &argc, char **argv, const QString &reason)
            : MacActivity(reason)
            , ApplicationExceptionHandler<QtApplication>(argc, argv)
        {
            [NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];
        }
    };

} // namespace

PlatformNativeInterface *platformNativeInterface()
{
    static MacPlatform platform;
    return &platform;
}

MacPlatform::MacPlatform()
{
}

QCoreApplication *MacPlatform::createConsoleApplication(int &argc, char **argv)
{
    return new ApplicationExceptionHandler<QCoreApplication>(argc, argv);
}

QApplication *MacPlatform::createServerApplication(int &argc, char **argv)
{
    QApplication *app = new Activity<ClipboardApplication>(argc, argv, "CopyQ Server");

    // Switch the app to foreground when in foreground
    ForegroundBackgroundFilter::installFilter(app);

    return app;
}

QGuiApplication *MacPlatform::createMonitorApplication(int &argc, char **argv)
{
    return new Activity<ClipboardApplication>(argc, argv, "CopyQ clipboard monitor");
}

QGuiApplication *MacPlatform::createClipboardProviderApplication(int &argc, char **argv)
{
    return new Activity<ClipboardApplication>(argc, argv, "CopyQ clipboard provider");
}

QCoreApplication *MacPlatform::createClientApplication(int &argc, char **argv)
{
    return new Activity<QCoreApplication>(argc, argv, "CopyQ Client");
}

QGuiApplication *MacPlatform::createTestApplication(int &argc, char **argv)
{
    return new Activity<QGuiApplication>(argc, argv, "CopyQ Tests");
}

PlatformClipboardPtr MacPlatform::clipboard()
{
    return PlatformClipboardPtr(new MacClipboard());
}

QStringList MacPlatform::getCommandLineArguments(int argc, char **argv)
{
    QStringList arguments;

    for (int i = 1; i < argc; ++i)
        arguments.append( QString::fromUtf8(argv[i]) );

    return arguments;
}

bool MacPlatform::findPluginDir(QDir *pluginsDir)
{
    pluginsDir->setPath( qApp->applicationDirPath() );
    if (pluginsDir->dirName() != "MacOS") {
        if ( pluginsDir->cd("plugins")) {
            COPYQ_LOG("Found plugins in build tree");
            return true;
        }
        return false;
    }

    if ( pluginsDir->cdUp() // Contents
            && pluginsDir->cd("PlugIns")
            && pluginsDir->cd("copyq"))
    {
        // OK, found it in the bundle
        COPYQ_LOG("Found plugins in application bundle");
        return true;
    }

    pluginsDir->setPath( qApp->applicationDirPath() );

    if ( pluginsDir->cdUp() // Contents
            && pluginsDir->cdUp() // copyq.app
            && pluginsDir->cdUp() // repo root
            && pluginsDir->cd("plugins")) {
        COPYQ_LOG("Found plugins in build tree");
        return true;
    }

    return false;
}

QString MacPlatform::defaultEditorCommand()
{
    return "open -t -W -n %1";
}

QString MacPlatform::translationPrefix()
{
    return absoluteResourcesePath("translations");
}

QString MacPlatform::themePrefix()
{
    return absoluteResourcesePath("themes");
}

PlatformWindowPtr MacPlatform::getCurrentWindow()
{
    // FIXME: frontmostApplication doesn't seem to work well for own windows (at least in tests).
    auto window = QApplication::activeWindow();
    if (window == nullptr)
        window = QApplication::activeModalWidget();
    if (window != nullptr)
        return PlatformWindowPtr(new MacPlatformWindow(window->winId()));

    NSRunningApplication *runningApp = [[NSWorkspace sharedWorkspace] frontmostApplication];
    return PlatformWindowPtr(new MacPlatformWindow(runningApp));
}

PlatformWindowPtr MacPlatform::getWindow(WId winId) {
    return PlatformWindowPtr(new MacPlatformWindow(winId));
}

bool MacPlatform::isAutostartEnabled()
{
    // Note that this will need to be done differently if CopyQ goes into
    // the App Store.
    // http://rhult.github.io/articles/sandboxed-launch-on-login/
    bool isInList = false;
    LSSharedFileListRef list = LSSharedFileListCreate(kCFAllocatorDefault, kLSSharedFileListSessionLoginItems, NULL);
    if (list) {
        isInList = isApplicationInItemList(list);
        CFRelease(list);
    }
    return isInList;
}

void MacPlatform::setAutostartEnabled(bool shouldEnable)
{
    if (shouldEnable != isAutostartEnabled()) {
        if (shouldEnable) {
            addToLoginItems();
        } else {
            removeFromLoginItems();
        }
    }
}
