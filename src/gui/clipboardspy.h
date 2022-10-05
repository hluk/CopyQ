// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CLIPBOARDSPY_H
#define CLIPBOARDSPY_H

#include "common/clipboardmode.h"
#include "platform/platformnativeinterface.h"

#include <QObject>

class ClipboardSpy final : public QObject
{
    Q_OBJECT
public:
    explicit ClipboardSpy(ClipboardMode mode, const QByteArray &owner);

    /// Actively wait for clipboard/selection to change.
    void wait(int ms = 2000, int checkIntervalMs = 100);

    bool setClipboardData(const QVariantMap &data);

    QByteArray currentOwnerData() const;

    void stop();

signals:
    void changed();
    void stopped();

private:
    bool check();

    ClipboardMode m_mode;
    PlatformClipboardPtr m_clipboard;
    QByteArray m_oldOwnerData;
};

#endif // CLIPBOARDSPY_H
