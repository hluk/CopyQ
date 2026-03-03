// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include "common/clipboardmode.h"
#include "platform/platformclipboard.h"

#include <QClipboard>

QClipboard::Mode modeToQClipboardMode(ClipboardMode mode);

class DummyClipboard : public PlatformClipboard
{
public:
    QVariantMap data(ClipboardMode mode, const QStringList &formats) const override;

    void setData(ClipboardMode mode, const QVariantMap &dataMap) override;
    void setRawData(ClipboardMode mode, QMimeData *mimeData) override;

    const QMimeData *mimeData(ClipboardMode mode) const override;

    bool isSelectionSupported() const override { return false; }

    bool isHidden(const QMimeData &data) const override;

    void setClipboardOwner(const QString &) override {}

protected:
    void startMonitoringBackend(const QStringList &, ClipboardModeMask) override;
    void stopMonitoringBackend() override;
    virtual const QMimeData *rawMimeData(ClipboardMode mode) const;
    virtual void onChanged(int mode);
    void onClipboardChanged(QClipboard::Mode mode);
    virtual const long int *clipboardSequenceNumber(ClipboardMode) const { return nullptr; }
};
