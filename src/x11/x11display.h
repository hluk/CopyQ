/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#ifndef X11DISPLAY_H
#define X11DISPLAY_H

#include <QString>
#include <QWidget>

struct X11DisplayPrivate;

class X11Display
{
public:
    X11Display();

    ~X11Display();

    bool isValid() const;

    QString getCurrentWindowTitle();

    bool hasFocus(WId wid);

    void raiseWindow(WId wid);

    void pasteToCurrentWindow();

    bool isButtonOrShiftPressed();

private:
    X11DisplayPrivate *d;
};

#endif // X11DISPLAY_H
