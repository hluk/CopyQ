// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WINPLATFORM_H
#define WINPLATFORM_H

#include "platform/platformnativeinterface.h"

class WinPlatform final : public PlatformNativeInterface
{
public:
    WinPlatform() {}

    PlatformWindowPtr getWindow(WId winId) override;

    PlatformWindowPtr getCurrentWindow() override;

    bool canGetWindowTitle() override { return true; }

    /** Setting application autostart is not implemented for Windows (works just from installer). */
    bool canAutostart() override { return false; }

    bool isAutostartEnabled() override { return false; }

    void setAutostartEnabled(bool) override {}

    QCoreApplication *createConsoleApplication(int &argc, char **argv) override;

    QApplication *createServerApplication(int &argc, char **argv) override;

    QGuiApplication *createMonitorApplication(int &argc, char **argv) override;

    QGuiApplication *createClipboardProviderApplication(int &argc, char **argv) override;

    QCoreApplication *createClientApplication(int &argc, char **argv) override;

    QGuiApplication *createTestApplication(int &argc, char **argv) override;

    PlatformClipboardPtr clipboard() override;

    QStringList getCommandLineArguments(int, char**) override;

    bool findPluginDir(QDir *pluginsDir) override;

    QString defaultEditorCommand() override;

    QString translationPrefix() override;

    QString themePrefix() override;
};

#endif // WINPLATFORM_H
