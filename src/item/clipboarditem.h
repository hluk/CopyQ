// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CLIPBOARDITEM_H
#define CLIPBOARDITEM_H

#include <QVariant>

class QByteArray;
class QString;

/**
 * Class for clipboard items in ClipboardModel.
 *
 * Clipboard item stores data of different MIME types and has single default
 * MIME type for displaying the contents.
 */
class ClipboardItem final
{
public:
    ClipboardItem();

    explicit ClipboardItem(const QVariantMap &data);

    /** Compare with other item (using hash). */
    bool operator ==(const ClipboardItem &item) const;

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
    bool setData(const QVariantMap &data);

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

    /** Return data for format. */
    QByteArray data(const QString &format) const { return m_data.value(format).toByteArray(); }

    /** Return hash for item's data. */
    unsigned int dataHash() const;

private:
    void invalidateDataHash();

    QVariantMap m_data;
    mutable unsigned int m_hash;
};

#endif // CLIPBOARDITEM_H
