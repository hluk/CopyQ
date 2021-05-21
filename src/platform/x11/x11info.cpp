/*
    Copyright (c) 2021, Lukas Holecek <hluk@email.cz>

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

#include "x11info.h"

#include <QGuiApplication>

#include <X11/Xlib.h>

bool X11Info::isPlatformX11()
{
    return QGuiApplication::platformName() == QStringLiteral("xcb");
}

Display *X11Info::display()
{
    static const QByteArray displayName = qgetenv("DISPLAY");
    static Display *display = XOpenDisplay(displayName.constData());
    return display;
}
