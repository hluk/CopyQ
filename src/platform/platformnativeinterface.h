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
#include <QWidget>

class PlatformWindow;
class QWidget;

/**
 * Shared pointer type for PlatformWindow.
 */
typedef QSharedPointer<PlatformWindow> PlatformWindowPtr;

/**
 * Interface for platform dependent code.
 */
class PlatformNativeInterface
{
public:
    virtual ~PlatformNativeInterface() {}

    /**
     * Get window from widget (NULL if failed or not implemented).
     */
    virtual PlatformWindowPtr getWindow(WId winId) = 0;

    /**
     * Get currently focused window (NULL if failed or not implemented).
     */
    virtual PlatformWindowPtr getCurrentWindow() = 0;

    /**
     * Return true automatic the application start at system startup is supported.
     */
    virtual bool canAutostart() = 0;

    /**
     * Return true if the application is automatically started at system startup.
     */
    virtual bool isAutostartEnabled() = 0;

    /**
     * Enable automatic application start at system startup.
     */
    virtual void setAutostartEnabled(bool enable) = 0;

    /**
     * Called after application starts.
     */
    virtual void onApplicationStarted() = 0;
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
