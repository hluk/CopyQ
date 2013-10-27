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

#include "clipboarditem.h"

#include "common/common.h"
#include "common/contenttype.h"
#include "item/serialize.h"

#include <QByteArray>
#include <QDataStream>
#include <QMimeData>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace {

void clearDataExceptInternal(QVariantMap *data)
{
    foreach ( const QString &format, data->keys() ) {
        if ( !format.startsWith(MIME_PREFIX) )
            data->remove(format);
    }
}

bool containsInternalFormat(const QList<QString> &keys)
{
    return !keys.isEmpty() && keys[0].startsWith(MIME_PREFIX);
}

} // namespace

ClipboardItem::ClipboardItem()
    : m_data()
    , m_hash(0)
{
}

ClipboardItem::~ClipboardItem()
{
}

bool ClipboardItem::operator ==(const ClipboardItem &item) const
{
    return m_hash == item.m_hash;
}

bool ClipboardItem::operator ==(const QVariantMap &data) const
{
    return m_hash == hash(data);
}

void ClipboardItem::clear()
{
    m_data.clear();
    updateDataHash();
}

void ClipboardItem::setData(const QVariant &value)
{
    // rewrite all original data, except internal data
    clearDataExceptInternal(&m_data);
    m_data.insert( mimeText, value.toString() );
    updateDataHash();
}

void ClipboardItem::setData(const QVariantMap &data)
{
    m_data = data;
    updateDataHash();
}

bool ClipboardItem::updateData(const QVariantMap &data)
{
    bool added = false;
    foreach ( const QString &format, data.keys() ) {
        if ( m_data.value(format) != data[format] ) {
            m_data.insert( format, data[format].toByteArray() );
            added = true;
        }
    }
    return added;
}

void ClipboardItem::removeData(const QString &mimeType)
{
    m_data.remove(mimeType);
    updateDataHash();
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
    return removed;
}

bool ClipboardItem::isEmpty() const
{
    return m_data.isEmpty() || ( m_data.size() == 1 && m_data.contains(mimeWindowTitle) );
}

void ClipboardItem::setData(const QString &mimeType, const QByteArray &data)
{
    m_data.insert(mimeType, data);
    updateDataHash();
}

QString ClipboardItem::text() const
{
    return m_data.value(mimeText).toString();
}

QVariant ClipboardItem::data(int role) const
{
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        if ( m_data.contains(mimeText) )
            return text();
    } else if (role >= Qt::UserRole) {
        if (role == contentType::data) {
            return m_data; // copy-on-write, so this should be fast
        } else if (role == contentType::hasText) {
            return m_data.contains(mimeText);
        } else if (role == contentType::hasHtml) {
            return m_data.contains("text/html");
        } else if (role == contentType::hasNotes) {
            return m_data.contains(mimeItemNotes);
        } else if (role == contentType::text) {
            return m_data.value(mimeText);
        } else if (role == contentType::html) {
            return m_data.value("text/html");
        } else if (role == contentType::notes) {
            return QString::fromUtf8( m_data.value(mimeItemNotes).toByteArray() );
        }
    }

    return QVariant();
}

void ClipboardItem::updateDataHash()
{
    m_hash = hash(m_data);
}

QDataStream &operator<<(QDataStream &stream, const ClipboardItem &item)
{
    stream << item.data();
    return stream;
}

QDataStream &operator>>(QDataStream &stream, ClipboardItem &item)
{
    QVariantMap data;
    stream >> data;
    item.setData(data);
    return stream;
}
