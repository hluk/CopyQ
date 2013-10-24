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

void clearDataExceptInternal(QMimeData *data)
{
    foreach ( const QString &format, data->formats() ) {
        if ( !format.startsWith("application/x-copyq-") )
            data->removeFormat(format);
    }
}

bool needUpdate(const QVariantMap &data, const QMimeData &itemData)
{
    QStringList formats = itemData.formats();
    if ( !data.contains(mimeItemNotes) )
        formats.removeOne(mimeItemNotes);
    if ( formats.toSet() != data.keys().toSet() )
        return true;

    foreach (const QString &format, formats) {
        if ( itemData.data(format) != data[format].toByteArray() )
            return true;
    }

    return false;
}

} // namespace

ClipboardItem::ClipboardItem()
    : m_data(new QMimeData)
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

bool ClipboardItem::operator ==(const QMimeData &data) const
{
    return m_hash == hash(data, data.formats());
}

void ClipboardItem::clear()
{
    m_data->clear();
    updateDataHash();
}

void ClipboardItem::setData(QMimeData *data)
{
    Q_ASSERT(data != NULL);

    // if new data contains internal data (MIME starts with "application/x-copyq-"), update all data
    // otherwise rewrite all original data, except internal data
    const QStringList newFormats = data->formats();
    bool updateAll = newFormats.indexOf( QRegExp("^application/x-copyq-.*") ) != -1;
    foreach ( const QString &format, m_data->formats().toSet().subtract(newFormats.toSet()) ) {
        if ( updateAll || format.startsWith("application/x-copyq-") )
            data->setData( format, m_data->data(format) );
    }

    m_data.reset(data);
    updateDataHash();
}

void ClipboardItem::setData(const QVariant &value)
{
    // rewrite all original data, except internal data
    clearDataExceptInternal( m_data.data() );
    m_data->setText( value.toString() );
    updateDataHash();
}

bool ClipboardItem::setData(const QVariantMap &data)
{
    if ( !needUpdate(data, *m_data) )
        return false;

    clearDataExceptInternal( m_data.data() );
    foreach ( const QString &format, data.keys() )
        m_data->setData( format, data[format].toByteArray() );
    updateDataHash();

    return true;
}

void ClipboardItem::removeData(const QString &mimeType)
{
    m_data->removeFormat(mimeType);
    updateDataHash();
}

bool ClipboardItem::isEmpty() const
{
    const QStringList formats = m_data->formats();
    return formats.isEmpty() || (formats.size() == 1 && formats[0] == mimeWindowTitle);
}

void ClipboardItem::setData(const QString &mimeType, const QByteArray &data)
{
    m_data->setData(mimeType, data);
    updateDataHash();
}

void ClipboardItem::setData(int formatIndex, const QByteArray &data)
{
    if ( formatIndex >= 0 && formatIndex < m_data->formats().size() )
        m_data->setData( m_data->formats().value(formatIndex), data );
}

QString ClipboardItem::text() const
{
    return m_data->text();
}

QVariant ClipboardItem::data(int role) const
{
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        if ( m_data->hasText() )
            return text();
    } else if (role >= Qt::UserRole) {
        if (role == contentType::formats) {
            return m_data->formats();
        } else if (role == contentType::hasText) {
            return m_data->hasText();
        } else if (role == contentType::hasHtml) {
            return m_data->hasHtml();
        } else if (role == contentType::hasNotes) {
            return !m_data->data(mimeItemNotes).isEmpty();
        } else if (role == contentType::text) {
            return m_data->text();
        } else if (role == contentType::html) {
            return m_data->html();
        } else if (role == contentType::imageData) {
            return m_data->imageData();
        } else if (role == contentType::notes) {
            return QString::fromUtf8( m_data->data(mimeItemNotes) );
        } else if (role >= contentType::firstFormat) {
            return m_data->data( m_data->formats().value(role - contentType::firstFormat) );
        }
    }

    return QVariant();
}

void ClipboardItem::updateDataHash()
{
    m_hash = hash( *m_data, m_data->formats() );
}

QDataStream &operator<<(QDataStream &stream, const ClipboardItem &item)
{
    stream << item.data();
    return stream;
}

QDataStream &operator>>(QDataStream &stream, ClipboardItem &item)
{
    QMimeData *data = new QMimeData();
    stream >> *data;
    item.setData(data);
    return stream;
}
