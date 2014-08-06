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

#include "clipboardspy.h"
#include "platform/platformcommon.h"
#include "x11platformwindow.h"
#include "x11displayguard.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

#ifdef HAS_X11TEST
#   include <X11/extensions/XTest.h>
#endif

#include <unistd.h> // usleep()

namespace {

#ifdef HAS_X11TEST
void FakeKeyEvent(Display* display, unsigned int keyCode, Bool isPress)
{
    XTestFakeKeyEvent(display, keyCode, isPress, CurrentTime);
    XSync(display, False);
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
    return (keyMap[keyCode >> 3] >> (keyCode & 7)) & 1;
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

Window getCurrentWindow(Display *display)
{
    Q_ASSERT(display);

    XSync(display, False);

    static Atom atomWindow = XInternAtom(display, "_NET_ACTIVE_WINDOW", true);

    X11WindowProperty property(display, DefaultRootWindow(display), atomWindow, 0l, 1l, XA_WINDOW);

    if ( property.isValid() && property.type == XA_WINDOW && property.format == 32 && property.len == 1)
        return *reinterpret_cast<Window *>(property.data);

    return 0L;
}

} // namespace


X11PlatformWindow::X11PlatformWindow(X11DisplayGuard &d)
    : m_window(getCurrentWindow(d.display()))
    , d(d)
{
    Q_ASSERT(d.display());
}

X11PlatformWindow::X11PlatformWindow(X11DisplayGuard &d, Window winId)
    : m_window(winId)
    , d(d)
{
    Q_ASSERT(d.display());
}

QString X11PlatformWindow::getTitle()
{
    Q_ASSERT( isValid() );

    static Atom atomName = XInternAtom(d.display(), "_NET_WM_NAME", false);
    static Atom atomUTF8 = XInternAtom(d.display(), "UTF8_STRING", false);

    X11WindowProperty property(d.display(), m_window, atomName, 0, (~0L), atomUTF8);
    if ( property.isValid() ) {
        QByteArray result(reinterpret_cast<const char *>(property.data), property.len);
        return QString::fromUtf8(result);
    }

    return QString();
}

void X11PlatformWindow::raise()
{
    Q_ASSERT( isValid() );

    XEvent e;
    memset(&e, 0, sizeof(e));
    e.type = ClientMessage;
    e.xclient.display = d.display();
    e.xclient.window = m_window;
    e.xclient.message_type = XInternAtom(d.display(), "_NET_ACTIVE_WINDOW", False);
    e.xclient.format = 32;
    e.xclient.data.l[0] = 2;
    e.xclient.data.l[1] = CurrentTime;
    e.xclient.data.l[2] = 0;
    e.xclient.data.l[3] = 0;
    e.xclient.data.l[4] = 0;

    XWindowAttributes wattr;
    XGetWindowAttributes(d.display(), m_window, &wattr);

    if (wattr.map_state == IsViewable) {
        XSendEvent(d.display(), wattr.screen->root, False,
                   SubstructureNotifyMask | SubstructureRedirectMask,
                   &e);
        XSync(d.display(), False);
        XRaiseWindow(d.display(), m_window);
        XSetInputFocus(d.display(), m_window, RevertToPointerRoot, CurrentTime);
    }
}

void X11PlatformWindow::pasteClipboard()
{
    if ( pasteWithCtrlV(*this) )
        sendKeyPress(XK_Control_L, XK_V);
    else
        sendKeyPress(XK_Shift_L, XK_Insert);

    // Don't do anything hasty until the content is actually pasted.
    usleep(150000);
}

void X11PlatformWindow::copy()
{
    ClipboardSpy spy;
    sendKeyPress(XK_Control_L, XK_C);
    spy.wait(2000);
}

bool X11PlatformWindow::isValid() const
{
    return m_window != 0L;
}

void X11PlatformWindow::sendKeyPress(int modifier, int key)
{
    Q_ASSERT( isValid() );

    raise();
    usleep(150000);

    if (getCurrentWindow(d.display()) != m_window)
        return;

#ifdef HAS_X11TEST
    simulateKeyPress(d.display(), QList<int>() << modifier, key);
#else
    const int modifierMask = (modifier == XK_Control_L) ? ControlMask : ShiftMask;
    simulateKeyPress(d.display(), m_window, modifierMask, key);
#endif
}
