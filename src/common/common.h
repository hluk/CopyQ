// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef COMMON_H
#define COMMON_H

#include <QtGlobal> // Q_WS_*
#include <QStringList>
#include <QVariantMap>

class QByteArray;
class QDropEvent;
class QFont;
class QKeyEvent;
class QMimeData;
class QProcess;
class QString;

bool isMainThread();

QByteArray makeClipboardOwnerData();

/** Clone data for given formats (text or HTML will be UTF8 encoded). */
QVariantMap cloneData(const QMimeData &data, QStringList formats, bool *abortCloning = nullptr);

/** Clone all data as is. */
QVariantMap cloneData(const QMimeData &data);

QString cloneText(const QMimeData &data);

QMimeData* createMimeData(const QVariantMap &data);

/** Return true if clipboard content was created by any session of this application. */
bool anySessionOwnsClipboardData(const QVariantMap &data);

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
 * @return shortened text (or same text if not too long)
 */
QString elideText(const QString &text, const QFont &font,
                  const QString &format, bool escapeAmpersands = false,
                  int maxWidthPixels = -1, int maxLines = 1);

/**
 * Show small label for data.
 *
 * @param data  data
 * @param font  font for label
 * @param format  optional format for result, e.g. "-- %1 --"
 * @param escapeAmpersands  escape ampersand characters for labes
 *          (unescaped ampersand is used for shortcut key in labels)
 * @param maxWidthPixels  maximum width in pixels
 * @param maxLines  maximum number of lines
 *
 * @return result text
 */
QString textLabelForData(const QVariantMap &data, const QFont &font,
                         const QString &format, bool escapeAmpersands = false,
                         int maxWidthPixels = -1, int maxLines = 1);

QString textLabelForData(const QVariantMap &data);

void renameToUnique(QString *name, const QStringList &names);

QString dataToText(const QByteArray &bytes, const QString &mime = QString());

bool isClipboardData(const QVariantMap &data);

/**
 * Handle key for Vi mode.
 */
bool handleViKey(QKeyEvent *event, QObject *eventReceiver);

/**
 * Return true only if tabs can accept the drag'n'drop event.
 */
bool canDropToTab(const QDropEvent &event);

/**
 * Accept any proposed drop action, preferably "move" if items data available.
 */
void acceptDrag(QDropEvent *event);

#endif // COMMON_H
