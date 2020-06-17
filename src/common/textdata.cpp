/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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
    uint seed = 0;
    QtPrivate::QHashCombine hash;

    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        const auto &mime = it.key();

        // Skip some special data.
        if (mime == mimeWindowTitle || mime == mimeOwner || mime == mimeClipboardMode)
            continue;

        seed = hash(seed, mime);
        seed = hash(seed, data[mime].toByteArray());
    }

    return seed;
}

QString quoteString(const QString &str)
{
    return QLocale().quoteString(str);
}

QString escapeHtml(const QString &str)
{
    return escapeHtmlSpaces(str.toHtmlEscaped());
}

QString getTextData(const QByteArray &bytes)
{
    // QString::fromUtf8(bytes) ends string at first '\0'.
    return QString::fromUtf8( bytes.constData(), bytes.size() );
}

QString getTextData(const QVariantMap &data, const QString &mime)
{
    const auto it = data.find(mime);
    if ( it != data.constEnd() )
        return getTextData( it->toByteArray() );

    return QString();
}

QString getTextData(const QVariantMap &data)
{
    for (const auto &mime : {mimeText, mimeUriList}) {
        const auto it = data.find(mime);
        if ( it != data.constEnd() )
            return getTextData( it->toByteArray() );
    }

    return QString();
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
