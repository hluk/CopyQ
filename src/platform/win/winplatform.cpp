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

#include <qt_windows.h>
#include <windows.h>

PlatformPtr createPlatformNativeInterface()
{
    return PlatformPtr(new WinPlatform);
}

WinPlatform::WinPlatform()
{
}

bool WinPlatform::isValid()
{
    return true;
}

QString WinPlatform::getCurrentWindowTitle()
{
    TCHAR buf[1024];
    GetWindowText(GetForegroundWindow(), buf, 1024);
#   ifdef UNICODE
    return QString::fromUtf16(reinterpret_cast<ushort *>(buf));
#   else
    return QString::fromLocal8Bit(buf);
#   endif
}

void WinPlatform::raiseWindow(WId wid)
{
    SetForegroundWindow(wid);
    SetWindowPos(wid, HWND_TOP, 0, 0, 0, 0,
                 SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
}

void WinPlatform::pasteToCurrentWindow()
{
}
