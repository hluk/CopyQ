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

#ifndef WINPLATFORM_H
#define WINPLATFORM_H

#include "platform/platformnativeinterface.h"

#include <QStringList>

class QApplication;
class QCoreApplication;

class WinPlatform : public PlatformNativeInterface
{
public:
    WinPlatform() {}

    PlatformWindowPtr getWindow(WId winId);

    PlatformWindowPtr getCurrentWindow();

    /** Setting application autostart is not implemented for Windows (works just from installer). */
    bool canAutostart() { return false; }

    bool isAutostartEnabled() { return false; }

    void setAutostartEnabled(bool) {}

    QApplication *createServerApplication(int &argc, char **argv);

    QApplication *createMonitorApplication(int &argc, char **argv);

    QCoreApplication *createClientApplication(int &argc, char **argv);

    void loadSettings();

    PlatformClipboardPtr clipboard();

    int keyCode(const QKeyEvent &event);

    QStringList getCommandLineArguments(int, char**);
};

#endif // WINPLATFORM_H
