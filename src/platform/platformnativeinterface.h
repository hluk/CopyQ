// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PLATFORMNATIVEINTERFACE_H
#define PLATFORMNATIVEINTERFACE_H

#include <QWidget> // WId

#include <memory>

class QApplication;
class QCoreApplication;
class QDir;
class QGuiApplication;
class QKeyEvent;

class PlatformWindow;
class PlatformClipboard;

using PlatformWindowPtr = std::shared_ptr<PlatformWindow>;
using PlatformClipboardPtr = std::shared_ptr<PlatformClipboard>;

/**
 * Interface for platform dependent code.
 */
class PlatformNativeInterface
{
public:
    PlatformNativeInterface() = default;

    virtual ~PlatformNativeInterface() = default;

    /**
     * Get window from widget (nullptr if failed or not implemented).
     */
    virtual PlatformWindowPtr getWindow(WId winId) = 0;

    /**
     * Get currently focused window (nullptr if failed or not implemented).
     */
    virtual PlatformWindowPtr getCurrentWindow() = 0;

    /**
     * Return true only if window titles can be retrieved using PlatformWindow::getTitle().
     */
    virtual bool canGetWindowTitle() = 0;

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
     * Create QCoreApplication object for console output (to show help or version and quit).
     */
    virtual QCoreApplication *createConsoleApplication(int &argc, char **argv) = 0;

    /**
     * Create QApplication object for server.
     */
    virtual QApplication *createServerApplication(int &argc, char **argv) = 0;

    /**
     * Create QGuiApplication object for clipboard monitor.
     */
    virtual QGuiApplication *createMonitorApplication(int &argc, char **argv) = 0;

    /**
     * Create QGuiApplication object that provides clipboard.
     */
    virtual QGuiApplication *createClipboardProviderApplication(int &argc, char **argv) = 0;

    /**
     * Create QCoreApplication object for client.
     */
    virtual QCoreApplication *createClientApplication(int &argc, char **argv) = 0;
    /**
     * Create QGuiApplication object for tests.
     */
    virtual QGuiApplication *createTestApplication(int &argc, char **argv) = 0;

    /**
     * Return object for managing clipboard.
     */
    virtual PlatformClipboardPtr clipboard() = 0;

    /**
     * Returns list of command line arguments without executable name (argv[0]).
     */
    virtual QStringList getCommandLineArguments(int argc, char **argv) = 0;

    /**
     * Find directory with plugins and return true on success.
     */
    virtual bool findPluginDir(QDir *pluginsDir) = 0;

    /**
     * Default editor command (e.g. "notepad %1"; "%1" will be replaced with file name to edit).
     */
    virtual QString defaultEditorCommand() = 0;

    /**
     * Path to translations.
     *
     * Can be overridden by preprocessor flag COPYQ_TRANSLATION_PREFIX.
     *
     * Custom translation prefix can be added by setting COPYQ_TRANSLATION_PREFIX
     * environment variable.
     */
    virtual QString translationPrefix() = 0;

    /**
     * Path to installed themes.
     *
     * Can be overridden by preprocessor flag COPYQ_THEME_PREFIX.
     *
     * Custom theme prefix can be added by setting COPYQ_THEME_PREFIX
     * environment variable.
     *
     * Note: Customized themes are saved to settings path.
     */
    virtual QString themePrefix() = 0;

    PlatformNativeInterface(const PlatformNativeInterface &) = delete;
    PlatformNativeInterface &operator=(const PlatformNativeInterface &) = delete;
};

PlatformNativeInterface *platformNativeInterface();

#endif // PLATFORMNATIVEINTERFACE_H
