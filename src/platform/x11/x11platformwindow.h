// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef X11PLATFORMWINDOW_H
#define X11PLATFORMWINDOW_H

#include "platform/platformwindow.h"

#include <memory>

class AppConfig;
class QWidget;

class X11PlatformWindow final : public PlatformWindow
{
public:
    explicit X11PlatformWindow();

    explicit X11PlatformWindow(quintptr winId);

    QString getTitle() override;

    void raise() override;

    void pasteClipboard() override;

    void copy() override;

    bool isValid() const;

private:
    bool waitForFocus(int ms);

    void sendKeyPress(int modifier, int key, const AppConfig &config);

    quintptr m_window;
};

#endif // X11PLATFORMWINDOW_H
