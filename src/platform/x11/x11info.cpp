// SPDX-License-Identifier: GPL-3.0-or-later

#include "x11info.h"

#include <QGuiApplication>

#ifndef COPYQ_WITH_X11
struct _XDisplay {};
#elif QT_VERSION < QT_VERSION_CHECK(6,0,0)
#   include <QX11Info>
#else
#   include <X11/Xlib.h>
#endif

bool X11Info::isPlatformX11()
{
    return QGuiApplication::platformName() == QStringLiteral("xcb");
}

Display *X11Info::display()
{
#ifndef COPYQ_WITH_X11
    return nullptr;
#elif QT_VERSION < QT_VERSION_CHECK(6,0,0)
    return QX11Info::display();
#else
    auto *x11App = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    return x11App ? x11App->display() : nullptr;
#endif
}
