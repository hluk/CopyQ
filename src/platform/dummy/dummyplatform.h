// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DUMMYPLATFORM_H
#define DUMMYPLATFORM_H

#include "platform/platformnativeinterface.h"

#include <QKeyEvent>
#include <QString>

class DummyPlatform : public PlatformNativeInterface
{
public:
    PlatformWindowPtr getWindow(WId) override { return PlatformWindowPtr(); }

    PlatformWindowPtr getCurrentWindow() override { return PlatformWindowPtr(); }

    bool canGetWindowTitle() override { return false; }

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

    QStringList getCommandLineArguments(int argc, char **argv) override;

    bool findPluginDir(QDir *pluginsDir) override;

    QString defaultEditorCommand() override;

    QString translationPrefix() override;

    QString themePrefix() override { return QString(); }
};

#endif // DUMMYPLATFORM_H
