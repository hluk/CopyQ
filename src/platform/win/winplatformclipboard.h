// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WINPLATFORMCLIPBOARD_H
#define WINPLATFORMCLIPBOARD_H

#include "platform/dummy/dummyclipboard.h"

#include <qt_windows.h>

class WinPlatformClipboard final : public DummyClipboard
{
public:
    void startMonitoring(const QStringList &) override;

    bool isHidden(const QMimeData &data) const override;

protected:
    void onChanged(int) override;
    const long int *clipboardSequenceNumber(ClipboardMode) const override {
        return &m_lastClipboardSequenceNumber; 
    }

private:
    long int m_lastClipboardSequenceNumber = 0;
};

#endif // WINPLATFORMCLIPBOARD_H
