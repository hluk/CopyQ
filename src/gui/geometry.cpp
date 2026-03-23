// SPDX-License-Identifier: GPL-3.0-or-later

#include "geometry.h"

#include "common/config.h"
#include "gui/screen.h"

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QScreen>
#include <QSettings>
#include <QVariant>
#include <QWidget>
#include <QWindow>

#ifdef COPYQ_MOVE_TO_WORKSPACE
#include "platform/x11/x11info.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif

#define GEOMETRY_LOG(window) \
    qCDebug(geometryCategory) << "Window \"" << window->objectName() << "\": "

namespace {

Q_DECLARE_LOGGING_CATEGORY(geometryCategory)
Q_LOGGING_CATEGORY(geometryCategory, "copyq.geometry")

const char propertyGeometryLockedUntilHide[] = "CopyQ_geometry_locked_until_hide";

constexpr int windowMinWidth = 50;
constexpr int windowMinHeight = 50;

QSize frameSize(QWidget *widget) {
    const QRect frame = widget->frameGeometry();
    const QSize size = widget->size();
    const int w = qMax(windowMinWidth, qMax(frame.width(), size.width()));
    const int h = qMax(windowMinHeight, qMax(frame.height(), size.height()));
    return QSize(w, h);
}

QString toString(const QRect &geometry)
{
    return QStringLiteral("%1x%2,%3,%4")
            .arg(geometry.width())
            .arg(geometry.height())
            .arg(geometry.x())
            .arg(geometry.y());
}

int screenNumber(const QWidget &widget)
{
    QWindow *windowHandle = widget.windowHandle();
    if (windowHandle) {
        QScreen *screen = windowHandle->screen();
        if (screen)
            return QGuiApplication::screens().indexOf(screen);
    }

    return -1;
}

QString geometryOptionName(const QWidget &widget)
{
    return QStringLiteral("Options/%1_geometry").arg(widget.objectName());
}

QString geometryOptionName(const QWidget &widget, bool openOnCurrentScreen)
{
    const QString baseGeometryName = geometryOptionName(widget);

    if (!openOnCurrentScreen)
        return QStringLiteral("%1_global").arg(baseGeometryName);

    const int n = screenNumber(widget);
    if (n > 0)
        return QStringLiteral("%1_screen_%2").arg(baseGeometryName).arg(n);

    return baseGeometryName;
}

QString getGeometryConfigurationFilePath()
{
    return getConfigurationFilePath("_geometry.ini");
}

QString resolutionTagForScreen(int i)
{
    const QRect screenGeometry = ::screenGeometry(i);
    return QStringLiteral("_%1x%2")
            .arg(screenGeometry.width())
            .arg(screenGeometry.height());
}

QString resolutionTag(const QWidget &widget, bool openOnCurrentScreen)
{
    if (openOnCurrentScreen) {
        const int i = screenNumber(widget);
        if (i == -1)
            return QString();
        return resolutionTagForScreen(i);
    }

    QString tag;
    for ( int i = 0; i < screenCount(); ++i )
        tag.append( resolutionTagForScreen(i) );

    return tag;
}

} // namespace

void ensureWindowOnScreen(QWidget *widget)
{
    const QSize frame  = frameSize(widget);
    int x = widget->x();
    int y = widget->y();
    int w = qMax(windowMinWidth, frame.width());
    int h = qMax(windowMinHeight, frame.height());

    const QRect availableGeometry = screenAvailableGeometry(*widget);
    if ( availableGeometry.isValid() ) {
        // Ensure that the window fits the screen, otherwise it would be moved
        // to a neighboring screen automatically.
        w = qMin(w, availableGeometry.width());
        h = qMin(h, availableGeometry.height());

        if ( x + w > availableGeometry.right() )
            x = availableGeometry.right() - w;

        if ( x < availableGeometry.left() )
            x = availableGeometry.left();

        if ( y + h > availableGeometry.bottom() )
            y = availableGeometry.bottom() - h;

        if ( y < availableGeometry.top())
            y = availableGeometry.top();
    }

    if ( frame != QSize(w, h) ) {
        GEOMETRY_LOG(widget) << "Resize window: " << frame << " -> " << QSize(w, h);
        widget->resize(w, h);
    }

    if ( widget->pos() != QPoint(x, y) ) {
        GEOMETRY_LOG(widget) << "Move window: " << widget->pos() << QPoint(x, y);
        widget->move(x, y);
    }
}

QVariant geometryOptionValue(const QString &optionName)
{
    const QSettings geometrySettings( getGeometryConfigurationFilePath(), QSettings::IniFormat );
    return geometrySettings.value(optionName);
}

void setGeometryOptionValue(const QString &optionName, const QVariant &value)
{
    QSettings geometrySettings( getGeometryConfigurationFilePath(), QSettings::IniFormat );
    geometrySettings.setValue(optionName, value);
}

