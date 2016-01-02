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

#ifndef PLATFORMNATIVEINTERFACE_H
#define PLATFORMNATIVEINTERFACE_H

#include "platform/platformwindow.h"
#include "platform/platformclipboard.h"

#include <QKeyEvent>
#include <QSharedPointer>
#include <QWidget>
#include <Qt>

class QApplication;
class QCoreApplication;
class QByteArray;
class QWidget;

typedef QSharedPointer<PlatformWindow> PlatformWindowPtr;

typedef QSharedPointer<PlatformClipboard> PlatformClipboardPtr;

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
     * Create QApplication object for server.
     */
    virtual QApplication *createServerApplication(int &argc, char **argv) = 0;

    /**
     * Create QApplication object for clipboard monitor.
     */
    virtual QApplication *createMonitorApplication(int &argc, char **argv) = 0;

    /**
     * Create QCoreApplication object for client.
     */
    virtual QCoreApplication *createClientApplication(int &argc, char **argv) = 0;

    /**
     * Modify settings (QSettings) before it's first used.
     */
    virtual void loadSettings() = 0;

    /**
     * Deserialize window from data (for serialization use PlatformNativeInterface::serialize()).
     * Only used to steal window focus on client side.
     * Returns null pointer if deserialization fails.
     */
    virtual PlatformWindowPtr deserialize(const QByteArray &) { return PlatformWindowPtr(); }

    /**
     * Serialize window ID (before sending it to client).
     * Only used to steal window focus on client side.
     * Returns true if serialization is successful.
     */
    virtual bool serialize(WId, QByteArray *) { return false; }

    /**
     * Return object for managing clipboard.
     */
    virtual PlatformClipboardPtr clipboard() = 0;

    /**
     * Return Qt key code from key press event (possibly using QKeyEvent::nativeVirtualKey()).
     */
    virtual int keyCode(const QKeyEvent &event) { return event.key(); }

    /**
     * Returns list of command line arguments without executable name (argv[0]).
     *
     * Default implementation returns argv[1] up to argv[argc - 1] and assumes Utf-8 encoding.
     */
    virtual QStringList getCommandLineArguments(int argc, char **argv)
    {
        QStringList arguments;

        for (int i = 1; i < argc; ++i)
            arguments.append( QString::fromUtf8(argv[i]) );

        return arguments;
    }
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
