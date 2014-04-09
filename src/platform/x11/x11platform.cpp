/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QRegExp>
#include <QVariant>
#include <QWidget>

#include "x11platformwindow.h"
#include "x11displayguard.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>

namespace {

int (*old_xio_errhandler)(Display *) = NULL;

// Try to handle X11 fatal error gracefully.
int copyq_xio_errhandler(Display *display)
{
    // Try to call MainWindow::saveTabs().
    if ( QCoreApplication::instance() ) {
        foreach ( QWidget *obj, qApp->topLevelWidgets() ) {
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

    return 0;
}

#ifdef COPYQ_DESKTOP_PREFIX
QString getDesktopFilename()
{
    const char *path = getenv("XDG_CONFIG_HOME");
    QString filename = path ? QString::fromUtf8(path) : QDir::homePath() + "/.config";
    filename.append("/autostart/" + QCoreApplication::applicationName() + ".desktop");
    return filename;
}
#endif

} // namespace

PlatformPtr createPlatformNativeInterface()
{
    return PlatformPtr(new X11Platform);
}

X11Platform::X11Platform()
    : d(new X11DisplayGuard)
{
}

X11Platform::~X11Platform()
{
}

PlatformWindowPtr X11Platform::getWindow(WId winId)
{
    QScopedPointer<X11PlatformWindow> window(new X11PlatformWindow(*d, winId));
    return PlatformWindowPtr(window->isValid() ? window.take() : NULL);
}

PlatformWindowPtr X11Platform::getCurrentWindow()
{
    if (!d->display())
        return PlatformWindowPtr();

    QScopedPointer<X11PlatformWindow> window(new X11PlatformWindow(*d));
    return PlatformWindowPtr(window->isValid() ? window.take() : NULL);
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
        QString line = QString::fromUtf8(desktopFile.readLine());
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

    QRegExp re("^Hidden\\s*=\\s*");

    while ( !desktopFile.atEnd() ) {
        QString line = QString::fromUtf8(desktopFile.readLine());
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

    QFile::remove(filename);
    desktopFile2.rename(filename);
#else
    Q_UNUSED(enable);
#endif
}

bool X11Platform::isSelecting()
{
    // If mouse button or shift is pressed then assume that user is selecting text.
    if (!d->display())
        return false;

    XEvent event;
    XQueryPointer(d->display(), DefaultRootWindow(d->display()),
                  &event.xbutton.root, &event.xbutton.window,
                  &event.xbutton.x_root, &event.xbutton.y_root,
                  &event.xbutton.x, &event.xbutton.y,
                  &event.xbutton.state);

    return event.xbutton.state & (Button1Mask | ShiftMask);
}

bool X11Platform::isClipboardEmpty() const
{
    if (!d->display())
        return false;

    static Atom atom = XInternAtom(d->display(), "CLIPBOARD", False);
    return XGetSelectionOwner(d->display(), atom) == None;
}

bool X11Platform::isSelectionEmpty() const
{
    if (!d->display())
        return false;

    static Atom atom = XA_PRIMARY;
    return XGetSelectionOwner(d->display(), atom) == None;
}

QApplication *X11Platform::createServerApplication(int &argc, char **argv)
{
    old_xio_errhandler = XSetIOErrorHandler(copyq_xio_errhandler);
    return new QApplication(argc, argv);
}

QApplication *X11Platform::createMonitorApplication(int &argc, char **argv)
{
    return new QApplication(argc, argv);
}

QCoreApplication *X11Platform::createClientApplication(int &argc, char **argv)
{
    return new QCoreApplication(argc, argv);
}
