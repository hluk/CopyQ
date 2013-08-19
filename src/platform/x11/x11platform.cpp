/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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
#include <QDir>
#include <QRegExp>

#include <X11/extensions/XTest.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <unistd.h> // usleep()

namespace {

struct X11WindowProperty {
    X11WindowProperty(Display *display, Window w, Atom property, long longOffset,
                      long longLength, Atom reqType)
    {
        if ( XGetWindowProperty(display, w, property, longOffset, longLength, false,
                                reqType, &type, &format, &len, &remain, &data) != Success )
        {
            data = NULL;
        }
    }

    ~X11WindowProperty()
    {
        if (data != NULL)
            XFree(data);
    }

    bool isValid() const { return data != NULL; }

    Atom type;
    int format;
    unsigned long len;
    unsigned long remain;
    unsigned char *data;
};

#ifdef HAS_X11TEST
void FakeKeyEvent(Display* display, unsigned int keyCode, Bool isPress)
{
    XTestFakeKeyEvent(display, keyCode, isPress, CurrentTime);
    XSync(display, False);
    usleep(6000);
}

void simulateModifierKeyPress(Display *display, const QList<int> &modCodes, Bool keyDown)
{
    foreach (int modCode, modCodes) {
        KeyCode keyCode = XKeysymToKeycode(display, modCode);
        FakeKeyEvent(display, keyCode, keyDown);
    }
}

bool isPressed(KeyCode keyCode, const char keyMap[32])
{
    return ((keyMap[keyCode >> 3] >> (keyCode & 7)) & 1)
            || ((keyMap[keyCode >> 3] >> (keyCode & 7)) & 1);
}

void simulateKeyPress(Display *display, const QList<int> &modCodes, unsigned int key)
{
    // Find modifiers to release.
    static QList<int> mods = QList<int>()
            << XK_Shift_L << XK_Shift_R
            << XK_Control_L << XK_Control_R
            << XK_Meta_L << XK_Meta_R
            << XK_Alt_L << XK_Alt_R
            << XK_Super_L << XK_Super_R
            << XK_Hyper_L << XK_Hyper_R;

    char keyMap[32];
    XQueryKeymap(display, keyMap);

    QList<KeyCode> modsToRelease;
    foreach (int mod, mods) {
        if ( isPressed(XKeysymToKeycode(display, mod), keyMap) )
            modsToRelease << XKeysymToKeycode(display, mod);
    }

    // Release currently pressed modifiers.
    foreach (KeyCode mod, modsToRelease)
        FakeKeyEvent(display, mod, False);

    simulateModifierKeyPress(display, modCodes, True);

    KeyCode keyCode = XKeysymToKeycode(display, key);

    FakeKeyEvent(display, keyCode, True);
    FakeKeyEvent(display, keyCode, False);

    simulateModifierKeyPress(display, modCodes, False);

    // Press modifiers again.
    foreach (KeyCode mod, modsToRelease)
        FakeKeyEvent(display, mod, True);

    XSync(display, False);
}
#else

void simulateKeyPress(Display *display, Window window, unsigned int modifiers, unsigned int key)
{
    XKeyEvent event;
    XEvent *xev = reinterpret_cast<XEvent *>(&event);
    event.display     = display;
    event.window      = window;
    event.root        = DefaultRootWindow(display);
    event.subwindow   = None;
    event.time        = CurrentTime;
    event.x           = 1;
    event.y           = 1;
    event.x_root      = 1;
    event.y_root      = 1;
    event.same_screen = True;
    event.keycode     = XKeysymToKeycode(display, key);
    event.state       = modifiers;

    event.type = KeyPress;
    XSendEvent(display, window, True, KeyPressMask, xev);
    XSync(display, False);

    event.type = KeyRelease;
    XSendEvent(display, window, True, KeyPressMask, xev);
    XSync(display, False);
}
#endif

#ifdef COPYQ_DESKTOP_PREFIX
QString getDesktopFilename()
{
    QString filename;

    const char *path = getenv("XDG_CONFIG_HOME");
    if (path == NULL) {
        filename = QDir::homePath() + QDir::separator() + ".config";
    } else {
        filename.fromLocal8Bit(path);
    }

    filename.append( QDir::separator() );
    filename.append("autostart/copyq.desktop");

    return filename;
}
#endif

} // namespace

PlatformPtr createPlatformNativeInterface()
{
    return PlatformPtr(new X11Platform);
}

