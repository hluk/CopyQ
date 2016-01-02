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

#ifndef X11DISPLAY_H
#define X11DISPLAY_H

#include "platform/platformnativeinterface.h"

#include <QSharedPointer>

class QApplication;
class QCoreApplication;
class X11DisplayGuard;

class X11Platform : public PlatformNativeInterface
{
public:
    X11Platform();

    ~X11Platform();

    PlatformWindowPtr getWindow(WId winId);

    PlatformWindowPtr getCurrentWindow();

    PlatformWindowPtr deserialize(const QByteArray &data);

    bool serialize(WId winId, QByteArray *data);

    bool canAutostart();

    /**
     * Return true only if "copyq.desktop" file exists in "autostart" directory of current user and
     * it doesn't contain "Hidden" property or its value is false.
     */
    bool isAutostartEnabled();

    /**
     * Replace "Hidden" property in current user's autostart "copyq.desktop" file
     * (create the file if it doesn't exist).
     *
     * Additionally, replace "Exec" property with current application path.
     */
    void setAutostartEnabled(bool);

    QApplication *createServerApplication(int &argc, char **argv);

    QApplication *createMonitorApplication(int &argc, char **argv);

    QCoreApplication *createClientApplication(int &argc, char **argv);

    void loadSettings() {}

    PlatformClipboardPtr clipboard();

private:
    QSharedPointer<X11DisplayGuard> d;
};

#endif // X11DISPLAY_H
