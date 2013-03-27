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
#include <QSize>
#include <QString>
#include <QStringList>

class ClipboardModel;
class QMimeData;
class QVariant;

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
    bool operator ==(const QMimeData &data) const;

    /** Return item's plain text. */
    QString text() const;
    /** Return item's HTML text. */
    QString html() const;

    /** Clear item's data */
    void clear();

    /**
     * Set item's data.
     * Item takes ownership of the @a data.
     */
    void setData(QMimeData *data);

    /** Set item's MIME type data. */
    void setData(const QString &mimeType, const QByteArray &data);

    /**
     * Set item's data.
     * Clears all data and saves @a value as text/plain MIME type.
     */
    void setData(const QVariant &value);

    /** Return data for given @a role. */
    QVariant data(int role) const;

    /** Return item's data. */
    QMimeData *data() const { return m_data; }

    /** Return hash for item's data. */
    uint dataHash() const { return m_hash; }

private:
    QMimeData *m_data;
    uint m_hash;
};

/**
 * @defgroup clipboard_item_serialization_operators ClipboardItem Serialization Operators
 * @{
 */
QDataStream &operator<<(QDataStream &stream, const ClipboardItem &item);
QDataStream &operator>>(QDataStream &stream, ClipboardItem &item);
///@}

#endif // CLIPBOARDITEM_H
