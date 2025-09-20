// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include "platform/dummy/dummyclipboard.h"

#include <QByteArray>
#include <QStringList>
#include <QTimer>

class X11PlatformClipboard final : public DummyClipboard
{
public:
    X11PlatformClipboard();

    void startMonitoring(const QStringList &formats) override;

    void setMonitoringEnabled(ClipboardMode mode, bool enable) override;

    QVariantMap data(ClipboardMode mode, const QStringList &formats) const override;

    void setData(ClipboardMode mode, const QVariantMap &dataMap) override;

    bool isSelectionSupported() const override { return true; }

    void setClipboardOwner(const QString &owner) override { m_clipboardOwner = owner; }

protected:
    const QMimeData *rawMimeData(ClipboardMode mode) const override;
    void onChanged(int mode) override;
    const long int *clipboardSequenceNumber(ClipboardMode mode) const override {
        return mode == ClipboardMode::Clipboard
            ? &m_clipboardData.sequenceNumber
            : &m_selectionData.sequenceNumber;
    }

private:
    struct ClipboardData {
        QVariantMap newData;
        QVariantMap data;
        QString owner;
        QString newOwner;
        QTimer timerEmitChange;
        QStringList formats;
        quint32 newDataTimestamp;
        ClipboardMode mode;
        bool enabled = true;
        bool cloningData = false;
        long int sequenceNumber = 0;
        bool ignoreNext = false;
        int retry = 0;
    };

    void check();
    void updateClipboardData(ClipboardData *clipboardData);
    void useNewClipboardData(ClipboardData *clipboardData);
    void checkAgainLater(bool clipboardChanged, int interval);

    QTimer m_timerCheckAgain;

    ClipboardData m_clipboardData;
    ClipboardData m_selectionData;

    bool m_monitoring = false;

    QString m_clipboardOwner;
};
