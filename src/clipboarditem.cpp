/*
    Copyright (c) 2012, Lukas Holecek <hluk@email.cz>

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
#include "clipboardmodel.h"
#include "client_server.h"
#include <QImage>

ClipboardItem::ClipboardItem(const ClipboardModel *parent)
    : m_parent(parent)
    , m_data(new QMimeData)
    , m_mimeType()
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

    setPreferredFormat();
}

void ClipboardItem::setFormat(const QString &mimeType)
{
    m_mimeType = mimeType;
}

void ClipboardItem::setData(const QVariant &value)
{
    // rewrite all original data with edited text
    m_data->clear();
    m_data->setText( value.toString() );
    m_mimeType = QString("text/plain");
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
    if (role == Qt::DisplayRole) {
        if ( m_mimeType.startsWith(QString("image")) ) {
            QPixmap pix;
            pixmap(&pix, m_mimeType);
            return pix;
        } else if ( m_data->hasImage() ) {
            QPixmap pix;
            pixmap(&pix);
            return pix;
        } else if ( m_mimeType.endsWith("html") ) {
            return html();
        }
    } else if (role == Qt::EditRole) {
        if ( m_data->hasText() )
            return text();
        else if ( m_mimeType.startsWith("text") )
            return QString::fromLocal8Bit( m_data->data(m_mimeType) );
    } else if (role == Qt::UserRole) {
        return format();
    }

    return QVariant();
}

QString ClipboardItem::html() const
{
    if ( m_mimeType.endsWith("html") ) {
        return m_data->html();
    } else {
        return escape( text() );
    }
}

void ClipboardItem::pixmap(QPixmap *pix, const QString &mimeType) const
{
    QByteArray data = m_data->data("image/x-copyq-thumbnail");

    if (!data.isEmpty()) {
        QDataStream in(&data, QIODevice::ReadOnly);
        in >> *pix;
    }

    QSize size = m_parent ? m_parent->maxImageSize() : QSize(320, 240);
    int w = size.width();
    int h = size.height();

    if ( data.isEmpty() ||
         (w > 0 && pix->width() != w) ||
         (h > 0 && pix->height() != h) )
    {
        if ( mimeType.isEmpty() )
            pix->fromImage( m_data->imageData().value<QImage>() );
        else
            pix->loadFromData( m_data->data(mimeType), mimeType.toLatin1() );

        if (w > 0 && pix->width() > w && pix->width()/w > pix->height()/h) {
            *pix = pix->scaledToWidth(w);
        } else if ( h > 0 && pix->height() > size.height() ) {
            *pix = pix->scaledToHeight(h);
        }

        data.clear();
        QDataStream out(&data, QIODevice::WriteOnly);
        out << *pix;
        m_data->setData("image/x-copyq-thumbnail", data);
    }
}

void ClipboardItem::setPreferredFormat()
{
    if ( !m_parent ) return;

    // get right mime type -- default mime type is the first in the list
    m_mimeType = m_formats.isEmpty() ? QString("text/plain") : m_formats.first();

    // try formats
    foreach (const QString &format, m_parent->formats()) {
        if( m_formats.contains(format) ) {
            m_mimeType = format;
            break;
        }
    }
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
    item.setPreferredFormat();

    return stream;
}
