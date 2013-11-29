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

#ifndef MACPLATFORM_H
#define MACPLATFORM_H

#include "platform/platformnativeinterface.h"

class MacPlatform : public PlatformNativeInterface
{
public:
    MacPlatform();

    /**
     * Get currently focused window.
     */
    WId getCurrentWindow();

    /**
     * Return window title text of current window.
     */
    QString getWindowTitle(WId wid);

    /**
     * Raise and focus a window to foreground.
     */
    void raiseWindow(WId wid);

    /**
     * Paste clipboard content to given window (e.g. send Ctrl+Insert to the window).
     */
    void pasteToWindow(WId wid);

    /**
     * Get window for pasting (can be different from current window).
     */
    WId getPasteWindow();

    /**
     * Get the number of changes to the clipboard (NSPasteboard::changeCount).
     */
    long int getChangeCount();

    // TODO: this
    bool canAutostart() { return false; }
    bool isAutostartEnabled() { return false; }
    void setAutostartEnabled(bool) {}
};

#endif // MACPLATFORM_H
