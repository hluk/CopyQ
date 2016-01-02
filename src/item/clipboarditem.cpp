/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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

#include "clipboarditem.h"

#include "common/common.h"
#include "common/contenttype.h"
#include "common/mimetypes.h"
#include "item/serialize.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace {

void clearDataExceptInternal(QVariantMap *data)
{
    foreach ( const QString &format, data->keys() ) {
        if ( !format.startsWith(COPYQ_MIME_PREFIX) )
            data->remove(format);
    }
}

} // namespace

ClipboardItem::ClipboardItem()
    : m_data()
    , m_hash(0)
{
}

bool ClipboardItem::operator ==(const ClipboardItem &item) const
{
    return dataHash() == item.dataHash();
}

void ClipboardItem::setText(const QString &text)
{
    foreach ( const QString &format, m_data.keys() ) {
        if ( format.startsWith("text/") )
            m_data.remove(format);
    }

    setTextData(&m_data, text);

    invalidateDataHash();
}

bool ClipboardItem::setData(const QVariantMap &data)
{
    if (m_data == data)
        return false;

    m_data = data;
    invalidateDataHash();
    return true;
}

bool ClipboardItem::updateData(const QVariantMap &data)
{
    const int oldSize = m_data.size();
    foreach ( const QString &format, data.keys() ) {
        if ( !format.startsWith(COPYQ_MIME_PREFIX) ) {
            clearDataExceptInternal(&m_data);
            break;
        }
    }

    bool changed = (oldSize != m_data.size());

    foreach ( const QString &format, data.keys() ) {
        if ( m_data.value(format) != data[format] ) {
            m_data.insert(format, data[format]);
            changed = true;
        }
    }

    invalidateDataHash();

    return changed;
}

void ClipboardItem::removeData(const QString &mimeType)
{
    m_data.remove(mimeType);
    invalidateDataHash();
}

bool ClipboardItem::removeData(const QStringList &mimeTypeList)
{
    bool removed = false;

    foreach (const QString &mimeType, mimeTypeList) {
        if ( m_data.contains(mimeType) ) {
            m_data.remove(mimeType);
            removed = true;
        }
    }

    if (removed)
        invalidateDataHash();

    return removed;
}

void ClipboardItem::setData(const QString &mimeType, const QByteArray &data)
{
    m_data.insert(mimeType, data);
    invalidateDataHash();
}

QVariant ClipboardItem::data(int role) const
{
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        if ( m_data.contains(mimeText) )
            return getTextData(m_data);
        if ( m_data.contains(mimeUriList) )
            return getTextData(m_data, mimeUriList);
    } else if (role >= Qt::UserRole) {
        if (role == contentType::data) {
            return m_data; // copy-on-write, so this should be fast
        } else if (role == contentType::hash) {
            return dataHash();
        } else if (role == contentType::hasText) {
            return m_data.contains(mimeText) || m_data.contains(mimeUriList);
        } else if (role == contentType::hasHtml) {
            return m_data.contains(mimeHtml);
        } else if (role == contentType::hasNotes) {
            return m_data.contains(mimeItemNotes);
        } else if (role == contentType::text) {
            return getTextData(m_data);
        } else if (role == contentType::html) {
            return getTextData(m_data, mimeHtml);
        } else if (role == contentType::notes) {
            return getTextData(m_data, mimeItemNotes);
        }
    }

    return QVariant();
}

unsigned int ClipboardItem::dataHash() const
{
    if (m_hash == 0)
        m_hash = hash(m_data);

    return m_hash;
}

void ClipboardItem::invalidateDataHash()
{
    m_hash = 0;
}
