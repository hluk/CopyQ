// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WINPLATFORMCLIPBOARD_H
#define WINPLATFORMCLIPBOARD_H

#include "platform/dummy/dummyclipboard.h"

#include <qt_windows.h>

class WinPlatformClipboard final : public DummyClipboard
{
public:
    void startMonitoring(const QStringList &) override;

private:
    void checkClipboard();

    DWORD m_lastClipboardSequenceNumber = -1;
};

#endif // WINPLATFORMCLIPBOARD_H
