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

#include "qxtglobalshortcut.h"

#include "platform/x11/x11info.h"

#include <QApplication>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDebug>
#include <QDBusObjectPath>
#include <QPointer>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTimer>
#include <QVector>
#include <QWidget>
#include <X11/keysym.h>
#include <X11/Xlib.h>

#include <xcb/xcb.h>

#include "xcbkeyboard.h"

class GlobalShortcutsPortal : public QObject
{
    Q_OBJECT
public:
    using PortalShortcut = QPair<QString, QVariantMap>;
    using PortalShortcuts = QList<PortalShortcut>;

    virtual ~GlobalShortcutsPortal()
    {
        disconnectPortal();
    }

    static GlobalShortcutsPortal *instance()
    {
        static GlobalShortcutsPortal portal;
        return &portal;
    }

    bool isValid() const
    {
        return !m_objPathCreateSession.path().isEmpty();
    }

    void addShortcut(QxtGlobalShortcut *shortcut)
    {
        m_shortcuts.removeAll(nullptr);
        if ( !m_shortcuts.contains(shortcut) ) {
            m_shortcuts.append(shortcut);
            m_timerBind.start();
        }
    }

    void removeShortcut(QxtGlobalShortcut *shortcut)
    {
        m_shortcuts.removeAll(nullptr);
        if ( m_shortcuts.contains(shortcut) ) {
            m_shortcuts.removeAll(shortcut);
            m_timerBind.start();
        }
    }

private:
    static QString normalizeShortcutString(const QString &shortcut)
    {
        return shortcut.toUpper().replace("SUPER", "LOGO").replace("META", "LOGO");
    }

    QString handleToken()
    {
        return QStringLiteral("%1_%2")
            .arg(m_portalToken)
            .arg(QRandomGenerator::global()->generate());
    }

    QDBusObjectPath initPortalGlobalShortcuts()
    {
        qDBusRegisterMetaType<PortalShortcut>();
        qDBusRegisterMetaType<PortalShortcuts>();

        QDBusMessage message = m_globalShortcutInterface.call(
            QStringLiteral("CreateSession"),
            QMap<QString, QVariant>{
                {QStringLiteral("handle_token"), handleToken()},
                {QStringLiteral("session_handle_token"), m_portalToken}
            }
        );

        if (message.type() == QDBusMessage::ErrorMessage) {
            qWarning() << "QxtGlobalShortcut: Failed to create portal global shortcuts session:"
                << message.errorMessage();
            return {};
        }

        return message.arguments().first().value<QDBusObjectPath>();
    }

    void connectPortal()
    {
        m_objPathCreateSession = initPortalGlobalShortcuts();
        if (!m_objPathCreateSession.path().isEmpty()) {
            QDBusConnection::sessionBus().connect(
                QStringLiteral("org.freedesktop.portal.Desktop"),
                m_objPathCreateSession.path(),
                QStringLiteral("org.freedesktop.portal.Request"),
                QStringLiteral("Response"),
                this,
                SLOT(onPortalSessionCreated(uint,QVariantMap))
            );
        }
    }

    void disconnectPortal()
    {
        if (!m_objPathCreateSession.path().isEmpty()) {
            QDBusConnection::sessionBus().disconnect(
                QStringLiteral("org.freedesktop.portal.Desktop"),
                m_objPathCreateSession.path(),
                QStringLiteral("org.freedesktop.portal.Request"),
                QStringLiteral("Response"),
                this,
                SLOT(onPortalSessionCreated(uint,QVariantMap))
            );
        }

        if (!m_objPathListShortcuts.path().isEmpty()) {
            QDBusConnection::sessionBus().disconnect(
                QStringLiteral("org.freedesktop.portal.Desktop"),
                m_objPathListShortcuts.path(),
                QStringLiteral("org.freedesktop.portal.Request"),
                QStringLiteral("Response"),
                this,
                SLOT(onListShortcuts(uint,QVariantMap))
            );
        }

        if (!m_objPathGlobalShortcuts.path().isEmpty()) {
            QDBusConnection::sessionBus().disconnect(
                QStringLiteral("org.freedesktop.portal.Desktop"),
                QStringLiteral("/org/freedesktop/portal/desktop"),
                QStringLiteral("org.freedesktop.portal.GlobalShortcuts"),
                QStringLiteral("Activated"),
                this,
                SLOT(onPortalGlobalShortcutActivated(QDBusObjectPath,QString,qulonglong,QVariantMap))
            );
        }
    }

