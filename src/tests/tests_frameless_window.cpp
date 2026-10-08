// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_utils.h"
#include "tests.h"

#define SIZE_GRIP "size_grip"

void CoreTests::framelessWindowShowsSizeGrip()
{
    RUN("config" << "frameless_window", "false\n");
    RUN("config" << "frameless_window" << "true", "true\n");

#ifdef Q_OS_MAC
    SKIP("Mouse events on the size grip don't reach the window on OS X");
#endif

    RUN("add" << "ITEM", "");
    RUN("show", "");

    // Without the title bar the window is resized from the bottom right corner.
    KEYS(clipboardBrowserId << "mouse|PRESS|" SIZE_GRIP);
    KEYS(clipboardBrowserId << "mouse|RELEASE|" SIZE_GRIP);

    RUN("config" << "frameless_window" << "false", "false\n");
}
