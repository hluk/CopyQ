// SPDX-License-Identifier: GPL-3.0-or-later

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
