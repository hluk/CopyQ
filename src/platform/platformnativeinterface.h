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

#ifndef PLATFORMNATIVEINTERFACE_H
#define PLATFORMNATIVEINTERFACE_H

#include <QSharedPointer>
#include <QString>
#include <QWidget> // WId

/**
 * Interface for platform dependent code.
 */
class PlatformNativeInterface
{
public:
    virtual ~PlatformNativeInterface() {}

    /**
     * Return window title text of current window.
     */
    virtual QString getCurrentWindowTitle() = 0;

    /**
     * Raise and focus a window to foreground.
     */
    virtual void raiseWindow(WId wid) = 0;

    /**
     * Paste clipboard content to current window (e.g. send Ctrl+Insert to current window).
     */
    virtual void pasteToCurrentWindow() = 0;
};

/**
 * Shared pointer type for PlatformNativeInterface instance.
 */
typedef QSharedPointer<PlatformNativeInterface> PlatformPtr;

/**
 * Factory method to create PlatformNativeInterface instance.
 */
PlatformPtr createPlatformNativeInterface();

#endif // PLATFORMNATIVEINTERFACE_H
