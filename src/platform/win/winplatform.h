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

#ifndef WINPLATFORM_H
#define WINPLATFORM_H

#include "platform/platformnativeinterface.h"

class WinPlatform : public PlatformNativeInterface
{
public:
    WinPlatform();

    bool isValid();

    QString getCurrentWindowTitle();

    void raiseWindow(WId wid);

    void pasteToCurrentWindow();
};

#endif // WINPLATFORM_H
