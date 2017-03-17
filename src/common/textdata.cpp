/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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

#include "textdata.h"

#include "common/mimetypes.h"

#include <QLocale>
#include <QString>
#include <Qt>
#if QT_VERSION < 0x050000
#   include <QTextDocument> // Qt::escape()
#endif

namespace {

QString escapeHtmlSpaces(const QString &str)
{
    QString str2 = str;
    return str2
            .replace(' ', "&nbsp;")
            .replace('\n', "<br />");
}

} // namespace

uint hash(const QVariantMap &data)
{
    uint hash = 0;

    for ( const auto &mime : data.keys() ) {
        // Skip some special data.
        if (mime == mimeWindowTitle || mime == mimeOwner || mime == mimeClipboardMode)
            continue;
        hash ^= qHash(data[mime].toByteArray()) + qHash(mime);
    }

    return hash;
}

QString quoteString(const QString &str)
{
#if QT_VERSION >= 0x040800
    return QLocale().quoteString(str);
#else
    return '"' + str + '"';
#endif
}

QString escapeHtml(const QString &str)
{
#if QT_VERSION < 0x050000
    return escapeHtmlSpaces(Qt::escape(str));
#else
    return escapeHtmlSpaces(str.toHtmlEscaped());
#endif
}

QString getTextData(const QByteArray &bytes)
{
    // QString::fromUtf8(bytes) ends string at first '\0'.
    return QString::fromUtf8( bytes.constData(), bytes.size() );
}

QString getTextData(const QVariantMap &data, const QString &mime)
{
    return getTextData( data.value(mime).toByteArray() );
}

QString getTextData(const QVariantMap &data)
{
    return getTextData(data, data.contains(mimeText) ? mimeText : mimeUriList);
}

void setTextData(QVariantMap *data, const QString &text, const QString &mime)
{
    data->insert(mime, text.toUtf8());
}

void setTextData(QVariantMap *data, const QString &text)
{
    setTextData(data, text, mimeText);
}

QVariantMap createDataMap(const QString &format, const QVariant &value)
{
    QVariantMap dataMap;
    dataMap.insert(format, value);
    return dataMap;
}

QVariantMap createDataMap(const QString &format, const QByteArray &value)
{
    return createDataMap( format, QVariant(value) );
}

QVariantMap createDataMap(const QString &format, const QString &value)
{
    return createDataMap( format, value.toUtf8() );
}
