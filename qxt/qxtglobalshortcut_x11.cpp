#include "qxtglobalshortcut_p.h"
/****************************************************************************
** Copyright (c) 2006 - 2011, the LibQxt project.
** See the Qxt AUTHORS file for a list of authors and copyright holders.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * Neither the name of the LibQxt project nor the
**       names of its contributors may be used to endorse or promote products
**       derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
** DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
** (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
** LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
** ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
** <http://libqxt.org>  <foundation@libqxt.org>
*****************************************************************************/

#include <QX11Info>
#include <X11/Xlib.h>

static int (*original_x_errhandler)(Display* display, XErrorEvent* event);

static int qxt_x_errhandler(Display* display, XErrorEvent *event)
{
    Q_UNUSED(display);
    switch (event->error_code)
    {
        case BadAccess:
        case BadValue:
        case BadWindow:
            if (event->request_code == 33 /* X_GrabKey */ ||
                event->request_code == 34 /* X_UngrabKey */)
            {
                QxtGlobalShortcutPrivate::error = true;
                //TODO:
                //char errstr[256];
                //XGetErrorText(dpy, err->error_code, errstr, 256);
            }
        default:
            return 0;
    }
}

bool QxtGlobalShortcutPrivate::eventFilter(void* message)
{
    XEvent* event = static_cast<XEvent*>(message);
    if (event->type == KeyPress)
    {
        XKeyEvent* key = (XKeyEvent*) event;
        activateShortcut(key->keycode, 
            // Mod1Mask == Alt, Mod4Mask == Meta
            key->state & (ShiftMask | ControlMask | Mod1Mask | Mod4Mask));
    }
    return false;
}

quint32 QxtGlobalShortcutPrivate::nativeModifiers(Qt::KeyboardModifiers modifiers)
{
    // ShiftMask, LockMask, ControlMask, Mod1Mask, Mod2Mask, Mod3Mask, Mod4Mask, and Mod5Mask
    quint32 native = 0;
    if (modifiers & Qt::ShiftModifier)
        native |= ShiftMask;
    if (modifiers & Qt::ControlModifier)
        native |= ControlMask;
    if (modifiers & Qt::AltModifier)
        native |= Mod1Mask;
    if (modifiers & Qt::MetaModifier)
        native |= Mod4Mask;

    // TODO: resolve these?
    //if (modifiers & Qt::MetaModifier)
    //if (modifiers & Qt::KeypadModifier)
    //if (modifiers & Qt::GroupSwitchModifier)
    return native;
}

quint32 QxtGlobalShortcutPrivate::nativeKeycode(Qt::Key key)
{
    Display* display = QX11Info::display();
    return XKeysymToKeycode(display, XStringToKeysym(QKeySequence(key).toString().toLatin1().data()));
}

bool QxtGlobalShortcutPrivate::registerShortcut(quint32 nativeKey, quint32 nativeMods)
{
    Display* display = QX11Info::display();
    Window window = QX11Info::appRootWindow();
    Bool owner = True;
    int pointer = GrabModeAsync;
    int keyboard = GrabModeAsync;
    error = false;
    original_x_errhandler = XSetErrorHandler(qxt_x_errhandler);
    XGrabKey(display, nativeKey, nativeMods, window, owner, pointer, keyboard);
    XGrabKey(display, nativeKey, nativeMods | Mod2Mask, window, owner, pointer, keyboard); // allow numlock
    XGrabKey(display, nativeKey, nativeMods | LockMask, window, owner, pointer, keyboard);
    XGrabKey(display, nativeKey, nativeMods | Mod2Mask | LockMask, window, owner, pointer, keyboard);
    XSync(display, False);
    XSetErrorHandler(original_x_errhandler);
    return !error;
}

bool QxtGlobalShortcutPrivate::unregisterShortcut(quint32 nativeKey, quint32 nativeMods)
{
    Display* display = QX11Info::display();
    Window window = QX11Info::appRootWindow();
    error = false;
    original_x_errhandler = XSetErrorHandler(qxt_x_errhandler);
    XUngrabKey(display, nativeKey, nativeMods, window);
    XUngrabKey(display, nativeKey, nativeMods | Mod2Mask, window); // allow numlock
    XUngrabKey(display, nativeKey, nativeMods | LockMask, window);
    XUngrabKey(display, nativeKey, nativeMods | Mod2Mask | LockMask, window);
    XSync(display, False);
    XSetErrorHandler(original_x_errhandler);
    return !error;
}