    void bindPortalGlobalShortcuts()
    {
        const QString descriptionPrefix = QCoreApplication::applicationName()
            .replace(QStringLiteral("copyq"), QStringLiteral("CopyQ"));
        PortalShortcuts shortcuts;
        m_shortcuts.removeAll(nullptr);
        for (const auto &shortcut : m_shortcuts) {
            if (!shortcut->isEnabled())
                continue;
            const QString name = shortcut->name();
            const QString description = QStringLiteral("%1 - %2")
                .arg(descriptionPrefix, name);
            const QString preferredTrigger = shortcut->shortcut()
                .toString(QKeySequence::PortableText)
                .toUpper()
                .replace("SUPER", "LOGO")
                .replace("META", "LOGO");
            // WORKAROUND: Include the shortcut in ID so it can be overridden.
            // Works at least in KDE.
            const QString shortcutId = QStringLiteral("%1||%2").arg(preferredTrigger, name);
            shortcuts.append({shortcutId, {
                {QStringLiteral("description"), description},
                {QStringLiteral("preferred_trigger"), preferredTrigger},
            }});
        }

        std::sort(shortcuts.begin(), shortcuts.end(), [](const auto &a, const auto &b) {
            return a.first < b.first;
        });

        if (shortcuts.isEmpty() || shortcuts == m_boundShortcuts)
            return;

        // Shortcuts can be bound only once per session.
        // Notify user to restart the app.
        if (m_bound) {
            qDebug() << "QxtGlobalShortcut: Can bind portal global shortcuts only once per session";
            if (m_notifyRestart) {
                m_notifyRestart = false;
                QxtGlobalShortcut::notifyRestartNeeded();
            }
            return;
        }

        // WORKAROUND: Reset old shortcuts if they are not available anymore.
        // There is no API to unbind old shortcuts.
        // This works in KDE: Changing ID to some unused value and keeping the
        // description allows to override the old shortcut.
        for (const auto &oldShortcut : m_boundShortcuts) {
            auto it = std::find_if(shortcuts.begin(), shortcuts.end(),
                [&oldShortcut](const auto &s) { return s.first == oldShortcut.first; });
            if (it == shortcuts.cend()) {
                const QString shortcutId = QStringLiteral("OBSOLETE||%1").arg(oldShortcut.first);
                shortcuts.append({shortcutId, {
                    {QStringLiteral("description"), oldShortcut.second.value(QStringLiteral("description"))},
                    {QStringLiteral("preferred_trigger"), QString()},
                }});
            }
        }

        qDebug() << "QxtGlobalShortcut: Binding portal global shortcuts:" << shortcuts;

        const QDBusMessage message = m_globalShortcutInterface.call(
            QStringLiteral("BindShortcuts"),
            m_objPathGlobalShortcuts,
            QVariant::fromValue(shortcuts),
            QString(),
            QMap<QString, QVariant>{
                {QStringLiteral("handle_token"), handleToken()},
            }
        );

        if (message.type() == QDBusMessage::ErrorMessage) {
            qWarning() << "QxtGlobalShortcut: Failed to bind portal global shortcuts:"
                << message.errorMessage();
        }

        m_boundShortcuts = shortcuts;
        m_bound = true;
    }

    GlobalShortcutsPortal()
        : m_globalShortcutInterface(
            QStringLiteral("org.freedesktop.portal.Desktop"),
            QStringLiteral("/org/freedesktop/portal/desktop"),
            QStringLiteral("org.freedesktop.portal.GlobalShortcuts")
        )
        , m_portalToken(
            // Use only valid token name characters.
            QCoreApplication::applicationName().replace(
                QRegularExpression(QStringLiteral("[^A-Za-z0-9_]+")), QStringLiteral("_"))
        )
    {
        m_timerBind.setSingleShot(true);
        m_timerBind.setInterval(0);
        connectPortal();
    }

