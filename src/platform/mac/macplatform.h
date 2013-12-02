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

    WId getCurrentWindow();

    /**
     * Return window title text of current window.
     *
     * On OS X, this gets the title of the application instead of the window.
     */
    QString getWindowTitle(WId wid);

    void raiseWindow(WId wid);
    void pasteToWindow(WId wid);
    WId getPasteWindow();

    /**
     * Get the number of changes to the clipboard (NSPasteboard::changeCount).
     */
    long int getChangeCount();

    bool canAutostart() { return true; }
    bool isAutostartEnabled();
    void setAutostartEnabled(bool);

    bool becomeNormalApp();
    bool becomeBackgroundApp();
    bool isNormalApp();

private:
    static void *m_currentPasteWindow;
};

#endif // MACPLATFORM_H
