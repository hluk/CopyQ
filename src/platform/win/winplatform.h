// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include "platform/platformnativeinterface.h"

class WinPlatform final : public PlatformNativeInterface
{
public:
    WinPlatform() {}

    PlatformWindowPtr getWindow(WId winId) override;

    PlatformWindowPtr getCurrentWindow() override;

    bool canGetWindowTitle() override { return true; }

    bool canAutostart() override { return true; }

    bool isAutostartEnabled() override;

    void setAutostartEnabled(bool enable) override;

    bool setPreventScreenCapture(WId winId, bool prevent) override;
    bool canPreventScreenCapture() override { return true; }

    QCoreApplication *createConsoleApplication(int &argc, char **argv) override;

    QApplication *createServerApplication(int &argc, char **argv) override;

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
