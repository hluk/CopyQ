/*
    Copyright (c) 2009, Lukas Holecek <hluk@email.cz>

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

ClipboardItem::ClipboardItem(const ClipboardModel *parent) :
        m_parent(parent)
{
    m_data = new QMimeData;
}

ClipboardItem::~ClipboardItem()
{
    delete m_data;
}

void ClipboardItem::clear()
{
    //m_data->clear();
    delete m_data;
    m_data = new QMimeData;
}

void ClipboardItem::setData(QMimeData *data)
{
    delete m_data;
    m_data = data;

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
}

void ClipboardItem::setData(const QString &mimeType, const QByteArray &data)
{
    m_data->setData(mimeType, data);
    setPreferredFormat();
}

QString ClipboardItem::text() const
{
    return m_data->text();
}

QVariant ClipboardItem::data(int role) const
{
    if (role == Qt::DisplayRole)
        return toHtml();
    else if (role == Qt::EditRole)
        return text();
    else if (role == Qt::UserRole)
        return m_mimeType;
    else
        return QVariant();
}

QString ClipboardItem::highlightedHtml() const
{
    const QRegExp *re;
    QString highlight;

    re = m_parent ? m_parent->search() : NULL;
    if ( !re || re->isEmpty() ) {
        // show html if not searching or the text is not too large
        if ( m_mimeType.endsWith("html") )
            return m_data->html();
        else
            return escape( text() );
    }

    const QString str = text();

    int a = 0;
    int b = re->indexIn(str, a);
    int len;

    while ( b != -1 ) {
        len = re->matchedLength();
        if ( len == 0 )
            break;

        highlight.append( escape(str.mid(a, b-a)) );
        highlight.append( "<span class=\"em\">" );
        highlight.append( escape(str.mid(b, len)) );
        highlight.append( "</span>" );

        a = b + len;
        b = re->indexIn(str, a);
    }

    // filter items
    if ( highlight.isEmpty() ) {
        return QString();
    } else {
        if ( a != str.length() )
            highlight += escape(str.mid(a));
        // highlight matched
        return highlight;
    }
}

void ClipboardItem::setPreferredFormat()
{
    if ( !m_parent ) return;

    // get right mime type
    QStringList formats = m_data->formats();
    const QStringList &tryformats = m_parent->formats();
    // default mime type is the first in the list
    m_mimeType = formats.first();
    // try formats
    foreach(QString format, tryformats) {
        if( formats.contains(format) ) {
            m_mimeType = format;
            break;
        }
    }
}

const QVariant ClipboardItem::toHtml() const
{
    QVariantList lst;
    QString html;

    if ( m_mimeType.startsWith(QString("image")) ) {
        html = QString("<img src=\"data://1\" />");
        lst.append( m_data->data(m_mimeType) );
    } else {
        html = highlightedHtml();
        int i;
        i = html.indexOf("<!--EndFragment-->");
        if (i >= 0)
            html.resize(i);
        i = html.indexOf("</body>", 0, Qt::CaseInsensitive);
        if (i >= 0)
            html.resize(i);
        html.remove("<!--StartFragment-->");
    };

    html.append("<div id=\"formats\">");
    QStringList formats = m_data->formats();
    int len = formats.size();
    for( int i = 0; i<len; ++i ) {
        const QString &it = formats[i];
        if ( it == m_mimeType )
            html.append(" <span class=\"format current\">");
        else
            html.append(" <span class=\"format\">");
        html.append(it);
        html.append("</span> ");
    }
    html.append("</div>");

    lst.prepend(html);

    return QVariant(lst);
}

QDataStream &operator<<(QDataStream &stream, const ClipboardItem &item)
{
    const QMimeData *data = item.data();
    QByteArray bytes;
    stream << data->formats().length();
    foreach( QString mime, data->formats() ) {
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
        item.setPreferredFormat();
    }

    return stream;
}
