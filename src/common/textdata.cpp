// SPDX-License-Identifier: GPL-3.0-or-later

#include "textdata.h"

#include "common/mimetypes.h"

#include <QLocale>
#include <QString>
#include <Qt>

#include <algorithm>
#include <iterator>

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
    for (const auto &mime : {mimeTextUtf8, mimeText, mimeUriList}) {
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
    data->remove(mimeTextUtf8);
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

QString accentsRemoved(const QString &text)
{
    if (text.isEmpty())
        return {};

    QString result = text.normalized(QString::NormalizationForm_D);
    const auto newEnd = std::remove_if(
        std::begin(result), std::end(result),
        [](QChar c){ return c.category() == QChar::Mark_NonSpacing; });
    const auto newSize = std::distance(std::begin(result), newEnd);
    result.resize(newSize);
    return result;
}
