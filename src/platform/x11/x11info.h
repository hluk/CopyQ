// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

typedef struct _XDisplay Display;

class X11Info {
public:
    static bool isPlatformX11();
    static Display *display();
};
