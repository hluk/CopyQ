// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DUMMYCLIPBOARD_H
#define DUMMYCLIPBOARD_H

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

    const QMimeData *mimeData(ClipboardMode mode) const override;

    bool isSelectionSupported() const override { return false; }

    bool isHidden(const QMimeData &data) const override;

    void setClipboardOwner(const QString &) override {}

protected:
    virtual const QMimeData *rawMimeData(ClipboardMode mode) const;
    virtual void onChanged(int mode);
    void onClipboardChanged(QClipboard::Mode mode);
};

#endif // DUMMYCLIPBOARD_H
