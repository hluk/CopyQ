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

#ifndef COMMON_H
#define COMMON_H

#include "common/clipboardmode.h"

#include <QtGlobal> // Q_WS_*
#include <QVariantMap>

class QAction;
class QByteArray;
class QClipboard;
class QDropEvent;
class QFont;
class QIODevice;
class QKeyEvent;
class QMimeData;
class QPoint;
class QProcess;
class QString;
class QStringList;
class QWidget;

#if !defined(COPYQ_WS_X11) && !defined(Q_OS_WIN) && !defined(Q_OS_MAC)
#   define NO_GLOBAL_SHORTCUTS
#endif

#ifdef COPYQ_WS_X11
// QClipboard::supportsSelection() must return true.
#   define HAS_MOUSE_SELECTIONS
#endif

bool isMainThread();

const QMimeData *clipboardData(ClipboardMode mode = ClipboardMode::Clipboard);

QByteArray makeClipboardOwnerData();

QByteArray clipboardOwnerData(ClipboardMode mode);

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
 * Terminate process or kill if it takes too long.
 */
void terminateProcess(QProcess *p);

/**
 * Return true only if tabs can accept the drag'n'drop event.
 */
bool canDropToTab(const QDropEvent &event);

/**
 * Accept any proposed drop action, preferably "move" if items data available.
 */
void acceptDrag(QDropEvent *event);

#endif // COMMON_H
