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

#include "common/log.h"
#include "common/settings.h"

#include "platform/dummy/dummyclipboard.h"
#include "winplatform.h"
#include "winplatformclipboard.h"
#include "winplatformwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

#include <qt_windows.h>

#include <stdio.h>
#include <fcntl.h>
#include <io.h>

namespace {

void setBinaryFor(FILE *f)
{
    const int result = _setmode( _fileno(f), _O_BINARY );
    if (result == -1)
        log("Failed to set binary stdin.", LogError);
}

void migrateDirectory(const QString oldPath, const QString newPath)
{
    QDir oldDir(oldPath);
    QDir newDir(newPath);

    if ( oldDir.exists() ) {
        newDir.mkpath(".");

        foreach ( const QString &fileName, oldDir.entryList(QDir::Files) ) {
            const QString oldFileName = oldDir.absoluteFilePath(fileName);
            const QString newFileName = newDir.absoluteFilePath(fileName);
            COPYQ_LOG( QString("Migrating \"%1\" -> \"%2\"")
                       .arg(oldFileName)
                       .arg(newFileName) );
            QFile::copy(oldFileName, newFileName);
        }
    }
}

void migrateConfig(const QSettings &oldSettings, Settings &newSettings)
{
    foreach ( const QString &key, oldSettings.allKeys() )
        newSettings.setValue(key, oldSettings.value(key));
}

void migrateConfigToAppDir()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString path = appDir + "/config";
    const QString uninstPath = appDir + "/unins000.exe";
    QDir dir(path);

    if ( !QFile::exists(uninstPath)
         && QFileInfo(appDir).isWritable()
         && dir.mkpath("copyq")
         && dir.cd("copyq")
         && dir.isReadable()
         && QFileInfo(dir.absolutePath()).isWritable() )
    {
        QSettings oldSettings;
        const QString oldConfigFileName =
                QSettings(QSettings::IniFormat, QSettings::UserScope,
                          QCoreApplication::organizationName(),
                          QCoreApplication::applicationName()).fileName();
        const QString oldConfigPath = QDir::cleanPath(oldConfigFileName + "/..");

        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, path);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        Settings newSettings;

        if ( Settings::canModifySettings() && newSettings.isEmpty() ) {
            COPYQ_LOG("Migrating configuration to application directory.");
            const QString newConfigPath = QDir::cleanPath(newSettings.fileName() + "/..");

            // Migrate configuration from system directory.
            migrateDirectory(oldConfigPath, newConfigPath);

            // Migrate themes from system directory.
            migrateDirectory(oldConfigPath + "/themes", newConfigPath + "/themes");

            // Migrate rest of the configuration from the system registry.
            migrateConfig(oldSettings, newSettings);
        }
    } else {
        COPYQ_LOG( QString("Cannot use \"%1\" directory to save user configuration and items.")
                   .arg(path) );

        QSettings oldSettings;

        QSettings::setDefaultFormat(QSettings::IniFormat);
        Settings newSettings;

        // Move settings from Windows registry.
        if (newSettings.isEmpty()) {
            COPYQ_LOG("Moving configuration from Windows registry.");
            migrateConfig(oldSettings, newSettings);
        }
    }
}

BOOL ctrlHandler(DWORD fdwCtrlType)
{
    switch (fdwCtrlType) {
    case CTRL_C_EVENT:
        COPYQ_LOG("Terminating application on signal.");
        QCoreApplication::exit();
        return TRUE;

    case CTRL_CLOSE_EVENT:
        COPYQ_LOG("Terminating application on close event.");
        QCoreApplication::exit();
        return TRUE;

    case CTRL_BREAK_EVENT:
        COPYQ_LOG("Terminating application on break event.");
        QCoreApplication::exit();
        return TRUE;

    case CTRL_LOGOFF_EVENT:
        COPYQ_LOG("Terminating application on log off.");
        QCoreApplication::exit();
        return TRUE;

    case CTRL_SHUTDOWN_EVENT:
        COPYQ_LOG("Terminating application on shut down.");
        QCoreApplication::exit();
        return TRUE;

    default:
        return FALSE;
    }
}

void installControlHandler()
{
    if ( !SetConsoleCtrlHandler(reinterpret_cast<PHANDLER_ROUTINE>(ctrlHandler), TRUE) )
        log("Failed to set Windows control handler.", LogError);
}

template <typename Application>
Application *createApplication(int &argc, char **argv)
{
    Application *app = new Application(argc, argv);
    installControlHandler();
    setBinaryFor(stdin);
    setBinaryFor(stdout);
    return app;
}