    void listShortcuts()
    {
        const QDBusMessage message = m_globalShortcutInterface.call(
            QStringLiteral("ListShortcuts"),
            m_objPathGlobalShortcuts,
            QMap<QString, QVariant>{
                {QStringLiteral("handle_token"), handleToken()},
            }
        );

        if (message.type() == QDBusMessage::ErrorMessage) {
            qWarning() << "QxtGlobalShortcut: Failed to list portal global shortcuts:"
                << message.errorMessage();
            return;
        }

        m_objPathListShortcuts = message.arguments().first().value<QDBusObjectPath>();

        QDBusConnection::sessionBus().connect(
            QStringLiteral("org.freedesktop.portal.Desktop"),
            m_objPathListShortcuts.path(),
            QStringLiteral("org.freedesktop.portal.Request"),
            QStringLiteral("Response"),
            this,
            SLOT(onListShortcuts(uint,QVariantMap))
        );
    }

private slots:
    void onListShortcuts(uint responseCode, const QVariantMap& results)
    {
        if (responseCode != 0) {
            qWarning() << "QxtGlobalShortcut: Failed to list portal global shortcuts:"
                << responseCode << results;
            return;
        }

        const auto arg = results.value(QStringLiteral("shortcuts")).value<QDBusArgument>();
        arg >> m_boundShortcuts;
        const auto it = std::remove_if(m_boundShortcuts.begin(), m_boundShortcuts.end(),
            [](const auto &shortcut) {
                return shortcut.second.value(QStringLiteral("trigger_description")).toString().isEmpty();
            });
        m_boundShortcuts.erase(it, m_boundShortcuts.end());
        for (auto &shortcut : m_boundShortcuts) {
            const auto trigger = shortcut.second.take(QStringLiteral("trigger_description")).toString();
            shortcut.second[QStringLiteral("preferred_trigger")] = normalizeShortcutString(trigger);
        }
        std::sort(m_boundShortcuts.begin(), m_boundShortcuts.end(), [](const auto &a, const auto &b) {
            return a.first < b.first;
        });
        qDebug() << "QxtGlobalShortcut: Previously registered shortcuts:" << m_boundShortcuts;

        connect(
            &m_timerBind, &QTimer::timeout,
            this, &GlobalShortcutsPortal::bindPortalGlobalShortcuts);

        QDBusConnection::sessionBus().connect(
            QStringLiteral("org.freedesktop.portal.Desktop"),
            QStringLiteral("/org/freedesktop/portal/desktop"),
            QStringLiteral("org.freedesktop.portal.GlobalShortcuts"),
            QStringLiteral("Activated"),
            this,
            SLOT(onPortalGlobalShortcutActivated(QDBusObjectPath,QString,qulonglong,QVariantMap))
        );

        m_timerBind.start();
    }

    void onPortalSessionCreated(uint responseCode, const QVariantMap& results)
    {
        if (responseCode != 0) {
            qWarning() << "QxtGlobalShortcut: Failed to create portal global shortcuts session:"
                << responseCode << results;
            return;
        }

        m_objPathGlobalShortcuts = QDBusObjectPath(
            results.value(QStringLiteral("session_handle")).value<QString>());
        if (m_objPathGlobalShortcuts.path().isEmpty()) {
            qWarning() << "QxtGlobalShortcut: Failed to get portal global shortcuts session path:" << results;
            return;
        }

        qDebug() << "QxtGlobalShortcut: Portal global shortcut session:"
            << m_objPathGlobalShortcuts.path();

        disconnectPortal();

        listShortcuts();
    }

    void onPortalGlobalShortcutActivated(
        const QDBusObjectPath &,
        const QString &shortcutId,
        qulonglong ,
        const QVariantMap &)
    {
        const QString shortcutName = shortcutId.section("||", 1, 1);
        for (const auto &shortcut : m_shortcuts) {
            if (shortcut && shortcut->name() == shortcutName) {
                qDebug() << "QxtGlobalShortcut: Portal global shortcut activated:" << shortcutId;
                shortcut->activate();
                return;
            }
        }
        qWarning() << "QxtGlobalShortcut: Portal global shortcut failed to activate:" << shortcutName;
    }

private:
    bool m_bound = false;
    PortalShortcuts m_boundShortcuts;
    bool m_notifyRestart = true;
    QDBusInterface m_globalShortcutInterface;
    QString m_portalToken;
    QDBusObjectPath m_objPathCreateSession;
    QDBusObjectPath m_objPathListShortcuts;
    QDBusObjectPath m_objPathGlobalShortcuts;
    QTimer m_timerBind;
    QList<QPointer<QxtGlobalShortcut>> m_shortcuts;
};

