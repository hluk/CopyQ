/*
    Copyright (c) 2018, Lukas Holecek <hluk@email.cz>

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

#ifndef PLATFORMCLIPBOARD_H
#define PLATFORMCLIPBOARD_H

#include "common/clipboardmode.h"

#include <QObject>
#include <QVariantMap>

/**
 * Interface for clipboard.
 *
 * Define signal changed(ClipboardMode) in derived class.
 * This signal notifies about clipboard changes.
 */
class PlatformClipboard : public QObject
{
public:
    /**
     * Set MIME types to monitor.
     */
    virtual void setFormats(const QStringList &formats) = 0;

    /**
     * Return clipboard data containing specified @a formats if available.
     */
    virtual QVariantMap data(ClipboardMode mode, const QStringList &formats) const = 0;

    /**
     * Set data to clipboard.
     */
    virtual void setData(ClipboardMode mode, const QVariantMap &dataMap) = 0;
};

#endif // PLATFORMCLIPBOARD_H
