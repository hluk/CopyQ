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

#include "x11platform.h"

#include "app/applicationexceptionhandler.h"
#include "common/log.h"
#include "common/textdata.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QRegularExpression>
#include <QStringList>
#include <QVariant>
#include <QWidget>
#include <QX11Info>

#include "x11platformwindow.h"
#include "x11platformclipboard.h"

#include <X11/Xatom.h>

#include <memory>

namespace {

int (*old_xio_errhandler)(Display *) = nullptr;

const char *defaultDesktopFileContent =
R"([Desktop Entry]
Name=CopyQ
Icon=copyq
GenericName=Clipboard Manager
Type=Application
Terminal=false
X-KDE-autostart-after=panel
X-KDE-StartupNotify=false
X-KDE-UniqueApplet=true
)";

// Try to handle X11 fatal error gracefully.
int copyq_xio_errhandler(Display *display)
{
    // Try to call MainWindow::saveTabs().
    if ( QCoreApplication::instance() ) {
        for ( auto obj : qApp->topLevelWidgets() ) {
            if (obj->objectName() == "MainWindow") {
                QMetaObject::invokeMethod(obj, "saveTabs");
                break;
            }
        }
    }

    // Call the old handler (possibly for Qt).
    if (old_xio_errhandler)
        old_xio_errhandler(display);

    // As documentation for XSetIOErrorHandler states, this function should not return.
    exit(1);
}

#ifdef COPYQ_DESKTOP_FILE
QString getDesktopFilename()
{
    const char *path = getenv("XDG_CONFIG_HOME");
    QString filename = path ? getTextData(path) : QDir::homePath() + "/.config";
    filename.append("/autostart/" + QCoreApplication::applicationName() + ".desktop");
    return filename;
}
#endif

void printFileError(const QFile &file, const char *message, LogLevel logLevel = LogError)
{
    log( QString("%1 \"%2\": %3")
         .arg(message)
         .arg(file.fileName())
         .arg(file.errorString()), logLevel );
}

void maybePrintFileError(const QFile &file, const char *message)
{
    if (file.error() != QFile::NoError)
        printFileError(file, message);
}

} // namespace

PlatformNativeInterface *platformNativeInterface()
{
    static X11Platform platform;
    return &platform;
}

X11Platform::~X11Platform() = default;

PlatformWindowPtr X11Platform::getWindow(WId winId)
{
    if (!QX11Info::isPlatformX11())
        return PlatformWindowPtr();

    std::unique_ptr<X11PlatformWindow> window(new X11PlatformWindow(winId));
    return PlatformWindowPtr(window->isValid() ? window.release() : nullptr);
}

PlatformWindowPtr X11Platform::getCurrentWindow()
{
    if (!QX11Info::isPlatformX11())
        return PlatformWindowPtr();

    std::unique_ptr<X11PlatformWindow> window(new X11PlatformWindow());
    return PlatformWindowPtr(window->isValid() ? window.release() : nullptr);
}

bool X11Platform::canAutostart()
{
#ifdef COPYQ_DESKTOP_FILE
    return true;
#else
    return false;
#endif
}

bool X11Platform::isAutostartEnabled()
{
#ifdef COPYQ_DESKTOP_FILE
    const QString filename = getDesktopFilename();

    QFile desktopFile(filename);

    if ( !desktopFile.exists() )
        return false;

    if ( !desktopFile.open(QIODevice::ReadOnly | QIODevice::Text) )
        return false;

    const QRegularExpression re("^Hidden\\s*=\\s*([a-zA-Z01]+)");

    while ( !desktopFile.atEnd() ) {
        const QString line = getTextData(desktopFile.readLine());
        const auto m = re.match(line);
        if (m.hasMatch()) {
            const QString value = m.captured(1);
            return !(value.startsWith("True") || value.startsWith("true") || value.startsWith("0"));
        }
    }

    return true;
#else
    return false;
#endif
}

