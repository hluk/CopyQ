// SPDX-License-Identifier: GPL-3.0-or-later
#include "x11platformwindow.h"

#include "x11info.h"

#include "common/appconfig.h"
#include "common/log.h"
#include "common/sleeptimer.h"
#include "gui/clipboardspy.h"
#include "platform/platformcommon.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

#ifdef HAS_X11TEST
#   include <X11/extensions/XTest.h>
#endif

#include <unistd.h> // usleep()

namespace {

class KeyPressTester final {
public:
    explicit KeyPressTester(Display *display)
        : m_display(display)
    {
        XQueryKeymap(m_display, m_keyMap);
    }

    bool isPressed(KeySym key) const
    {
        const KeyCode keyCode = XKeysymToKeycode(m_display, key);
        return (m_keyMap[keyCode >> 3] >> (keyCode & 7)) & 1;
    }

private:
    Display *m_display;
    char m_keyMap[32]{};
};

#ifdef HAS_X11TEST
void fakeKeyEvent(Display* display, unsigned int keyCode, Bool isPress, unsigned long delayMs = CurrentTime)
{
    XTestFakeKeyEvent(display, keyCode, isPress, delayMs);
    XSync(display, False);
}

void simulateModifierKeyPress(Display *display, const QList<int> &modCodes, Bool keyDown)
{
    for (int modCode : modCodes) {
        const auto keySym = static_cast<KeySym>(modCode);
        KeyCode keyCode = XKeysymToKeycode(display, keySym);
        fakeKeyEvent(display, keyCode, keyDown);
    }
}

bool isModifierPressed(Display *display)
{
    KeyPressTester tester(display);
    return tester.isPressed(XK_Shift_L)
        || tester.isPressed(XK_Shift_R)
        || tester.isPressed(XK_Control_L)
        || tester.isPressed(XK_Control_R)
        || tester.isPressed(XK_Meta_L)
        || tester.isPressed(XK_Meta_R)
        || tester.isPressed(XK_Alt_L)
        || tester.isPressed(XK_Alt_R)
        || tester.isPressed(XK_Super_L)
        || tester.isPressed(XK_Super_R)
        || tester.isPressed(XK_Hyper_L)
        || tester.isPressed(XK_Hyper_R);
}

bool waitForModifiersReleased(Display *display, const AppConfig &config)
{
    const int maxWaitForModsReleaseMs = config.option<Config::window_wait_for_modifier_released_ms>();
    if (maxWaitForModsReleaseMs >= 0) {
        SleepTimer t(maxWaitForModsReleaseMs);
        while (t.sleep()) {
            if (!isModifierPressed(display))
                return true;
        }
    }

    return !isModifierPressed(display);
}

void simulateKeyPress(Display *display, const QList<int> &modCodes, unsigned int key, const AppConfig &config)
{
    // Wait for user to release modifiers.
    if (!waitForModifiersReleased(display, config))
        return;

    simulateModifierKeyPress(display, modCodes, True);

    const KeyCode keyCode = XKeysymToKeycode(display, key);

    fakeKeyEvent(display, keyCode, True);
    // This is needed to paste into URL bar in Chrome.
    const unsigned long delayMs = config.option<Config::window_key_press_time_ms>();
    fakeKeyEvent(display, keyCode, False, delayMs);

    simulateModifierKeyPress(display, modCodes, False);

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

class X11WindowProperty final {
public:
    X11WindowProperty(Display *display, Window w, Atom property, long longOffset,
                      long longLength, Atom reqType)
    {
        if ( XGetWindowProperty(display, w, property, longOffset, longLength, false,
                                reqType, &type, &format, &len, &remain, &data) != Success )
        {
            data = nullptr;
        }
    }

    ~X11WindowProperty()
    {
        if (data != nullptr)
            XFree(data);
    }

    bool isValid() const { return data != nullptr; }

    X11WindowProperty(const X11WindowProperty &) = delete;
    X11WindowProperty &operator=(const X11WindowProperty &) = delete;

    Atom type{};
    int format{};
    unsigned long len{};
    unsigned long remain{};
    unsigned char *data;
};

Window getCurrentWindow()
{
    if (!X11Info::isPlatformX11())
        return 0L;

    auto display = X11Info::display();
    if (!display)
        return 0L;

    XSync(display, False);

    static Atom atomWindow = XInternAtom(display, "_NET_ACTIVE_WINDOW", true);

    X11WindowProperty property(display, DefaultRootWindow(display), atomWindow, 0l, 1l, XA_WINDOW);

    if ( property.isValid() && property.type == XA_WINDOW && property.format == 32 && property.len == 1)
        return *reinterpret_cast<Window *>(property.data);

    return 0L;
}

} // namespace


X11PlatformWindow::X11PlatformWindow()
    : m_window(getCurrentWindow())
{
}

X11PlatformWindow::X11PlatformWindow(quintptr winId)
    : m_window(winId)
{
}

QString X11PlatformWindow::getTitle()
{
    Q_ASSERT( isValid() );

    if (!X11Info::isPlatformX11())
        return QString();

    auto display = X11Info::display();
    if (!display)
        return QString();

    static Atom atomName = XInternAtom(display, "_NET_WM_NAME", false);
    static Atom atomUTF8 = XInternAtom(display, "UTF8_STRING", false);

    X11WindowProperty property(display, m_window, atomName, 0, (~0L), atomUTF8);
    if ( property.isValid() ) {
        const auto len = static_cast<int>(property.len);
        QByteArray result(reinterpret_cast<const char *>(property.data), len);
        return QString::fromUtf8(result);
    }

    return QString();
}

void X11PlatformWindow::raise()
{
    Q_ASSERT( isValid() );

    if (!X11Info::isPlatformX11())
        return;

    auto display = X11Info::display();
    if (!display)
        return;

    COPYQ_LOG( QString("Raising window \"%1\"").arg(getTitle()) );

    XClientMessageEvent e{};
    memset(&e, 0, sizeof(e));
    e.type = ClientMessage;
    e.display = display;
    e.window = m_window;
    e.message_type = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
    e.format = 32;
    e.data.l[0] = 2;
    e.data.l[1] = CurrentTime;
    e.data.l[2] = 0;
    e.data.l[3] = 0;
    e.data.l[4] = 0;

    XWindowAttributes wattr{};
    XGetWindowAttributes(display, m_window, &wattr);

    if (wattr.map_state == IsViewable) {
        XSendEvent(display, wattr.screen->root, False,
                   SubstructureNotifyMask | SubstructureRedirectMask,
                   reinterpret_cast<XEvent*>(&e));
        XSync(display, False);
        XRaiseWindow(display, m_window);
        XSetInputFocus(display, m_window, RevertToPointerRoot, CurrentTime);
        XSync(display, False);
    }
}

void X11PlatformWindow::pasteClipboard()
{
    const AppConfig config;
    if ( pasteWithCtrlV(*this, config) )
        sendKeyPress(XK_Control_L, XK_V, config);
    else
        sendKeyPress(XK_Shift_L, XK_Insert, config);
}

void X11PlatformWindow::copy()
{
    const AppConfig config;
    ClipboardSpy spy(ClipboardMode::Clipboard, QByteArray());
    sendKeyPress(XK_Control_L, XK_C, config);
    spy.wait();
}

bool X11PlatformWindow::isValid() const
{
    return m_window != 0L;
}

bool X11PlatformWindow::waitForFocus(int ms)
{
    Q_ASSERT( isValid() );

    if (ms >= 0) {
        SleepTimer t(ms);
        while (t.sleep()) {
            const auto currentWindow = getCurrentWindow();
            if (currentWindow == m_window)
                return true;
        }
    }

    return m_window == getCurrentWindow();
}

void X11PlatformWindow::sendKeyPress(int modifier, int key, const AppConfig &config)
{
    Q_ASSERT( isValid() );

    if ( !waitForFocus(config.option<Config::window_wait_before_raise_ms>()) ) {
        raise();
        if ( !waitForFocus(config.option<Config::window_wait_raised_ms>()) ) {
            COPYQ_LOG( QString("Failed to focus window \"%1\"").arg(getTitle()) );
            return;
        }
    }

    waitMs(config.option<Config::window_wait_after_raised_ms>());

    if (!X11Info::isPlatformX11())
        return;

    auto display = X11Info::display();
    if (!display)
        return;

#ifdef HAS_X11TEST
    simulateKeyPress(display, QList<int>() << modifier, static_cast<uint>(key), config);
#else
    const int modifierMask = (modifier == XK_Control_L) ? ControlMask : ShiftMask;
    simulateKeyPress(display, m_window, modifierMask, key);
#endif
}
