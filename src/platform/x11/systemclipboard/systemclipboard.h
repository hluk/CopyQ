/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <QClipboard>
#include <QMimeData>
#include <QObject>

/**
 * This class mimics QClipboard but unlike QClipboard it will continue
 * to get updates even when our window does not have focus.
 *
 * This may require extra access permissions
 */
class SystemClipboard : public QObject
{
    Q_OBJECT
public:
    /**
     * Returns a shared global SystemClipboard instance
     */
    static SystemClipboard *instance();

    /**
     * Sets the clipboard to the new contents
     * The clpboard takes ownership of mime
     */
    // maybe I should unique_ptr it to be expressive, but then I don't match QClipboard?
    virtual void setMimeData(QMimeData *mime, QClipboard::Mode mode) = 0;
    /**
     * Clears the current clipboard
     */
    virtual void clear(QClipboard::Mode mode) = 0;
    /**
     * Returns the current mime data received by the clipboard
     */
    virtual const QMimeData *mimeData(QClipboard::Mode mode) const = 0;
Q_SIGNALS:
    void changed(QClipboard::Mode mode);

protected:
    SystemClipboard(QObject *parent);
};
