// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DUMMYCLIPBOARD_H
#define DUMMYCLIPBOARD_H

#include "app/clipboardownermonitor.h"
#include "common/clipboardmode.h"
#include "platform/platformclipboard.h"

#include <QClipboard>

QClipboard::Mode modeToQClipboardMode(ClipboardMode mode);

class DummyClipboard : public PlatformClipboard
{
public:
    void startMonitoring(const QStringList &) override;

    void setMonitoringEnabled(ClipboardMode, bool) override {}

    QVariantMap data(ClipboardMode mode, const QStringList &formats) const override;

    void setData(ClipboardMode mode, const QVariantMap &dataMap) override;

    QByteArray clipboardOwner() override { return m_ownerMonitor.clipboardOwner(); }

    const QMimeData *mimeData(ClipboardMode mode) const override;

    bool isSelectionSupported() const override { return false; }

protected:
    virtual void onChanged(int mode);
    void onClipboardChanged(QClipboard::Mode mode);

    ClipboardOwnerMonitor m_ownerMonitor;
};

#endif // DUMMYCLIPBOARD_H
