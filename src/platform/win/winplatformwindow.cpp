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

#include "platform/platformcommon.h"
#include "winplatformwindow.h"

#include "common/log.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QString>
#include <QVector>

namespace {

const int waitForModsReleaseMs = 25;
const int maxWaitForModsReleaseMs = 2000;

QString windowTitle(HWND window)
{
    WCHAR buf[1024];
    GetWindowTextW(window, buf, 1024);
    return QString::fromUtf16(reinterpret_cast<ushort *>(buf));
}

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

QString windowLogText(QString text, HWND window)
{
    const QString windowInfo =
            QString("%1").arg(reinterpret_cast<quintptr>(window))
            + " \"" + windowTitle(window) + "\"";

    text.prepend("Window " + windowInfo + ": ");

    const DWORD lastError = GetLastError();
    if (lastError != 0)
        text.append( QString(" (last error is %1)").arg(GetLastError()) );

    return text;
}

void logWindow(const char *text, HWND window)
{
    COPYQ_LOG( windowLogText(text, window) );
}

bool raiseWindowHelper(HWND window)
{
    if (!SetForegroundWindow(window)) {
        logWindow("Failed to raise:  SetForegroundWindow() == false", window);
        return false;
    }

    SetWindowPos(window, HWND_TOP, 0, 0, 0, 0,
                 SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

    logWindow("Raised", window);

    return true;
}

bool raiseWindow(HWND window)
{
    if (!IsWindowVisible(window)) {
        logWindow("Failed to raise: IsWindowVisible() == false", window);
        return false;
    }

    // WORKAROUND: Set foreground window if even this process is not in foreground.
    const auto thisThreadId = GetCurrentThreadId();
    const auto foregroundThreadId = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
    if (thisThreadId != foregroundThreadId) {
        if ( AttachThreadInput(thisThreadId, foregroundThreadId, true) ) {
            logWindow("Attached foreground thread", window);
            const bool result = raiseWindowHelper(window);
            AttachThreadInput(thisThreadId, foregroundThreadId, false);
            return result;
        }

        logWindow("Failed to attach foreground thread", window);
    }

    return raiseWindowHelper(window);
}

bool isKeyPressed(int key)
{
    return GetKeyState(key) & 0x8000;
}

bool isModifierPressed()
{
    return isKeyPressed(VK_LCONTROL)
        || isKeyPressed(VK_RCONTROL)
        || isKeyPressed(VK_LSHIFT)
        || isKeyPressed(VK_RSHIFT)
        || isKeyPressed(VK_LMENU)
        || isKeyPressed(VK_RMENU)
        || isKeyPressed(VK_MENU);
}

bool waitForModifiersReleased()
{
    for (int ms = 0; ms < maxWaitForModsReleaseMs; ms += waitForModsReleaseMs) {
        if (!isModifierPressed())
            return true;
        Sleep(waitForModsReleaseMs);
    }

    return false;
}

} // namespace

WinPlatformWindow::WinPlatformWindow(HWND window)
    : m_window(window)
{
}

QString WinPlatformWindow::getTitle()
{
    return windowTitle(m_window);
}

void WinPlatformWindow::raise()
{
    raiseWindow(m_window);
}

void WinPlatformWindow::pasteClipboard()
{
    if ( pasteWithCtrlV(*this) )
        sendKeyPress(VK_LCONTROL, 'V');
    else
        sendKeyPress(VK_LSHIFT, VK_INSERT);

    // Don't do anything hasty until the content is actually pasted.
    Sleep(150);
}

void WinPlatformWindow::copy()
{
    const DWORD clipboardSequenceNumber = GetClipboardSequenceNumber();
    sendKeyPress(VK_LCONTROL, 'C');

    // Wait for clipboard to change.
    QElapsedTimer t;
    t.start();
    while ( clipboardSequenceNumber == GetClipboardSequenceNumber() && t.elapsed() < 2000 )
        QApplication::processEvents(QEventLoop::AllEvents, 100);
}

void WinPlatformWindow::sendKeyPress(WORD modifier, WORD key)
{
     if (!raiseWindow(m_window))
        return;

    Sleep(150);

    // Wait for user to release modifiers.
    if (!waitForModifiersReleased())
        return;

    auto input = QVector<INPUT>()
          << createInput(modifier)
          << createInput(key)
          << createInput(key, KEYEVENTF_KEYUP)
          << createInput(modifier, KEYEVENTF_KEYUP);

    const UINT numberOfAddedEvents = SendInput( input.size(), input.data(), sizeof(INPUT) );

    if (numberOfAddedEvents == 0u)
        logWindow("Failed to paste", m_window);
    else
        logWindow("Paste successful", m_window);
}
