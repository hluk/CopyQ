// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include "platform/platformwindow.h"

class AppConfig;
class QWidget;

class X11PlatformWindow final : public PlatformWindow
{
public:
    explicit X11PlatformWindow();

    explicit X11PlatformWindow(quintptr winId);

    QString getTitle() override;

    void raise() override;

    bool pasteFromClipboard() override;

    bool copyToClipboard() override;

    bool isValid() const;

private:
    bool waitForFocus(int ms);

    bool sendKeyPress(int modifier, int key, const AppConfig &config);

    quintptr m_window;
};
