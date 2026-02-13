// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


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

    bool pasteFromClipboard() override;
    bool copyToClipboard() override;

private:
    bool sendKeyPress(WORD modifier, WORD key, const AppConfig &config);

    HWND m_window;
};
