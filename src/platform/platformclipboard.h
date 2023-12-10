// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PLATFORMCLIPBOARD_H
#define PLATFORMCLIPBOARD_H

#include "common/clipboardmode.h"

#include <QObject>
#include <QVariantMap>

class QMimeData;

/**
 * Interface for clipboard.
 */
class PlatformClipboard : public QObject
{
    Q_OBJECT
public:
    /**
     * Starts emitting changed.
     */
    virtual void startMonitoring(const QStringList &formats) = 0;

    virtual void setMonitoringEnabled(ClipboardMode mode, bool enable) = 0;

    /**
     * Return clipboard data containing specified @a formats if available.
     */
    virtual QVariantMap data(ClipboardMode mode, const QStringList &formats) const = 0;

    /**
     * Set data to clipboard.
     */
    virtual void setData(ClipboardMode mode, const QVariantMap &dataMap) = 0;

    virtual const QMimeData *mimeData(ClipboardMode mode) const = 0;

    virtual bool isSelectionSupported() const = 0;

    virtual bool isHidden(const QMimeData &data) const = 0;

    virtual void setClipboardOwner(const QString &owner) = 0;

signals:
    /// Notifies about clipboard changes.
    void changed(ClipboardMode mode);
};

#endif // PLATFORMCLIPBOARD_H
