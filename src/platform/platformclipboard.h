// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include "common/clipboardmode.h"

#include <QList>
#include <QObject>
#include <QVariantMap>
#include <memory>

class QMimeData;
class PlatformClipboard;

enum class ClipboardModeFlag {
    Clipboard = 0x1,
    Selection = 0x2,
};
Q_DECLARE_FLAGS(ClipboardModeMask, ClipboardModeFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(ClipboardModeMask)

ClipboardModeFlag clipboardModeFlag(ClipboardMode mode);

class ClipboardConnection final : public QObject
{
    Q_OBJECT
public:
    ~ClipboardConnection() override;

    const QStringList &formats() const { return m_formats; }
    ClipboardModeMask modes() const { return m_modes; }

signals:
    void changed(ClipboardMode mode);

private:
    friend class PlatformClipboard;
    ClipboardConnection(const QStringList &formats, ClipboardModeMask modes, PlatformClipboard *clipboard);

    QStringList m_formats;
    ClipboardModeMask m_modes;
    PlatformClipboard *m_clipboard = nullptr;
};

using ClipboardConnectionPtr = std::unique_ptr<ClipboardConnection>;

/**
 * Interface for clipboard.
 */
class PlatformClipboard : public QObject
{
    Q_OBJECT
public:
    ~PlatformClipboard() override;

    ClipboardConnectionPtr createConnection(const QStringList &formats, ClipboardModeMask modes);

    /**
     * Return clipboard data containing specified @a formats if available.
     */
    virtual QVariantMap data(ClipboardMode mode, const QStringList &formats) const = 0;

    /**
     * Set data to clipboard.
     */
    virtual void setData(ClipboardMode mode, const QVariantMap &dataMap) = 0;
    virtual void setRawData(ClipboardMode mode, QMimeData *mimeData) = 0;

    virtual const QMimeData *mimeData(ClipboardMode mode) const = 0;

    virtual bool isSelectionSupported() const = 0;

    virtual bool isHidden(const QMimeData &data) const = 0;

    virtual void setClipboardOwner(const QString &owner) = 0;

protected:
    virtual void startMonitoringBackend(const QStringList &formats, ClipboardModeMask modes) = 0;
    virtual void stopMonitoringBackend() = 0;
    virtual void updateMonitoringSubscription(const QStringList &formats, ClipboardModeMask modes);
    void emitConnectionChanged(ClipboardMode mode);

private:
    friend class ClipboardConnection;
    void registerConnection(ClipboardConnection *connection);
    void unregisterConnection(ClipboardConnection *connection);
    void setMonitoringState();

    QList<ClipboardConnection*> m_connections;
    QStringList m_monitoredFormats;
    ClipboardModeMask m_monitoredModes;
    bool m_isMonitoringActive = false;
};
