// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include "platform/dummy/dummyclipboard.h"

#include <QByteArray>
#include <QMimeData>
#include <QStringList>
#include <QTimer>
#include <memory>

class GnomeClipboardExtensionClient;

class X11PlatformClipboard final : public DummyClipboard
{
public:
    X11PlatformClipboard();
    ~X11PlatformClipboard() override;

    QVariantMap data(ClipboardMode mode, const QStringList &formats) const override;

    void setData(ClipboardMode mode, const QVariantMap &dataMap) override;
    void setRawData(ClipboardMode mode, QMimeData *mimeData) override;

    bool isSelectionSupported() const override { return true; }

    void setClipboardOwner(const QString &owner) override { m_clipboardOwner = owner; }

protected:
    void startMonitoringBackend(const QStringList &formats, ClipboardModeMask modes) override;
    void stopMonitoringBackend() override;
    void updateMonitoringSubscription(const QStringList &formats, ClipboardModeMask modes) override;
    const QMimeData *mimeData(ClipboardMode mode) const override;
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

    bool isGnomeExtensionAvailable() const;
    void setGnomeExtensionClipboardData(ClipboardMode mode, const QVariantMap &dataMap) const;
    void check();
    void updateClipboardData(ClipboardData *clipboardData);
    void useNewClipboardData(ClipboardData *clipboardData);
    void checkAgainLater(bool clipboardChanged, int interval);
    void onGnomeClipboardChanged(int clipboardType);
    int toGnomeClipboardTypes(ClipboardModeMask modes) const;

    QTimer m_timerCheckAgain;

    ClipboardData m_clipboardData;
    ClipboardData m_selectionData;

    bool m_monitoring = false;

    QString m_clipboardOwner;
    std::unique_ptr<GnomeClipboardExtensionClient> m_gnomeClipboardExtensionClient;
    mutable std::unique_ptr<QMimeData> m_gnomeClipboardMimeData;
    mutable std::unique_ptr<QMimeData> m_gnomeSelectionMimeData;
};
