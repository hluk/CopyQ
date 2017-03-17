/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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
#include "common/textdata.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QRegExp>
#include <QStringList>
#include <QVariant>
#include <QWidget>

#include "x11platformwindow.h"
#include "x11platformclipboard.h"
#include "x11displayguard.h"

#include <X11/Xatom.h>

#include <memory>

namespace {

int (*old_xio_errhandler)(Display *) = nullptr;

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

#ifdef COPYQ_DESKTOP_PREFIX
QString getDesktopFilename()
{
    const char *path = getenv("XDG_CONFIG_HOME");
    QString filename = path ? getTextData(path) : QDir::homePath() + "/.config";
    filename.append("/autostart/" + QCoreApplication::applicationName() + ".desktop");
    return filename;
}
#endif

} // namespace

PlatformPtr createPlatformNativeInterface()
{
    return PlatformPtr(new X11Platform);
}

X11Platform::~X11Platform() = default;

PlatformWindowPtr X11Platform::getWindow(WId winId)
{
    auto d = std::make_shared<X11DisplayGuard>();
    if (!d->display())
        return PlatformWindowPtr();

    std::unique_ptr<X11PlatformWindow> window(new X11PlatformWindow(d, winId));
    return PlatformWindowPtr(window->isValid() ? window.release() : nullptr);
}

PlatformWindowPtr X11Platform::getCurrentWindow()
{
    auto d = std::make_shared<X11DisplayGuard>();
    if (!d->display())
        return PlatformWindowPtr();

    std::unique_ptr<X11PlatformWindow> window(new X11PlatformWindow(d));
    return PlatformWindowPtr(window->isValid() ? window.release() : nullptr);
}

bool X11Platform::canAutostart()
{
#ifdef COPYQ_DESKTOP_PREFIX
    return true;
#else
    return false;
#endif
}

bool X11Platform::isAutostartEnabled()
{
#ifdef COPYQ_DESKTOP_PREFIX
    const QString filename = getDesktopFilename();

    QFile desktopFile(filename);

    if ( !desktopFile.exists() )
        return false;

    if ( !desktopFile.open(QIODevice::ReadOnly | QIODevice::Text) )
        return false;

    QRegExp re("^Hidden\\s*=\\s*([a-zA-Z01]+)");

    while ( !desktopFile.atEnd() ) {
        QString line = getTextData(desktopFile.readLine());
        if ( re.indexIn(line) != -1 ) {
            QString value = re.cap(1);
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
#ifdef COPYQ_DESKTOP_PREFIX
    if ( isAutostartEnabled() == enable )
        return;

    const QString filename = getDesktopFilename();

    QFile desktopFile(filename);

    bool createUserDesktopFile = !desktopFile.exists();
    if (createUserDesktopFile) {
        const QString filename2 =
                QString(COPYQ_DESKTOP_PREFIX) + QString("/copyq.desktop");
        desktopFile.setFileName(filename2);
    }

    if ( !desktopFile.open(QIODevice::ReadOnly | QIODevice::Text) )
        return;

    QFile desktopFile2(filename + ".new");
    if ( !QDir(QDir::cleanPath(filename + "/..")).mkpath(".")
         || !desktopFile2.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text) )
    {
        return;
    }

    QRegExp re("^(Hidden|X-GNOME-Autostart-enabled)\\s*=\\s*");

    while ( !desktopFile.atEnd() ) {
        QString line = getTextData(desktopFile.readLine());
        QString cmd = "\"" + QApplication::applicationFilePath() + "\"";
        if ( line.startsWith("Exec=") ) {
            const QString sessionName = qApp->property("CopyQ_session_name").toString();
            if ( !sessionName.isEmpty() )
                cmd.append(" -s " + sessionName);
            desktopFile2.write("Exec=" + cmd.toUtf8() + "\n");
        } else if ( re.indexIn(line) == -1 ) {
            desktopFile2.write(line.toUtf8());
        }
    }

    desktopFile.close();

    desktopFile2.write("Hidden=");
    desktopFile2.write(enable ? "False" : "True");
    desktopFile2.write("\n");

    desktopFile2.write("X-GNOME-Autostart-enabled=");
    desktopFile2.write(enable ? "true" : "false");
    desktopFile2.write("\n");

    QFile::remove(filename);
    desktopFile2.rename(filename);
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
    old_xio_errhandler = XSetIOErrorHandler(copyq_xio_errhandler);
    return new ApplicationExceptionHandler<QApplication>(argc, argv);
}

QApplication *X11Platform::createMonitorApplication(int &argc, char **argv)
{
    return new ApplicationExceptionHandler<QApplication>(argc, argv);
}

QCoreApplication *X11Platform::createClientApplication(int &argc, char **argv)
{
    return new ApplicationExceptionHandler<QCoreApplication>(argc, argv);
}

PlatformClipboardPtr X11Platform::clipboard()
{
    auto d = std::make_shared<X11DisplayGuard>();
    return PlatformClipboardPtr(new X11PlatformClipboard(d));
}

QStringList X11Platform::getCommandLineArguments(int argc, char **argv)
{
    QStringList arguments;

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
    return "gedit %1";
}

QString X11Platform::translationPrefix()
{
    return QString();
}
