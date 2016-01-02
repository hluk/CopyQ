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

#ifndef X11DISPLAYGUARD_H
#define X11DISPLAYGUARD_H

#include <QSharedPointer>

#include <X11/Xlib.h>

class X11DisplayGuard
{
public:
    /**
     * Opens Display with XOpenDisplay().
     * The create Display is automatically closed with XCloseDisplay() when object is destroyed.
     */
    X11DisplayGuard()
        : m_display(XOpenDisplay(NULL), XCloseDisplay)
    {}

    /**
     * Get the opened Display (can be NULL if opening failed).
     */
    Display *display() { return m_display.data(); }

private:
    QSharedPointer<Display> m_display;
};

#endif // X11DISPLAYGUARD_H
