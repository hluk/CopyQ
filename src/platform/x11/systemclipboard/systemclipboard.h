/*
   Copyright (C) 2020 David Edmundson <davidedmundson@kde.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the Lesser GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

   You should have received a copy of the Lesser GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#pragma once

#include <QObject>
#include <QClipboard>
#include <QMimeData>

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
    //maybe I should unique_ptr it to be expressive, but then I don't match QClipboard?
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
