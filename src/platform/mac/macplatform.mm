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

#include <Cocoa/Cocoa.h>

#include <QApplication>
#include <QWidget>

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
    QWidget *widget = QApplication::activeWindow();
    if (widget == NULL)
        return 0L;

    return widget->winId();
}

QString MacPlatform::getWindowTitle(WId wid)
{
    if (wid == 0L)
        return QString();

    QWidget *widget = QWidget::find(wid);
    if (widget == NULL)
        return QString();

    return widget->windowTitle();
}

void MacPlatform::raiseWindow(WId wid)
{
    if (wid == 0L)
        return;

    QWidget *widget = QWidget::find(wid);
    if (widget == NULL)
        return;

    widget->setWindowState(widget->windowState() & ~Qt::WindowMinimized | Qt::WindowActive);
    widget->show();
    widget->activateWindow();
    widget->raise();
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
