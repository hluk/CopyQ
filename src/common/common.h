/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

#ifndef COMMON_H
#define COMMON_H

#include <QClipboard>
#include <QFont>
#include <QFontMetrics>
#include <QtGlobal> // Q_WS_*
#include <QVariantMap>

// Application version
#define COPYQ_VERSION "2.2.0"

class QAction;
class QByteArray;
class QIODevice;
class QMimeData;
class QString;
class QStringList;

#if QT_VERSION < 0x050000
#   ifdef Q_WS_WIN
#       define Q_OS_WIN
#   elif defined Q_WS_MAC
#       define Q_OS_MAC
#   endif
#endif

#if !defined(COPYQ_WS_X11) && !defined(Q_OS_WIN) && !defined(Q_OS_MAC)
#   define NO_GLOBAL_SHORTCUTS
#endif

QString quoteString(const QString &str);

QString escapeHtml(const QString &str);

bool isMainThread();

const QMimeData *clipboardData(QClipboard::Mode mode = QClipboard::Clipboard);

uint hash(const QVariantMap &data);

QByteArray getUtf8Data(const QMimeData &data, const QString &format);

/**
 * Get given text format from data; null string if not available.
 * Assumes that text data is UTF8 encoded.
 */
QString getTextData(const QVariantMap &data, const QString &mime);

/** Helper function that calls getTextData(data, "text/plain"). */
QString getTextData(const QVariantMap &data);

void setTextData(QVariantMap *data, const QString &text, const QString &mime);

void setTextData(QVariantMap *data, const QString &text);

/** Clone data for given formats (text or HTML will be UTF8 encoded). */
QVariantMap cloneData(const QMimeData &data, const QStringList &formats);

/** Clone all data as is. */
QVariantMap cloneData(const QMimeData &data);

QMimeData* createMimeData(const QVariantMap &data);

QVariantMap createDataMap(const QString &format, const QVariant &value);

QVariantMap createDataMap(const QString &format, const QByteArray &value);

QVariantMap createDataMap(const QString &format, const QString &value);

/** Return true only if data map contains useful data. */
bool containsData(const QVariantMap &data);

/** Return true if clipboard content was created by any session of this application. */
bool ownsClipboardData(const QVariantMap &data);

/**
 * Elide text with given font or to maximum length
 *
 * @param text              text to shorten
 * @param format            optional format for result, e.g. "-- %1 --"
 * @param font              used to calculate ideal width if @a maxChars is zero or negative
 * @param escapeAmpersands  escape ampersands because only key hint can follow it
 * @param maxWidthPixels    maximum width of result text in pixels
 * @param maxLines          maximum number of lines
 *
 * @param shortened text (or same text if not too long)
 */
QString elideText(const QString &text, const QFont &font = QFont(),
                  const QString &format = QString(), bool escapeAmpersands = false,
                  int maxWidthPixels = 320, int maxLines = 1);

/**
 * Show small label for data.
 *
 * @param data      data
 * @param maxChars  maximum characters if data contain text; ignored if @a act is not null
 * @param maxLines  maximum lines if data contain text; ignored if @a act is not null
 * @param act       if not null, used to calculate ideal width (instead of @a maxChars) and
 *                  call QAction::setText() with result
 * @param format    optional format for result, e.g. "-- %1 --"
 *
 * @return result text
 */
QString textLabelForData(const QVariantMap &data, const QFont &font = QFont(),
                         const QString &format = QString(), bool escapeAmpersands = false,
                         int maxWidthPixels = 320, int maxLines = 1);

/**
 * Shortcut to remove items, formats etc.
 */
QString shortcutToRemove();

void renameToUnique(QString *name, const QStringList &names);

#endif // COMMON_H
