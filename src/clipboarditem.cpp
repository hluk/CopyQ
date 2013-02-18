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

#include "client_server.h"
#include "clipboardmodel.h"
#include "contenttype.h"

#include <QMimeData>
#include <QVariant>

ClipboardItem::ClipboardItem()
    : m_data(new QMimeData)
    , m_hash(0)
    , m_formats()
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
    m_formats.clear();
    m_hash = hash(*m_data, m_formats);
}

void ClipboardItem::setData(QMimeData *data)
{
    delete m_data;
    m_data = data;
    m_formats = m_data->formats();
    m_hash = hash(*m_data, m_formats);
}

void ClipboardItem::setData(const QVariant &value)
{
    // rewrite all original data with edited text
    m_data->clear();
    m_data->setText( value.toString() );
    m_hash = hash(*m_data, m_formats);
}

void ClipboardItem::setData(const QString &mimeType, const QByteArray &data)
{
    m_data->setData(mimeType, data);
    m_formats.append(mimeType);
    m_hash = hash(*m_data, m_formats);
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
            return formats();
        } else if (role == contentType::text) {
            return m_data->text();
        } else if (role == contentType::html) {
            return m_data->html();
        } else if (role == contentType::imageData) {
            return m_data->imageData();
        } else if (role >= contentType::firstFormat) {
            return m_data->data( formats().value(role - contentType::firstFormat) );
        }
    }

    return QVariant();
}

QDataStream &operator<<(QDataStream &stream, const ClipboardItem &item)
{
    const QMimeData *data = item.data();
    const QStringList &formats = item.formats();
    QByteArray bytes;
    stream << formats.length();
    foreach (const QString &mime, formats) {
        bytes = data->data(mime);
        if ( !bytes.isEmpty() )
            bytes = qCompress(bytes);
        stream << mime << bytes;
    }

    return stream;
}

QDataStream &operator>>(QDataStream &stream, ClipboardItem &item)
{
    int length;

    stream >> length;
    QString mime;
    QByteArray bytes;
    for (int i = 0; i < length; ++i) {
        stream >> mime >> bytes;
        if( !bytes.isEmpty() ) {
            bytes = qUncompress(bytes);
            if (bytes.isEmpty()) {
                log( QObject::tr("Clipboard history file copyq.dat is corrupted!"),
                     LogError );
                break;
            }
        }
        item.setData(mime, bytes);
    }

    return stream;
}