QString windowClass(HWND window)
{
    WCHAR buf[32];
    GetClassNameW(window, buf, 32);
    return QString::fromUtf16(reinterpret_cast<ushort *>(buf));
}

HWND getLastVisibleActivePopUpOfWindow(HWND window)
{
    HWND currentWindow = window;

    for (int i = 0; i < 50; ++i) {
        HWND lastPopUp = GetLastActivePopup(currentWindow);

        if (IsWindowVisible(lastPopUp))
            return lastPopUp;

        if (lastPopUp == currentWindow)
            return NULL;

        currentWindow = lastPopUp;
    }

    return NULL;
}

bool isAltTabWindow(HWND window)
{
    if (!window || window == GetShellWindow())
        return false;

    HWND root = GetAncestor(window, GA_ROOTOWNER);

    if (getLastVisibleActivePopUpOfWindow(root) != window)
        return false;

    const QString cls = windowClass(window);
	COPYQ_LOG( QString("cls: \"%1\"").arg(cls) );
    return !cls.isEmpty()
            && cls != "Shell_TrayWnd"
            && cls != "Shell_SecondaryTrayWnd"
            && cls != "DV2ControlHost"
            && cls != "MsgrIMEWindowClass"
            && cls != "SysShadow"
            && cls != "Button"
            && !cls.startsWith("WMP9MediaBarFlyout");
}

HWND currentWindow;
BOOL CALLBACK getCurrentWindowProc(HWND window, LPARAM)
{
    if (!isAltTabWindow(window))
        return TRUE;

    currentWindow = window;
    return FALSE;
}

} // namespace

PlatformPtr createPlatformNativeInterface()
{
    return PlatformPtr(new WinPlatform);
}

PlatformWindowPtr WinPlatform::getWindow(WId winId)
{
    HWND window = reinterpret_cast<HWND>(winId);
    return PlatformWindowPtr( window ? new WinPlatformWindow(window) : NULL );
}

PlatformWindowPtr WinPlatform::getCurrentWindow()
{
    currentWindow = GetForegroundWindow();
    if (!isAltTabWindow(currentWindow))
        EnumWindows(getCurrentWindowProc, 0);
    return PlatformWindowPtr( currentWindow ? new WinPlatformWindow(currentWindow) : NULL );
}

QApplication *WinPlatform::createServerApplication(int &argc, char **argv)
{
    return createApplication<QApplication>(argc, argv);
}

QApplication *WinPlatform::createMonitorApplication(int &argc, char **argv)
{
    return createApplication<QApplication>(argc, argv);
}

QCoreApplication *WinPlatform::createClientApplication(int &argc, char **argv)
{
    return createApplication<QCoreApplication>(argc, argv);
}

void WinPlatform::loadSettings()
{
    migrateConfigToAppDir();
}

PlatformClipboardPtr WinPlatform::clipboard()
{
    return PlatformClipboardPtr(new WinPlatformClipboard());
}

int WinPlatform::keyCode(const QKeyEvent &event)
{
    const int key = PlatformNativeInterface::keyCode(event);

    // Some keys shouldn't be translated.
    if ( key == Qt::Key_Return
      || key == Qt::Key_Enter
      || key == Qt::Key_Escape
      || key == Qt::Key_Tab
      || key == Qt::Key_Backtab
      || key == Qt::Key_Backspace
         )
    {
        return key;
    }

    const quint32 vk = event.nativeVirtualKey();
    const UINT result = MapVirtualKeyW(vk, MAPVK_VK_TO_CHAR);
    if (result != 0)
        return result;

    return key;
}

QStringList WinPlatform::getCommandLineArguments(int, char**)
{
    int argumentCount;
    LPWSTR *arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);

    QStringList result;

    for (int i = 1; i < argumentCount; ++i)
        result.append( QString::fromUtf16(reinterpret_cast<ushort*>(arguments[i])) );

    return result;
}

bool WinPlatform::findPluginDir(QDir *pluginsDir)
{
    pluginsDir->setPath( qApp->applicationDirPath() );
    return pluginsDir->cd("plugins");
}

QString WinPlatform::defaultEditorCommand()
{
    return "notepad %1";
}

QString WinPlatform::translationPrefix()
{
    return QCoreApplication::applicationDirPath() + "/translations";
}

QString WinPlatform::themePrefix()
{
    return QApplication::applicationDirPath() + "/themes";
}
