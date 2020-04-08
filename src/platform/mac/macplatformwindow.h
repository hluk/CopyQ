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
