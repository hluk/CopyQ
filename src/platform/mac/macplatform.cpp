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

#include "macplatform.h"

//#include <qt_mac.h>

namespace {

} // namespace

PlatformPtr createPlatformNativeInterface()
{
    return PlatformPtr(new MacPlatform);
}

MacPlatform::MacPlatform()
{
}

WId MacPlatform::getCurrentWindow()
{
    // TODO
    return (WId)0;
}

QString MacPlatform::getWindowTitle(WId wid)
{
    // TODO
    Q_UNUSED(wid);
    return QString();
}

void MacPlatform::raiseWindow(WId wid)
{
    // TODO
    Q_UNUSED(wid);
}

void MacPlatform::pasteToWindow(WId wid)
{
    // TODO
    Q_UNUSED(wid);
}

WId MacPlatform::getPasteWindow()
{
    return getCurrentWindow();
}
