// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-identifier"
typedef struct _XDisplay Display;
#pragma clang diagnostic pop

class X11Info {
public:
    static bool isPlatformX11();
    static Display *display();
};
