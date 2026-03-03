// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include "platform/dummy/dummyclipboard.h"

#include <qt_windows.h>

class QTimer;

class WinPlatformClipboard final : public DummyClipboard
{
public:
    bool isHidden(const QMimeData &data) const override;

protected:
    void startMonitoringBackend(const QStringList &, ClipboardModeMask) override;
    void stopMonitoringBackend() override;
    void onChanged(int) override;
    const long int *clipboardSequenceNumber(ClipboardMode) const override {
        return &m_lastClipboardSequenceNumber;
    }

private:
    long int m_lastClipboardSequenceNumber = 0;
    QTimer *m_timer = nullptr;
};
