// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-identifier"
#endif

typedef struct _XDisplay Display;

#ifdef __clang__
#pragma clang diagnostic pop
#endif

class X11Info {
public:
    static bool isPlatformX11();
    static Display *display();
};
