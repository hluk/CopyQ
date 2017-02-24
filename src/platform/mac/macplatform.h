/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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

#ifndef MACPLATFORM_H
#define MACPLATFORM_H

#include "platform/platformnativeinterface.h"

#include <QKeyEvent>
#include <QString>

class MacPlatform : public PlatformNativeInterface
{
public:
    MacPlatform();

    PlatformWindowPtr getWindow(WId winId);
    PlatformWindowPtr getCurrentWindow();

    bool canGetWindowTitle() { return true; }
    bool canAutostart() { return true; }
    bool isAutostartEnabled();
    void setAutostartEnabled(bool);

    QCoreApplication *createConsoleApplication(int &argc, char **argv);

    QApplication *createServerApplication(int &argc, char **argv);

    QApplication *createMonitorApplication(int &argc, char **argv);

    QCoreApplication *createClientApplication(int &argc, char **argv);

    void loadSettings() {}

    PlatformClipboardPtr clipboard();

    int keyCode(const QKeyEvent &event) { return event.key(); }

    QStringList getCommandLineArguments(int argc, char **argv);

    bool findPluginDir(QDir *pluginsDir);

    QString defaultEditorCommand();

    QString translationPrefix();

    QString themePrefix() { return QString(); }
};

#endif // MACPLATFORM_H