void restoreWindowGeometry(QWidget *w, bool openOnCurrentScreen)
{
    const QString optionName = geometryOptionName(*w, openOnCurrentScreen);
    const QString tag = resolutionTag(*w, openOnCurrentScreen);
    QByteArray geometry = geometryOptionValue(optionName + tag).toByteArray();

    // If geometry for screen resolution doesn't exist, use last saved one.
    const bool restoreUntaggedGeometry = geometry.isEmpty();
    if (restoreUntaggedGeometry) {
        geometry = geometryOptionValue(optionName).toByteArray();

        // If geometry for the screen doesn't exist, move window to the middle of the screen.
        if (geometry.isEmpty()) {
            const QRect availableGeometry = screenAvailableGeometry(*w);
            if ( availableGeometry.isValid() ) {
                const QPoint position = availableGeometry.center() - w->rect().center();
                w->move(position);
            }
        }
    }

    const QRect oldGeometry = w->geometry();
    if ( !geometry.isEmpty() )
        w->restoreGeometry(geometry);

    ensureWindowOnScreen(w);

    GEOMETRY_LOG(w)
        << (geometry.isEmpty() ? "New" : "Restore") << " geometry \""
        << optionName << (restoreUntaggedGeometry ? QString() : tag) << "\": "
        << toString(oldGeometry) << " -> " << toString(w->geometry());
}

void saveWindowGeometry(QWidget *w, bool openOnCurrentScreen)
{
    if (w->isMinimized())
        return;

    const QString optionName = geometryOptionName(*w, openOnCurrentScreen);
    const QString tag = resolutionTag(*w, openOnCurrentScreen);
    QSettings geometrySettings( getGeometryConfigurationFilePath(), QSettings::IniFormat );
    const auto geometry = w->saveGeometry();
    geometrySettings.setValue(optionName + tag, geometry);
    geometrySettings.setValue(optionName, geometry);
    geometrySettings.setValue(geometryOptionName(*w), geometry);
    GEOMETRY_LOG(w) << "Save geometry \""
        << optionName << tag << "\": " << toString(w->geometry());
}

QByteArray mainWindowState(const QString &mainWindowObjectName)
{
    const QString optionName = QStringLiteral("Options/%1_state").arg(mainWindowObjectName);
    return geometryOptionValue(optionName).toByteArray();
}

void saveMainWindowState(const QString &mainWindowObjectName, const QByteArray &state)
{
    const QString optionName = QStringLiteral("Options/%1_state").arg(mainWindowObjectName);
    setGeometryOptionValue(optionName, state);
}

#ifdef COPYQ_MOVE_TO_WORKSPACE
/// Returns the value of a single-long CARDINAL X window property, or -1 on failure.
static long getCardinalProperty(Display *display, Window window, Atom property)
{
    Atom type;
    int format;
    unsigned long nitems, bytes_after;
    unsigned char *data = nullptr;

    long value = -1;
    if (XGetWindowProperty(display, window, property, 0, 1, False,
                           XA_CARDINAL, &type, &format, &nitems, &bytes_after, &data) == Success) {
        if (data) {
            if (type == XA_CARDINAL && format == 32 && nitems == 1)
                value = *reinterpret_cast<long*>(data);
            XFree(data);
        }
    }

    return value;
}
#endif

void moveToCurrentWorkspace(QWidget *w)
{
#ifdef COPYQ_MOVE_TO_WORKSPACE
    if (!w->isVisible())
        return;

    if (X11Info::isPlatformX11()) {
        auto display = X11Info::display();
        if (!display)
            return;

        const Window root = DefaultRootWindow(display);
        const Window wid = w->winId();

        static Atom atomCurrentDesktop = XInternAtom(display, "_NET_CURRENT_DESKTOP", False);
        static Atom atomWmDesktop = XInternAtom(display, "_NET_WM_DESKTOP", False);

        const long currentDesktop = getCardinalProperty(display, root, atomCurrentDesktop);
        const long windowDesktop = getCardinalProperty(display, wid, atomWmDesktop);

        // 0xFFFFFFFF means "sticky" (visible on all desktops) — no move needed
        if (windowDesktop == currentDesktop
            || currentDesktop < 0
            || static_cast<unsigned long>(windowDesktop) == 0xFFFFFFFF) {
            return;
        }

        GEOMETRY_LOG(w) << "Move to current workspace" << currentDesktop;

        XClientMessageEvent e{};
        e.type = ClientMessage;
        e.display = display;
        e.window = wid;
        e.message_type = atomWmDesktop;
        e.format = 32;
        e.data.l[0] = currentDesktop;
        // Source indication 2 (direct user action): CopyQ activates via global
        // shortcut, so the WM should comply unconditionally.  Matches the
        // source indication used in raise() for _NET_ACTIVE_WINDOW.
        e.data.l[1] = 2;
        XSendEvent(display, root, False,
                   SubstructureNotifyMask | SubstructureRedirectMask,
                   reinterpret_cast<XEvent*>(&e));
        XFlush(display);
    } else {
        // Wayland: hide+show re-initializes the window in the compositor,
        // allowing it to steal focus when shown by the caller.
        GEOMETRY_LOG(w) << "Move to current workspace (hide for re-show)";
        const bool blockUntilHide = isGeometryGuardBlockedUntilHidden(w);
        w->hide();
        if (blockUntilHide)
            setGeometryGuardBlockedUntilHidden(w, true);
    }
#else
    Q_UNUSED(w)
#endif
}

void moveWindowOnScreen(QWidget *w, QPoint pos)
{
    w->move(pos);
    ensureWindowOnScreen(w);
    moveToCurrentWorkspace(w);
}

void setGeometryGuardBlockedUntilHidden(QWidget *w, bool blocked)
{
    GEOMETRY_LOG(w) << "Geometry blocked until hidden: " << blocked;
    w->setProperty(propertyGeometryLockedUntilHide, blocked);
}

bool isGeometryGuardBlockedUntilHidden(const QWidget *w)
{
    return w->property(propertyGeometryLockedUntilHide).toBool();
}
