// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include <QAbstractNativeEventFilter>
#include <QByteArray>
#include <QTimer>

class ClipboardMonitor;

using NativeEventResult = qintptr;

class ClipboardOwnerMonitor final : public QAbstractNativeEventFilter
{
public:
    explicit ClipboardOwnerMonitor(ClipboardMonitor *monitor);
    ~ClipboardOwnerMonitor();

    bool nativeEventFilter(
        const QByteArray &, void *message, NativeEventResult *result) override;

    void setUpdateInterval(int ms) { m_timerSetOwner.setInterval(ms); }

    void update();

private:
    ClipboardMonitor *m_monitor;
    QString m_lastClipboardOwner;
    QStringList m_nextClipboardOwners;
    QTimer m_timerSetOwner;
    QTimer m_timerUpdateAfterEvent;
};
