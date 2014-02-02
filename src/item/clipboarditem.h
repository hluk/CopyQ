/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#ifndef CLIPBOARDITEM_H
#define CLIPBOARDITEM_H

#include <QSharedPointer>
#include <QVariant>

class QByteArray;
class QDataStream;
class QMimeData;
class QString;

/**
 * Class for clipboard items in ClipboardModel.
 *
 * Clipboard item stores data of different MIME types and has single default
 * MIME type for displaying the contents.
 *
 * Clipboard item can be serialized and deserialized using operators << and >>
 * (see @ref clipboard_item_serialization_operators).
 */
class ClipboardItem
{
public:
    ClipboardItem();
    ~ClipboardItem();

    /** Compare with other item (using hash). */
    bool operator ==(const ClipboardItem &item) const;
    /** Compare with other data (using hash). */
    bool operator ==(const QVariantMap &data) const;

    /** Return item's plain text. */
    QString text() const;
    /** Return item's HTML text. */
    QString html() const;

    /** Clear item's data */
    void clear();

    /** Set item's MIME type data. */
    void setData(const QString &mimeType, const QByteArray &data);

    /**
     * Set item's text data.
     * Clears all other text data and saves @a text as text/plain MIME type.
     */
    void setText(const QString &text);

    /**
     * Set formats from map with MIME type as key and data as value.
     */
    void setData(const QVariantMap &data);

    /**
     * Update current data.
     * Clears non-internal data if passed data map contains non-internal data.
     * @return true if any data were changed
     */
    bool updateData(const QVariantMap &data);

    /** Remove item's MIME type data. */
    void removeData(const QString &mimeType);

    /** Remove item's MIME type data. */
    bool removeData(const QStringList &mimeTypeList);

    /** Return data for given @a role. */
    QVariant data(int role) const;

    /** Return item's data. */
    const QVariantMap &data() const { return m_data; }

    /** Return data for format. */
    QByteArray data(const QString &format) const { return m_data.value(format).toByteArray(); }

    /** Return hash for item's data. */
    unsigned int dataHash() const { return m_hash; }

    /** Return true if data are empty. */
    bool isEmpty() const;

private:
    void updateDataHash();

    QVariantMap m_data;
    unsigned int m_hash;
};

typedef QSharedPointer<ClipboardItem> ClipboardItemPtr;

#endif // CLIPBOARDITEM_H
