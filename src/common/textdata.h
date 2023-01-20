// SPDX-License-Identifier: GPL-3.0-or-later

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

/// Returns text without accents/diacritics.
QString accentsRemoved(const QString &text);

#endif // TEXTDATA_H
