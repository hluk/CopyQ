// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MACCLIPBOARD_H
#define MACCLIPBOARD_H

#include "platform/dummy/dummyclipboard.h"

class MacClipboard final : public DummyClipboard {
public:
    void startMonitoring(const QStringList &) override;

    void setData(ClipboardMode mode, const QVariantMap &dataMap) override;

    bool isHidden(const QMimeData &data) const override;

protected:
    void onChanged(int mode) override;

private:
    void clipboardTimeout();

    long int m_prevChangeCount = 0;
};

#endif // MACCLIPBOARD_H