void X11Platform::setAutostartEnabled(bool enable)
{
#ifdef COPYQ_DESKTOP_FILE
    if ( isAutostartEnabled() == enable )
        return;

    const QString filename = getDesktopFilename();

    const auto autostartPath = QDir::cleanPath(filename + "/..");
    QDir autostartDir(autostartPath);
    if ( !autostartDir.mkpath(".") ) {
        log( QString("Failed to create autostart path \"%1\"").arg(autostartPath) );
        return;
    }

    QFile desktopFile2(filename + ".new");
    if ( !desktopFile2.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text) ) {
        printFileError(desktopFile2, "Failed to create new desktop file");
        return;
    }

    const QRegularExpression re("^(Hidden|X-GNOME-Autostart-enabled|Exec)\\s*=\\s*");

    QFile desktopFile(filename);
    bool createUserDesktopFile = !desktopFile.exists();
    if (createUserDesktopFile)
        desktopFile.setFileName(COPYQ_DESKTOP_FILE);

    if ( desktopFile.open(QIODevice::ReadOnly | QIODevice::Text) ) {
        while ( !desktopFile.atEnd() ) {
            const QString line = getTextData(desktopFile.readLine());
            if ( !line.contains(re) )
                desktopFile2.write(line.toUtf8());
        }
        desktopFile.close();
        maybePrintFileError(desktopFile, "Failed to read desktop file");
    } else {
        // Installed desktop file not found (can happen when running tests).
        printFileError(desktopFile, "Failed to open desktop file", LogNote);
        desktopFile2.write(defaultDesktopFileContent);
    }

#ifdef COPYQ_AUTOSTART_COMMAND
    QString cmd = COPYQ_AUTOSTART_COMMAND;
#else
    QString cmd = "\"" + QApplication::applicationFilePath() + "\"";
#endif
    const QString sessionName = qApp->property("CopyQ_session_name").toString();
    if ( !sessionName.isEmpty() )
        cmd.append(" -s " + sessionName);
    desktopFile2.write("Exec=" + cmd.toUtf8() + "\n");

    desktopFile2.write("Hidden=");
    desktopFile2.write(enable ? "False" : "True");
    desktopFile2.write("\n");

    desktopFile2.write("X-GNOME-Autostart-enabled=");
    desktopFile2.write(enable ? "true" : "false");
    desktopFile2.write("\n");

    QFile::remove(filename);
    desktopFile2.rename(filename);

    maybePrintFileError(desktopFile2, "Failed to write desktop file");
#else
    Q_UNUSED(enable);
#endif
}

QCoreApplication *X11Platform::createConsoleApplication(int &argc, char **argv)
{
    return new ApplicationExceptionHandler<QCoreApplication>(argc, argv);
}

QApplication *X11Platform::createServerApplication(int &argc, char **argv)
{
    if (QX11Info::isPlatformX11())
        old_xio_errhandler = XSetIOErrorHandler(copyq_xio_errhandler);
    return new ApplicationExceptionHandler<QApplication>(argc, argv);
}

QGuiApplication *X11Platform::createMonitorApplication(int &argc, char **argv)
{
    return new ApplicationExceptionHandler<QGuiApplication>(argc, argv);
}

QGuiApplication *X11Platform::createClipboardProviderApplication(int &argc, char **argv)
{
    return new ApplicationExceptionHandler<QGuiApplication>(argc, argv);
}

QCoreApplication *X11Platform::createClientApplication(int &argc, char **argv)
{
    return new ApplicationExceptionHandler<QCoreApplication>(argc, argv);
}

QGuiApplication *X11Platform::createTestApplication(int &argc, char **argv)
{
    return new ApplicationExceptionHandler<QGuiApplication>(argc, argv);
}

PlatformClipboardPtr X11Platform::clipboard()
{
    return PlatformClipboardPtr(new X11PlatformClipboard());
}

QStringList X11Platform::getCommandLineArguments(int argc, char **argv)
{
    if (argc == 0)
        return QStringList();

    QStringList arguments;
    arguments.reserve(argc - 1);

    for (int i = 1; i < argc; ++i)
        arguments.append( QString::fromUtf8(argv[i]) );

    return arguments;
}

bool X11Platform::findPluginDir(QDir *pluginsDir)
{
    pluginsDir->setPath( qApp->applicationDirPath() );

    if ( pluginsDir->dirName() == QString("bin")
         && pluginsDir->cdUp()
         && (pluginsDir->cd("lib64") || pluginsDir->cd("lib"))
         && pluginsDir->cd("copyq") )
    {
        // OK, installed in /usr/local/bin or /usr/bin.
        return true;
    }

    pluginsDir->setPath( qApp->applicationDirPath() );

    if ( pluginsDir->cd("plugins") ) {
        // OK, plugins in same directory as executable.
        pluginsDir->cd("copyq");
        return true;
    }

    return false;
}

QString X11Platform::defaultEditorCommand()
{
    return "gedit --standalone -- %1";
}

QString X11Platform::translationPrefix()
{
    return QString();
}

void sendDummyX11Event()
{
    if (!QX11Info::isPlatformX11())
        return;

    auto display = QX11Info::display();
    auto black = BlackPixel(display, 0);
    Window window = XCreateSimpleWindow(
        display, RootWindow(display, 0), -100000, -100000, 1, 1, 0, black, black);
    XDestroyWindow(display, window);
    XFlush(display);
}
