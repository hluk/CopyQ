// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PLATFORMWINDOW_H
#define PLATFORMWINDOW_H

#include <QByteArray>

class QString;

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
    virtual void pasteClipboard() = 0;

    /**
     * Copy to clipboard from window.
     */
    virtual void copy() = 0;

    PlatformWindow(const PlatformWindow &) = delete;
    PlatformWindow &operator=(const PlatformWindow &) = delete;
};

#endif // PLATFORMWINDOW_H