class X11PlatformPrivate
{
public:
    Display *display;
};

X11Platform::X11Platform()
    : d(new X11PlatformPrivate)
{
    d->display = XOpenDisplay(NULL);
}

X11Platform::~X11Platform()
{
    if (d->display != NULL)
        XCloseDisplay(d->display);
    delete d;
}

WId X11Platform::getCurrentWindow()
{
    if (d->display == NULL)
        return 0L;

    XSync(d->display, False);

    static Atom atomWindow = XInternAtom(d->display, "_NET_ACTIVE_WINDOW", true);

    X11WindowProperty property(d->display, DefaultRootWindow(d->display), atomWindow, 0l, 1l,
                               XA_WINDOW);

    if ( property.isValid() && property.type == XA_WINDOW && property.format == 32 &&
         property.len == 1) {
            return *reinterpret_cast<Window *>(property.data);
    }

    return 0L;
}

QString X11Platform::getWindowTitle(WId wid)
{
    if (d->display == NULL || wid == 0L)
        return QString();

    static Atom atomName = XInternAtom(d->display, "_NET_WM_NAME", false);
    static Atom atomUTF8 = XInternAtom(d->display, "UTF8_STRING", false);

    X11WindowProperty property(d->display, wid, atomName, 0, (~0L), atomUTF8);
    if ( property.isValid() ) {
        QByteArray result(reinterpret_cast<const char *>(property.data), property.len);
        return QString::fromUtf8(result);
    }

    return QString();
}

void X11Platform::raiseWindow(WId wid)
{
    if (d->display == NULL || wid == 0L)
        return;

    XEvent e;
    memset(&e, 0, sizeof(e));
    e.type = ClientMessage;
    e.xclient.display = d->display;
    e.xclient.window = wid;
    e.xclient.message_type = XInternAtom(d->display, "_NET_ACTIVE_WINDOW", False);
    e.xclient.format = 32;
    e.xclient.data.l[0] = 2;
    e.xclient.data.l[1] = CurrentTime;
    e.xclient.data.l[2] = 0;
    e.xclient.data.l[3] = 0;
    e.xclient.data.l[4] = 0;

    XWindowAttributes wattr;
    XGetWindowAttributes(d->display, wid, &wattr);

    if (wattr.map_state == IsViewable) {
        XSendEvent(d->display, wattr.screen->root, False,
                   SubstructureNotifyMask | SubstructureRedirectMask,
                   &e);

        XRaiseWindow(d->display, wid);
        XSetInputFocus(d->display, wid, RevertToPointerRoot, CurrentTime);
    }
}

void X11Platform::pasteToWindow(WId wid)
{
    if (d->display == NULL || wid == 0L)
        return;

    raiseWindow(wid);
    usleep(150000);

#ifdef HAS_X11TEST
    simulateKeyPress(d->display, QList<int>() << XK_Shift_L, XK_Insert);
#else
    simulateKeyPress(d->display, wid, ShiftMask, XK_Insert);
#endif

    // Don't do anything hasty until the content is actually pasted.
    usleep(150000);
}

WId X11Platform::getPasteWindow()
{
    return getCurrentWindow();
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
                QString(COPYQ_DESKTOP_PREFIX) + QDir::separator() + QString("copyq.desktop");
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
        if ( line.startsWith("Exec=") && line.endsWith("copyq\n") ) {
            desktopFile2.write("Exec=");
            desktopFile2.write( QApplication::applicationFilePath().toUtf8() );
            desktopFile2.write("\n");
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
    if (d->display == NULL)
        return false;

    XEvent event;
    XQueryPointer(d->display, DefaultRootWindow(d->display),
                  &event.xbutton.root, &event.xbutton.window,
                  &event.xbutton.x_root, &event.xbutton.y_root,
                  &event.xbutton.x, &event.xbutton.y,
                  &event.xbutton.state);

    return event.xbutton.state & (Button1Mask | ShiftMask);
}

bool X11Platform::isClipboardEmpty() const
{
    if (d->display == NULL)
        return false;

    static Atom atom = XInternAtom(d->display, "CLIPBOARD", False);
    return XGetSelectionOwner(d->display, atom) == None;
}

bool X11Platform::isSelectionEmpty() const
{
    if (d->display == NULL)
        return false;

    static Atom atom = XA_PRIMARY;
    return XGetSelectionOwner(d->display, atom) == None;
}
