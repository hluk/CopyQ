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

#ifndef CONTENTTYPE_H
#define CONTENTTYPE_H

#include <Qt>

/**
 * Enum values are used in ClipboardModel class to fetch or set data from ClipboardItem.
 * @see ClipboardModel:setData(), ClipboardModel::data()
 */
namespace contentType {

enum {
    /**
     * Set/get data as QVarianMap (key is MIME type and value is QByteArray).
     */
    data = Qt::UserRole,

    /**
     * Update existing data. Clears non-internal data if passed data map contains non-internal data.
     */
    updateData,

    /**
     * Remove formats (QStringList of MIME types).
     */
    removeFormats,

    /**
     * Item hash
     */
    hash,

    hasText,
    hasHtml,
    text,
    html,
    notes,

    /// Item color (string expression as used in themes).
    color,

    /// If true, hide content of item (not notes, tags etc.).
    isHidden
};

}

#endif // CONTENTTYPE_H
