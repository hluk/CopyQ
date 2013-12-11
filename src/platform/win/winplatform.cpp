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

#include "winplatform.h"
#include "winplatformwindow.h"

#include <qt_windows.h>

PlatformPtr createPlatformNativeInterface()
{
    return PlatformPtr(new WinPlatform);
}

PlatformWindowPtr WinPlatform::getWindow(WId winId)
{
    HWND window = static_cast<HWND>(winId);
    return PlatformWindowPtr( window ? new WinPlatformWindow(window) : NULL );
}

PlatformWindowPtr WinPlatform::getCurrentWindow()
{
    HWND window = GetForegroundWindow();
    return PlatformWindowPtr( window ? new WinPlatformWindow(window) : NULL );
}

PlatformWindowPtr WinPlatform::getPasteWindow()
{
    return getCurrentWindow();
}
