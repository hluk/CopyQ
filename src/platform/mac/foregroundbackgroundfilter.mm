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

#include "foregroundbackgroundfilter.h"

#include "common/log.h"

#include <QEvent>
#include <QGuiApplication>
#include <QWindow>

#include <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>

namespace {

bool isNormalApp()
{
    return [NSApp activationPolicy] == NSApplicationActivationPolicyRegular;
}

bool becomeNormalApp()
{
    return isNormalApp()
            || [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
}

bool becomeBackgroundApp()
{
    return !isNormalApp()
            || [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
}

bool isNormalAppWindow(QWindow *window)
{
    const auto type = window->type();
    return (type == Qt::Dialog || type == Qt::Window)
            && !window->flags().testFlag(Qt::FramelessWindowHint);
}

} // namespace

void ForegroundBackgroundFilter::installFilter(QObject *parent)
{
    auto filter = new ForegroundBackgroundFilter(parent);
    parent->installEventFilter(filter);
}

ForegroundBackgroundFilter::ForegroundBackgroundFilter(QObject *parent)
    : QObject(parent)
{
}

bool ForegroundBackgroundFilter::eventFilter(QObject *obj, QEvent *ev)
{
    if ( !obj->isWindowType() )
        return false;

    auto window = static_cast<QWindow*>(obj);
    Q_ASSERT(window);

    if ( !window->isTopLevel() )
        return false;

    const auto eventType = ev->type();
    if (eventType == QEvent::Show) {
        // Don't foreground if we already are
        if (!isNormalApp()) {
            // WORKAROUND: Transform menu to frameless dialog to fix keyboard focus.
            if (window->type() == Qt::Popup)
                window->setFlags(window->flags() | Qt::Dialog | Qt::FramelessWindowHint);

            if ( isNormalAppWindow(window) ) {
                COPYQ_LOG("Become normal app");
                if ( !becomeNormalApp() )
                    COPYQ_LOG("unable to become a regular window");
            }
        }
    } else if (eventType == QEvent::Hide) {
        // Don't background if there are any other visible windows..
        const auto windows = QGuiApplication::topLevelWindows();
        const bool hasFocusableWindow = std::any_of(
                    std::begin(windows), std::end(windows),
                    [window](QWindow *aWindow) {
                        return aWindow != window && aWindow->isVisible() && isNormalAppWindow(aWindow);
                    });

        if (!hasFocusableWindow) {
            COPYQ_LOG("Become background app");
            if ( !becomeBackgroundApp() ) {
                COPYQ_LOG("unable to become an accessory window");

                const auto worked = [NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];
                if (!worked)
                    COPYQ_LOG("unable to become a background/prohibited window");
            }
        }
    }

    return false;
}
