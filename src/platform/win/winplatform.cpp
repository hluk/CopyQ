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
#include <QMetaObject>
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

QString portableConfigFolder()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    if ( !QFileInfo(appDir).isWritable() )
        return {};

    const QString uninstPath = appDir + QLatin1String("/unins000.exe");
    if ( QFile::exists(uninstPath) )
        return {};

    const QString path = appDir + QLatin1String("/config");
    QDir dir(path);

    if ( !dir.mkpath(".") || !dir.isReadable() )
        return {};

    const QString fullPath = dir.absolutePath();
    if ( !QFileInfo(fullPath).isWritable() )
        return {};

    return fullPath;
}

void uninstallControlHandler();

BOOL appQuit()
{
    uninstallControlHandler();
    const bool invoked = QMetaObject::invokeMethod(
        QCoreApplication::instance(), "quit", Qt::BlockingQueuedConnection);
    if (!invoked) {
        log("Failed to request application exit", LogError);
        return FALSE;
    }
    ExitProcess(EXIT_SUCCESS);
    return TRUE;
}

BOOL ctrlHandler(DWORD fdwCtrlType)
{
    switch (fdwCtrlType) {
    case CTRL_C_EVENT:
        log("Terminating application on signal.");
        return appQuit();

    case CTRL_CLOSE_EVENT:
        log("Terminating application on close event.");
        return appQuit();

    case CTRL_BREAK_EVENT:
        log("Terminating application on break event.");
        return appQuit();

    case CTRL_LOGOFF_EVENT:
        log("Terminating application on log off.");
        return appQuit();

    case CTRL_SHUTDOWN_EVENT:
        log("Terminating application on shut down.");
        return appQuit();

    default:
        return FALSE;
    }
}

void installControlHandler()
{
    if ( !SetConsoleCtrlHandler(reinterpret_cast<PHANDLER_ROUTINE>(ctrlHandler), TRUE) )
        log("Failed to set Windows control handler.", LogError);
}

void uninstallControlHandler()
{
    SetConsoleCtrlHandler(reinterpret_cast<PHANDLER_ROUTINE>(ctrlHandler), FALSE);
}

template <typename Application>
Application *createApplication(int &argc, char **argv)
{
    Application *app = new ApplicationExceptionHandler<Application>(argc, argv);
    installControlHandler();
    setBinaryFor(0);
    setBinaryFor(1);

    // Don't use Windows registry.
    QSettings::setDefaultFormat(QSettings::IniFormat);

    // Use config and log file in portable app folder.
    const QString portableFolder = portableConfigFolder();
    if ( !portableFolder.isEmpty() ) {
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, portableFolder);
        qputenv("COPYQ_LOG_FILE", portableFolder.toUtf8() + "/copyq.log");
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
    Q_UNUSED(winId)

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
