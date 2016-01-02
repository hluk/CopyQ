/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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

#include "x11displayguard.h"

class QWidget;

class X11PlatformWindow : public PlatformWindow
{
public:
    explicit X11PlatformWindow(X11DisplayGuard &d);

    X11PlatformWindow(X11DisplayGuard &d, Window winId);

    QString getTitle();

    void raise();

    void pasteClipboard();

    void copy();

    bool isValid() const;

private:
    bool hasFocus();

    void sendKeyPress(int modifier, int key);

    Window m_window;
    X11DisplayGuard d;
};

#endif // X11PLATFORMWINDOW_H
