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

#include "common/client_server.h"
#include "common/contenttype.h"
#include "item/serialize.h"

#include <QByteArray>
#include <QDataStream>
#include <QMimeData>
#include <QString>
#include <QStringList>
#include <QVariant>

ClipboardItem::ClipboardItem()
    : m_data(new QMimeData)
    , m_hash(0)
{
}

ClipboardItem::~ClipboardItem()
{
    delete m_data;
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

    // rewrite all original data, except notes, with edited text
    const QByteArray notes = m_data->data(mimeItemNotes);
    delete m_data;
    m_data = data;
    if ( !notes.isEmpty() )
        m_data->setData(mimeItemNotes, notes);
    updateDataHash();
}

void ClipboardItem::setData(const QVariant &value)
{
    // rewrite all original data, except notes, with edited text
    const QByteArray notes = m_data->data(mimeItemNotes);
    m_data->clear();
    m_data->setText( value.toString() );
    if ( !notes.isEmpty() )
        m_data->setData(mimeItemNotes, notes);
    updateDataHash();
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
    QStringList formats = m_data->formats();

    // Skip some special data.
    formats.removeAll(mimeClipboardMode);
    formats.removeAll(mimeWindowTitle);

    m_hash = hash(*m_data, formats);
}

QDataStream &operator<<(QDataStream &stream, const ClipboardItem &item)
{
    stream << *item.data();
    return stream;
}

QDataStream &operator>>(QDataStream &stream, ClipboardItem &item)
{
    QMimeData *data = new QMimeData();
    stream >> *data;
    if ( stream.status() == QDataStream::ReadCorruptData )
        log( QObject::tr("Clipboard history file copyq.dat is corrupted!"), LogError );
    item.setData(data);
    return stream;
}
