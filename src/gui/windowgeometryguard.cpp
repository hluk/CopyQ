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

#include "windowgeometryguard.h"

#include "common/appconfig.h"
#include "common/common.h"
#include "common/config.h"
#include "common/log.h"
#include "common/timer.h"
#include "gui/screen.h"

#include <QApplication>
#include <QEvent>
#include <QMoveEvent>
#include <QScreen>
#include <QVariant>
#include <QWindow>

namespace {

bool openOnCurrentScreen()
{
    return AppConfig().option<Config::open_windows_on_current_screen>();
}

bool isRestoreGeometryEnabled()
{
    return AppConfig().option<Config::restore_geometry>();
}

bool isMousePositionSupported()
{
    // On Wayland, getting mouse position can return
    // the last known mouse position in an own Qt application window.
    static const bool supported = !QCursor::pos().isNull();
    return supported;
}

QScreen *currentScreen()
{
    if (!isMousePositionSupported())
        return nullptr;

    const int i = screenNumberAt(QCursor::pos());
    const auto screens = QGuiApplication::screens();
    if (0 <= i && i < screens.size())
        return screens[i];

    return nullptr;
}

} // namespace

void WindowGeometryGuard::create(QWidget *window)
{
    static const bool enabled = isRestoreGeometryEnabled();
    if (enabled)
        new WindowGeometryGuard(window);
}

bool WindowGeometryGuard::eventFilter(QObject *, QEvent *event)
{
    const QEvent::Type type = event->type();

    switch (type) {
    case QEvent::Show: {
        m_timerSaveGeometry.stop();

        QWindow *window = m_window->windowHandle();
        if (window) {
            connect( window, &QWindow::screenChanged,
                     this, &WindowGeometryGuard::onScreenChanged, Qt::UniqueConnection );

            if ( !isWindowGeometryLocked() && openOnCurrentScreen() && isMousePositionSupported() ) {
                QScreen *screen = currentScreen();
                if (screen && window->screen() != screen) {
                    COPYQ_LOG(QStringLiteral("Geometry: Moving to screen: %1").arg(screen->name()));
                    m_window->move(screen->availableGeometry().topLeft());
                }
            }
        }
        break;
    }

    case QEvent::Move:
    case QEvent::Resize:
        if ( !isWindowGeometryLocked() && m_window->isVisible() )
            m_timerSaveGeometry.start();
        break;

    case QEvent::Hide:
        if ( isGeometryGuardBlockedUntilHidden(m_window) )
            setGeometryGuardBlockedUntilHidden(m_window, false);
        break;

    default:
        break;
    }

    return false;
}

WindowGeometryGuard::WindowGeometryGuard(QWidget *window)
    : QObject(window)
    , m_window(window)
{
    initSingleShotTimer(&m_timerSaveGeometry, 250, this, &WindowGeometryGuard::saveWindowGeometry);
    initSingleShotTimer(&m_timerRestoreGeometry, 0, this, &WindowGeometryGuard::restoreWindowGeometry);
    initSingleShotTimer(&m_timerUnlockGeometry, 250, this, &WindowGeometryGuard::unlockWindowGeometry);

    m_window->installEventFilter(this);
    restoreWindowGeometry();
}

bool WindowGeometryGuard::isWindowGeometryLocked() const
{
    return m_timerUnlockGeometry.isActive() || isGeometryGuardBlockedUntilHidden(m_window);
}

bool WindowGeometryGuard::lockWindowGeometry()
{
    if ( isWindowGeometryLocked() )
        return false;

    m_timerUnlockGeometry.start();

    return true;
}

void WindowGeometryGuard::saveWindowGeometry()
{
    if ( !lockWindowGeometry() )
        return;

    ::saveWindowGeometry(m_window, openOnCurrentScreen());
    unlockWindowGeometry();
}

void WindowGeometryGuard::restoreWindowGeometry()
{
    if ( !lockWindowGeometry() )
        return;

    ::restoreWindowGeometry(m_window, openOnCurrentScreen());
}

void WindowGeometryGuard::unlockWindowGeometry()
{
    m_timerUnlockGeometry.stop();
}

void WindowGeometryGuard::onScreenChanged()
{
    if (!openOnCurrentScreen())
        return;

    if ( !lockWindowGeometry() )
        return;

    QWindow *window = m_window->windowHandle();
    if (!window)
        return;

    QScreen *screen = window->screen();
    if (!screen)
        return;

    COPYQ_LOG(QStringLiteral("Geometry: Screen changed: %1").arg(screen->name()));

    const bool isMousePositionSupported = ::isMousePositionSupported();
    if ( window && isMousePositionSupported && screen != currentScreen() ) {
        COPYQ_LOG(QStringLiteral("Geometry: Avoiding geometry-restore on incorrect screen"));
        return;
    }

    if (isMousePositionSupported || m_window->isModal()) {
        ::restoreWindowGeometry(m_window, true);
    } else {
        // WORKAROUND: Center window position on Sway window compositor which
        // does not support changing window position.
        m_window->hide();
        ::restoreWindowGeometry(m_window, true);
        m_window->show();
    }
}
