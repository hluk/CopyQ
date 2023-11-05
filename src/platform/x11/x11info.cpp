// SPDX-License-Identifier: GPL-3.0-or-later

#include "x11info.h"

#include <QGuiApplication>

#ifdef COPYQ_WITH_X11
#   if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#       include <QX11Info>
#   else
#       include <QtGui/private/qtx11extras_p.h>
#   endif
#   include <X11/Xlib.h>
#else
struct _XDisplay {};
#endif

bool X11Info::isPlatformX11()
{
    return QGuiApplication::platformName() == QStringLiteral("xcb");
}

Display *X11Info::display()
{
#ifdef COPYQ_WITH_X11
    return QX11Info::display();
#else
    return nullptr;
#endif
}
