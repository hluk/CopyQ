// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MACPLATFORMWINDOW_H
#define MACPLATFORMWINDOW_H

#include "platform/platformwindow.h"

// For WId
#include <QWidget>

#ifdef __OBJC__
@class NSWindow;
@class NSRunningApplication;
#else
using NSWindow = void;
using NSRunningApplication = void;
#endif

class MacPlatformWindow final : public PlatformWindow
{
public:
    MacPlatformWindow();
    explicit MacPlatformWindow(WId wid);
    explicit MacPlatformWindow(NSRunningApplication *runningApplication);

    virtual ~MacPlatformWindow();

    /**
     * Return window title text of current window.
     *
     * On OS X, this gets the title of the application instead of the window.
     */
    QString getTitle();

    void raise();
    void pasteClipboard();
    void copy();

private:
    // Don't allow copies
    Q_DISABLE_COPY(MacPlatformWindow)

    long int m_windowNumber;
    NSWindow *m_window;
    NSRunningApplication *m_runningApplication;
};

#endif // MACPLATFORMWINDOW_H
