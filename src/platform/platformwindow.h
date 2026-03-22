// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include <QByteArray>

class QString;
class QWidget;

/**
 * Window storage class for platform. Used to raise/focus window and paste to window.
 */
class PlatformWindow
{
public:
    PlatformWindow() = default;

    virtual ~PlatformWindow() = default;

    /**
     * Return window title text.
     */
    virtual QString getTitle() = 0;

    /**
     * Raise/focus window.
     */
    virtual void raise() = 0;

    /**
     * Paste clipboard content to window.
     */
    virtual bool pasteFromClipboard() = 0;

    /**
     * Copy to clipboard from window.
     */
    virtual bool copyToClipboard() = 0;

    /**
     * Return true if this native window belongs to the given widget.
     */
    virtual bool matchesWidget(const QWidget *widget) const = 0;

    PlatformWindow(const PlatformWindow &) = delete;
    PlatformWindow &operator=(const PlatformWindow &) = delete;
};
