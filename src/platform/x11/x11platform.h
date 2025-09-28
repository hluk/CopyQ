// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include "platform/platformnativeinterface.h"

#include <QKeyEvent>
#include <QString>

class X11Platform final : public PlatformNativeInterface
{
public:
    X11Platform() = default;

    ~X11Platform();

    PlatformWindowPtr getWindow(WId winId) override;

    PlatformWindowPtr getCurrentWindow() override;

    bool canGetWindowTitle() override { return true; }

    bool canAutostart() override;

    /**
     * Return true only if "copyq.desktop" file exists in "autostart" directory of current user and
     * it doesn't contain "Hidden" property or its value is false.
     */
    bool isAutostartEnabled() override;

    /**
     * Replace "Hidden" property in current user's autostart "copyq.desktop" file
     * (create the file if it doesn't exist).
     *
     * Additionally, replace "Exec" property with current application path.
     */
    void setAutostartEnabled(bool) override;

    bool setPreventScreenCapture(WId, bool) override { return false; }
    bool canPreventScreenCapture() override { return false; }

    QCoreApplication *createConsoleApplication(int &argc, char **argv) override;

    QApplication *createServerApplication(int &argc, char **argv) override;

    QGuiApplication *createClipboardProviderApplication(int &argc, char **argv) override;

    QCoreApplication *createClientApplication(int &argc, char **argv) override;

    QGuiApplication *createTestApplication(int &argc, char **argv) override;

    PlatformClipboardPtr clipboard() override;

    QStringList getCommandLineArguments(int argc, char **argv) override;

    bool findPluginDir(QDir *pluginsDir) override;

    QString defaultEditorCommand() override;

    QString translationPrefix() override;

    QString themePrefix() override { return QString(); }
};
