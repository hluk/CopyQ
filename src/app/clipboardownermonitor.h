// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CLIPBOARDOWNERMONITOR_H
#define CLIPBOARDOWNERMONITOR_H

#include <QAbstractNativeEventFilter>
#include <QByteArray>
#include <QTimer>

class ClipboardMonitor;

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
using NativeEventResult = qintptr;
#else
using NativeEventResult = long;
#endif

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

#endif // CLIPBOARDOWNERMONITOR_H
