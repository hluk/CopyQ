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

#include "winplatformwindow.h"

#include <QString>

namespace {

INPUT createInput(WORD key, DWORD flags = 0)
{
    INPUT input;

    input.type = INPUT_KEYBOARD;
    input.ki.wVk = key;
    input.ki.wScan = 0;
    input.ki.dwFlags = KEYEVENTF_UNICODE | flags;
    input.ki.time = 0;
    input.ki.dwExtraInfo = GetMessageExtraInfo();

    return input;
}

bool raiseWindow(HWND window)
{
    if (!SetForegroundWindow(window))
        return false;

    SetWindowPos(window, HWND_TOP, 0, 0, 0, 0,
                 SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

    return true;
}

} // namespace

WinPlatformWindow::WinPlatformWindow(HWND window)
    : m_window(window)
{
}

QString WinPlatformWindow::getTitle()
{
    TCHAR buf[1024];
    GetWindowText(m_window, buf, 1024);
#   ifdef UNICODE
    return QString::fromUtf16(reinterpret_cast<ushort *>(buf));
#   else
    return QString::fromLocal8Bit(buf);
#   endif
}

void WinPlatformWindow::raise()
{
    raiseWindow(m_window);
}

void WinPlatformWindow::pasteClipboard()
{
    INPUT input[4];

    input[0] = createInput(VK_LSHIFT);
    input[1] = createInput(VK_INSERT);
    input[2] = createInput(VK_INSERT, KEYEVENTF_KEYUP);
    input[3] = createInput(VK_LSHIFT, KEYEVENTF_KEYUP);

    if (!raiseWindow(m_window))
        return;

    Sleep(150);
    SendInput( 4, input, sizeof(INPUT) );

    // Don't do anything hasty until the content is actually pasted.
    Sleep(150);
}
