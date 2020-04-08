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

#ifndef TEXTDATA_H
#define TEXTDATA_H

#include <QVariantMap>

class QByteArray;
class QString;

uint hash(const QVariantMap &data);

QString quoteString(const QString &str);

QString escapeHtml(const QString &str);

QString getTextData(const QByteArray &bytes);

/**
 * Get given text format from data; null string if not available.
 * Assumes that text data is UTF8 encoded.
 */
QString getTextData(const QVariantMap &data, const QString &mime);

/** Helper function that calls getTextData(data, "text/plain"). */
QString getTextData(const QVariantMap &data);

void setTextData(QVariantMap *data, const QString &text, const QString &mime);

void setTextData(QVariantMap *data, const QString &text);

QVariantMap createDataMap(const QString &format, const QVariant &value);

QVariantMap createDataMap(const QString &format, const QByteArray &value);

QVariantMap createDataMap(const QString &format, const QString &value);

#endif // TEXTDATA_H
