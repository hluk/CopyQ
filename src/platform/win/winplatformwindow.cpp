/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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
#include <QVector>

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

bool isKeyPressed(int key)
{
    return GetKeyState(key) & 0x8000;
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
    return QString::fromUtf8(buf);
#   endif
}

void WinPlatformWindow::raise()
{
    raiseWindow(m_window);
}

void WinPlatformWindow::pasteClipboard()
{
    if (!raiseWindow(m_window))
        return;

    Sleep(150);

    QVector<INPUT> input1;
    QVector<INPUT> input2;

    static const QList<int> mods = QList<int>()
            << VK_LCONTROL << VK_RCONTROL
            << VK_LSHIFT << VK_RSHIFT
            << VK_LMENU << VK_RMENU
            << VK_MENU;

    // Release all modifiers first to send just Shift+Insert.
    foreach (int mod, mods) {
        if ( isKeyPressed(mod) ) {
            input1 << createInput(mod, KEYEVENTF_KEYUP);
            input2 << createInput(mod); // Press again at the end.
        }
    }

    input1 << createInput(VK_LSHIFT)
           << createInput(VK_INSERT)
           << createInput(VK_INSERT, KEYEVENTF_KEYUP)
           << createInput(VK_LSHIFT, KEYEVENTF_KEYUP);

    QVector<INPUT> input = input1 + input2;
    SendInput( input.size(), input.data(), sizeof(INPUT) );

    // Don't do anything hasty until the content is actually pasted.
    Sleep(150);
}