namespace {

/**
 * Creates first invisible application window
 * so X11 key press events can be received.
 *
 * This is used for Cinnamon and KDE.
 */
void createFirstWindow()
{
    static QWidget *w = nullptr;
    if (!w) {
        // Try hard so the window is not visible.

        // Tool tips won't show in taskbar.
        w = new QWidget(nullptr, Qt::ToolTip);
        w->setWindowTitle(QStringLiteral("Dummy"));
        w->setWindowRole(QStringLiteral("dummy"));

        // Move out of screen (if it's not possible to show the window minimized).
        w->resize(1, 1);
        w->move(-100000, -100000);

        // Show and hide quickly.
        w->showMinimized();
        w->hide();
    }
}

QVector<quint32> maskModifiers()
{
    return QVector<quint32>() << 0 << Mod2Mask << LockMask << (Mod2Mask | LockMask);
}

using X11ErrorHandler = int (*)(Display* display, XErrorEvent* event);

class QxtX11ErrorHandler final {
public:
    static bool error;

    static int qxtX11ErrorHandler(Display* display, XErrorEvent *event)
    {
        Q_UNUSED(display)
        switch (event->error_code)
        {
            case BadAccess:
            case BadValue:
            case BadWindow:
                if (event->request_code == 33 /* X_GrabKey */ ||
                        event->request_code == 34 /* X_UngrabKey */)
                {
                    error = true;
                    //TODO:
                    //char errstr[256];
                    //XGetErrorText(dpy, err->error_code, errstr, 256);
                }
        }
        return 0;
    }

    QxtX11ErrorHandler()
        : m_previousErrorHandler(nullptr)
    {
        error = false;
        m_previousErrorHandler = XSetErrorHandler(qxtX11ErrorHandler);
    }

    ~QxtX11ErrorHandler()
    {
        XSetErrorHandler(m_previousErrorHandler);
    }

private:
    X11ErrorHandler m_previousErrorHandler;
};

bool QxtX11ErrorHandler::error = false;

class QxtX11Data final {
public:
    QxtX11Data()
        : m_display(nullptr)
    {
        if ( X11Info::isPlatformX11() ) {
            createFirstWindow();
            m_display = X11Info::display();
        }
    }

    bool isValid()
    {
        return X11Info::isPlatformX11() && m_display != nullptr;
    }

    Display *display()
    {
        Q_ASSERT(isValid());
        return m_display;
    }

    Window rootWindow()
    {
        return DefaultRootWindow(display());
    }

    bool grabKey(quint32 keycode, quint32 modifiers, Window window)
    {
        QxtX11ErrorHandler errorHandler;

        for (const auto maskMods : maskModifiers()) {
            XGrabKey(display(), static_cast<int>(keycode), modifiers | maskMods, window, True,
                     GrabModeAsync, GrabModeAsync);
            if (errorHandler.error)
                break;
        }

        if (errorHandler.error) {
            ungrabKey(keycode, modifiers, window);
            return false;
        }

        return true;
    }

