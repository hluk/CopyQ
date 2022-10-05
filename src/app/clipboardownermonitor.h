// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CLIPBOARDOWNERMONITOR_H
#define CLIPBOARDOWNERMONITOR_H

#include <QAbstractNativeEventFilter>
#include <QByteArray>
#include <QTimer>

#include "platform/platformnativeinterface.h"

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
using NativeEventResult = qintptr;
#else
using NativeEventResult = long;
#endif

class ClipboardOwnerMonitor final : public QAbstractNativeEventFilter
{
public:
    ClipboardOwnerMonitor();
    ~ClipboardOwnerMonitor();

    const QByteArray &clipboardOwner() const { return m_clipboardOwner; }

    bool nativeEventFilter(
        const QByteArray &, void *message, NativeEventResult *result) override;

private:
    QByteArray m_clipboardOwner;
    QByteArray m_newClipboardOwner;
    QTimer m_timer;
};

#endif // CLIPBOARDOWNERMONITOR_H
