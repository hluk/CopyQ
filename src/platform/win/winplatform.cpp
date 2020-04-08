/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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

#include "app/applicationexceptionhandler.h"
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
#include <QKeyEvent>
#include <QSettings>
#include <QStringList>
#include <QWidget>

#include <qt_windows.h>

#include <stdio.h>
#include <fcntl.h>
#include <io.h>

namespace {

void setBinaryFor(int fd)
{
    _setmode(fd, _O_BINARY);
}

bool isPortableVersion()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString uninstPath = appDir + "/unins000.exe";
    return !QFile::exists(uninstPath);
}

QDir configFolder(bool *isPortable)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString path = appDir + "/config";
    QDir dir(path);

    QSettings::setDefaultFormat(QSettings::IniFormat);

    *isPortable = isPortableVersion()
            && QFileInfo(appDir).isWritable()
            && dir.mkpath(".")
            && dir.isReadable()
            && QFileInfo(dir.absolutePath()).isWritable();

    return dir;
}

void migrateDirectory(const QString oldPath, const QString newPath)
{
    QDir oldDir(oldPath);
    QDir newDir(newPath);

    if ( oldDir.exists() ) {
        newDir.mkpath(".");

        for ( const auto &fileName : oldDir.entryList(QDir::Files) ) {
            const QString oldFileName = oldDir.absoluteFilePath(fileName);
            const QString newFileName = newDir.absoluteFilePath(fileName);
            COPYQ_LOG( QString("Migrating \"%1\" -> \"%2\"")
                       .arg(oldFileName)
                       .arg(newFileName) );
            QFile::copy(oldFileName, newFileName);
        }
    }
}

void migrateConfigToAppDir()
{
    // Don't use Windows registry.
    QSettings::setDefaultFormat(QSettings::IniFormat);

    bool isPortable;
    const QDir dir = configFolder(&isPortable);

    if (isPortable) {
        const QString oldConfigFileName =
                QSettings(QSettings::IniFormat, QSettings::UserScope,
                          QCoreApplication::organizationName(),
                          QCoreApplication::applicationName()).fileName();
        const QString oldConfigPath = QDir::cleanPath(oldConfigFileName + "/..");

        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.absolutePath());
        Settings newSettings;

        if ( Settings::canModifySettings && newSettings.isEmpty() ) {
            COPYQ_LOG("Migrating configuration to application directory.");
            const QString newConfigPath = QDir::cleanPath(newSettings.fileName() + "/..");

            // Migrate configuration from system directory.
            migrateDirectory(oldConfigPath, newConfigPath);

            // Migrate themes from system directory.
            migrateDirectory(oldConfigPath + "/themes", newConfigPath + "/themes");
        }
    } else if ( dir.exists() ) {
        log( QString("Ignoring configuration in \"%1\" (https://github.com/hluk/CopyQ/issues/583).")
             .arg(dir.absolutePath()), LogWarning );
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
    Application *app = new ApplicationExceptionHandler<Application>(argc, argv);
    installControlHandler();
    setBinaryFor(0);
    setBinaryFor(1);

    // Override log file for portable version.
    bool isPortable;
    const QDir dir = configFolder(&isPortable);
    if (isPortable) {
        const QString logFileName = dir.absolutePath() + "/copyq.log";
        qputenv("COPYQ_LOG_FILE", logFileName.toUtf8());
    }

    return app;
}

QApplication *createGuiApplication(int &argc, char **argv)
{
    auto app = createApplication<QApplication>(argc, argv);

    // WORKAROUND: Create a window so that application can receive
    //             WM_QUERYENDSESSION (from installer) and similar events.
    auto w = new QWidget();
    auto winId = w->winId();
    Q_UNUSED(winId);

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
            return nullptr;

        currentWindow = lastPopUp;
    }

    return nullptr;
}

bool isAltTabWindow(HWND window)
{
    if (!window || window == GetShellWindow())
        return false;

    HWND root = GetAncestor(window, GA_ROOTOWNER);

    if (getLastVisibleActivePopUpOfWindow(root) != window)
        return false;

    const QString cls = windowClass(window);
    COPYQ_LOG_VERBOSE( QString("cls: \"%1\"").arg(cls) );
    return !cls.isEmpty()
            && cls != "Shell_TrayWnd"
            && cls != "Shell_SecondaryTrayWnd"
            && cls != "Shell_CharmWindow"
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

PlatformNativeInterface *platformNativeInterface()
{
    static WinPlatform platform;
    return &platform;
}

PlatformWindowPtr WinPlatform::getWindow(WId winId)
{
    HWND window = reinterpret_cast<HWND>(winId);
    return PlatformWindowPtr( window ? new WinPlatformWindow(window) : nullptr );
}

PlatformWindowPtr WinPlatform::getCurrentWindow()
{
    currentWindow = GetForegroundWindow();
    if (!isAltTabWindow(currentWindow))
        EnumWindows(getCurrentWindowProc, 0);
    return PlatformWindowPtr( currentWindow ? new WinPlatformWindow(currentWindow) : nullptr );
}

QCoreApplication *WinPlatform::createConsoleApplication(int &argc, char **argv)
{
    return createApplication<QCoreApplication>(argc, argv);
}

QApplication *WinPlatform::createServerApplication(int &argc, char **argv)
{
    return createGuiApplication(argc, argv);
}

QGuiApplication *WinPlatform::createMonitorApplication(int &argc, char **argv)
{
    return createApplication<QGuiApplication>(argc, argv);
}

QGuiApplication *WinPlatform::createClipboardProviderApplication(int &argc, char **argv)
{
    return createApplication<QGuiApplication>(argc, argv);
}

QCoreApplication *WinPlatform::createClientApplication(int &argc, char **argv)
{
    return createApplication<QCoreApplication>(argc, argv);
}

QGuiApplication *WinPlatform::createTestApplication(int &argc, char **argv)
{
    return createApplication<QGuiApplication>(argc, argv);
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
    const int key = event.key();

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
