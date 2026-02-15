// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include "platform/dummy/dummyclipboard.h"

class MacTimer;

class MacClipboard final : public DummyClipboard {
public:
    void setData(ClipboardMode mode, const QVariantMap &dataMap) override;

    bool isHidden(const QMimeData &data) const override;

protected:
    void startMonitoringBackend(const QStringList &, ClipboardModeMask) override;
    void stopMonitoringBackend() override;
    void onChanged(int mode) override;
    const long int *clipboardSequenceNumber(ClipboardMode) const override {
        return &m_prevChangeCount;
    }

private:
    void clipboardTimeout();

    long int m_prevChangeCount = 0;
    MacTimer *m_timer = nullptr;
};
