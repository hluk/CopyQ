// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WINPLATFORMWINDOW_H
#define WINPLATFORMWINDOW_H

#include "platform/platformwindow.h"

#ifndef WINVER
#define WINVER 0x0500
#endif
#include <qt_windows.h>

class AppConfig;

class WinPlatformWindow final : public PlatformWindow
{
public:
    explicit WinPlatformWindow(HWND window);

    QString getTitle() override;

    void raise() override;

    void pasteClipboard() override;
    void copy() override;

private:
    void sendKeyPress(WORD modifier, WORD key, const AppConfig &config);

    HWND m_window;
};

#endif // WINPLATFORMWINDOW_H
