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

#ifndef X11PLATFORMWINDOW_H
#define X11PLATFORMWINDOW_H

#include "platform/platformwindow.h"

#include <X11/Xlib.h>

#include <memory>

class QWidget;

class X11PlatformWindow final : public PlatformWindow
{
public:
    explicit X11PlatformWindow();

    explicit X11PlatformWindow(Window winId);

    QString getTitle() override;

    void raise() override;

    void pasteClipboard() override;

    void copy() override;

    bool isValid() const;

private:
    bool waitForFocus(int ms);

    void sendKeyPress(int modifier, int key);

    Window m_window;
};

#endif // X11PLATFORMWINDOW_H
