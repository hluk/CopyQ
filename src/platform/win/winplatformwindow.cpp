// SPDX-License-Identifier: GPL-3.0-or-later

#include "platform/platformcommon.h"
#include "winplatformwindow.h"

#include "common/appconfig.h"
#include "common/log.h"
#include "common/sleeptimer.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QString>
#include <QVector>

namespace {

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

bool waitForModifiersReleased(const AppConfig &config)
{
    const int maxWaitForModsReleaseMs = config.option<Config::window_wait_for_modifier_released_ms>();
    if (maxWaitForModsReleaseMs >= 0) {
        SleepTimer t(maxWaitForModsReleaseMs);
        while (t.sleep()) {
            if (!isModifierPressed())
                return true;
        }
    }

    return !isModifierPressed();
}

bool sendInputs(QVector<INPUT> input, HWND wnd)
{
    const UINT numberOfAddedEvents = SendInput( input.size(), input.data(), sizeof(INPUT) );
    if (numberOfAddedEvents == 0u) {
        logWindow("Failed to paste", wnd);
        return false;
    }

    logWindow("Paste successful", wnd);
    return true;
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
    const AppConfig config;

    if ( pasteWithCtrlV(*this, config) )
        sendKeyPress(VK_LCONTROL, 'V', config);
    else
        sendKeyPress(VK_LSHIFT, VK_INSERT, config);
}

void WinPlatformWindow::copy()
{
    const AppConfig config;

    const DWORD clipboardSequenceNumber = GetClipboardSequenceNumber();
    sendKeyPress(VK_LCONTROL, 'C', config);

    // Wait for clipboard to change.
    QElapsedTimer t;
    t.start();
    while ( clipboardSequenceNumber == GetClipboardSequenceNumber() && t.elapsed() < 2000 )
        QApplication::processEvents(QEventLoop::AllEvents, 100);
}

void WinPlatformWindow::sendKeyPress(WORD modifier, WORD key, const AppConfig &config)
{
    waitMs(config.option<Config::window_wait_before_raise_ms>());

    if (!raiseWindow(m_window))
        return;

    waitMs(config.option<Config::window_wait_after_raised_ms>());

    // Wait for user to release modifiers.
    if (!waitForModifiersReleased(config))
        return;

    const int keyPressTimeMs = config.option<Config::window_key_press_time_ms>();
    if (keyPressTimeMs <= 0) {
        sendInputs({
           createInput(modifier),
           createInput(key),
           createInput(key, KEYEVENTF_KEYUP),
           createInput(modifier, KEYEVENTF_KEYUP)
        }, m_window);
    } else {
        const bool sent = sendInputs({
            createInput(modifier),
            createInput(key)
        }, m_window);
        if (!sent)
            return;

        waitMs(keyPressTimeMs);

        sendInputs({
           createInput(key, KEYEVENTF_KEYUP),
           createInput(modifier, KEYEVENTF_KEYUP)
        }, m_window);
    }
}
