/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

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
