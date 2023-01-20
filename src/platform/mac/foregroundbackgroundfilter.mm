// SPDX-License-Identifier: GPL-3.0-or-later

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

void becomeNormalApp()
{
    if (!isNormalApp()) {
        COPYQ_LOG("Become normal app");
        if (![NSApp setActivationPolicy:NSApplicationActivationPolicyRegular])
            COPYQ_LOG("Unable to become a normal app");
    }
}

void becomeAccessoryApp()
{
    if ([NSApp activationPolicy] != NSApplicationActivationPolicyAccessory) {
        COPYQ_LOG("Become accessory app");
        if (![NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory])
            COPYQ_LOG("Unable to become a accessory app");
    }
}

void becomeBackgroundApp()
{
    if ([NSApp activationPolicy] != NSApplicationActivationPolicyProhibited) {
        COPYQ_LOG("Become background app");
        if (![NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited])
            COPYQ_LOG("Unable to become a background app");
    }
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
        // WORKAROUND: Transform menu to frameless dialog to fix keyboard focus.
        if (window->type() == Qt::Popup && QGuiApplication::focusWindow() == nullptr) {
            COPYQ_LOG("Fixing focus for popup");
            window->setFlags(window->flags() | Qt::Dialog | Qt::FramelessWindowHint);
        }

        // Allow windows to pop up on current space.
        const auto wid = window->winId();
        auto nsView = reinterpret_cast<NSView*>(wid);
        auto nsWindow = [nsView window];
        if (nsWindow)
            [nsWindow setCollectionBehavior: NSWindowCollectionBehaviorMoveToActiveSpace];

        if ( isNormalAppWindow(window) )
            becomeNormalApp();
        else if (!isNormalApp())
            becomeAccessoryApp();
    } else if (eventType == QEvent::Hide) {
        // Don't background if there are any other visible windows..
        auto windows = QGuiApplication::topLevelWindows();
        windows.removeOne(window);

        const bool hasFocusableWindow = std::any_of(
                    std::begin(windows), std::end(windows),
                    [](QWindow *aWindow) {
                        return aWindow->isVisible() && isNormalAppWindow(aWindow);
                    });

        if (hasFocusableWindow) {
            becomeNormalApp();
        } else {
            const bool hasAccessoryWindow = std::any_of(
                        std::begin(windows), std::end(windows),
                        [](QWindow *aWindow) {
                            return aWindow->isVisible();
                        });

            if (hasAccessoryWindow)
                becomeAccessoryApp();
            else
                becomeBackgroundApp(); // This also focuses the previous app.
        }
    }

    return false;
}
