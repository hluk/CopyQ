// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MACPLATFORM_H
#define MACPLATFORM_H

#include "platform/platformnativeinterface.h"

#include <QKeyEvent>
#include <QString>

class MacPlatform final : public PlatformNativeInterface
{
public:
    MacPlatform();

    PlatformWindowPtr getWindow(WId winId) override;
    PlatformWindowPtr getCurrentWindow() override;

    bool canGetWindowTitle() override { return true; }
    bool canAutostart() override { return true; }
    bool isAutostartEnabled() override;
    void setAutostartEnabled(bool) override;

    QCoreApplication *createConsoleApplication(int &argc, char **argv) override;

    QApplication *createServerApplication(int &argc, char **argv) override;

    QGuiApplication *createMonitorApplication(int &argc, char **argv) override;

    QGuiApplication *createClipboardProviderApplication(int &argc, char **argv) override;

    QCoreApplication *createClientApplication(int &argc, char **argv) override;

    QGuiApplication *createTestApplication(int &argc, char **argv) override;

    PlatformClipboardPtr clipboard() override;

    QStringList getCommandLineArguments(int argc, char **argv) override;

    bool findPluginDir(QDir *pluginsDir) override;

    QString defaultEditorCommand() override;

    QString translationPrefix() override;

    QString themePrefix() override;
};

#endif // MACPLATFORM_H