    bool ungrabKey(quint32 keycode, quint32 modifiers, Window window)
    {
        QxtX11ErrorHandler errorHandler;

        for (const auto maskMods : maskModifiers()) {
            XUngrabKey(display(), static_cast<int>(keycode), modifiers | maskMods, window);
        }

        return !errorHandler.error;
    }

private:
    Display *m_display;
};

KeySym qtKeyToXKeySym(Qt::Key key, Qt::KeyboardModifiers mods)
{
    int i = 0;

    if ( mods.testFlag(Qt::KeypadModifier) ) {
        switch (key) {
        case Qt::Key_0: return XK_KP_Insert;
        case Qt::Key_1: return XK_KP_End;
        case Qt::Key_2: return XK_KP_Down;
        case Qt::Key_3: return XK_KP_Page_Down;
        case Qt::Key_4: return XK_KP_Left;
        case Qt::Key_5: return XK_KP_Begin;
        case Qt::Key_6: return XK_KP_Right;
        case Qt::Key_7: return XK_KP_Home;
        case Qt::Key_8: return XK_KP_Up;
        case Qt::Key_9: return XK_KP_Page_Up;
        default: break;
        }

        for (; KeyTbl[i] != 0; i += 2) {
            if (KeyTbl[i] == XK_Num_Lock)
                break;
        }
    } else {
        const auto keySym = XStringToKeysym(QKeySequence(key).toString().toLatin1().data());
        if (keySym != NoSymbol)
            return keySym;
    }

    for (; KeyTbl[i] != 0; i += 2) {
        if (KeyTbl[i + 1] == key)
            return KeyTbl[i];
    }

    return static_cast<ushort>(key);
}

} // namespace

bool QxtGlobalShortcutPrivate::nativeEventFilter(
    const QByteArray &eventType, void *message, NativeEventResult *result)
{
    Q_UNUSED(result)

    xcb_key_press_event_t *kev = nullptr;
    if (eventType == "xcb_generic_event_t") {
        xcb_generic_event_t* ev = static_cast<xcb_generic_event_t *>(message);
        if ((ev->response_type & 127) == XCB_KEY_PRESS)
            kev = static_cast<xcb_key_press_event_t *>(message);
    }

    if (kev != nullptr) {
        unsigned int keycode = kev->detail;
        unsigned int keystate = 0;
        if(kev->state & XCB_MOD_MASK_1)
            keystate |= Mod1Mask;
        if(kev->state & XCB_MOD_MASK_CONTROL)
            keystate |= ControlMask;
        if(kev->state & XCB_MOD_MASK_4)
            keystate |= Mod4Mask;
        if(kev->state & XCB_MOD_MASK_SHIFT)
            keystate |= ShiftMask;
        activateShortcut(keycode,
            // Mod1Mask == Alt, Mod4Mask == Meta
            keystate & (ShiftMask | ControlMask | Mod1Mask | Mod4Mask));
    }
    return false;
}

void QxtGlobalShortcutPrivate::init()
{
    if ( X11Info::isPlatformX11() )
        initFallback();
}

void QxtGlobalShortcutPrivate::destroy()
{
    if ( X11Info::isPlatformX11() )
        destroyFallback();
}

bool QxtGlobalShortcutPrivate::setShortcut(const QKeySequence& shortcut)
{
    if ( X11Info::isPlatformX11() )
        return setShortcutFallback(shortcut);

    auto portal = GlobalShortcutsPortal::instance();
    if (!portal->isValid())
        return false;

    setKeySequence(shortcut);
    portal->addShortcut(q_ptr);
    registered = true;
    return true;
}

bool QxtGlobalShortcutPrivate::unsetShortcut()
{
    if ( X11Info::isPlatformX11() )
        return unsetShortcutFallback();

    auto portal = GlobalShortcutsPortal::instance();
    portal->removeShortcut(q_ptr);
    registered = false;
    return true;
}

quint32 QxtGlobalShortcutPrivate::nativeModifiers(Qt::KeyboardModifiers modifiers)
{
    if ( !X11Info::isPlatformX11() )
        return 0;

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

quint32 QxtGlobalShortcutPrivate::nativeKeycode(Qt::Key key, Qt::KeyboardModifiers mods)
{
    QxtX11Data x11;
    if (!x11.isValid())
        return 0;

    const KeySym keysym = qtKeyToXKeySym(key, mods);
    return XKeysymToKeycode(x11.display(), keysym);
}

bool QxtGlobalShortcutPrivate::registerShortcut(quint32 nativeKey, quint32 nativeMods)
{
    QxtX11Data x11;
    return x11.isValid() && x11.grabKey(nativeKey, nativeMods, x11.rootWindow());
}

bool QxtGlobalShortcutPrivate::unregisterShortcut(quint32 nativeKey, quint32 nativeMods)
{
    QxtX11Data x11;
    return x11.isValid() && x11.ungrabKey(nativeKey, nativeMods, x11.rootWindow());
}

#include "qxtglobalshortcut_x11.moc"
