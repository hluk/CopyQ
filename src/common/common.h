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

#ifndef COMMON_H
#define COMMON_H

#include <QClipboard>
#include <QFont>
#include <QFontMetrics>
#include <QString>
#include <QtGlobal> // Q_WS_*
#include <QVariantMap>

#ifdef COPYQ_DEBUG
#   include <QDebug>
#   define QDEBUG() qDebug()
#else
#   define QDEBUG() if (false) DummyDebug()
class DummyDebug {
public:
    template <typename T>
    DummyDebug operator<<(T&&) { return DummyDebug(); }
};
#endif

class QAction;
class QByteArray;
class QDropEvent;
class QIODevice;
class QKeyEvent;
class QMimeData;
class QPoint;
class QProcess;
class QStringList;
class QTemporaryFile;
class QTimer;
class QWidget;

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

#ifdef COPYQ_WS_X11
// QClipboard::supportsSelection() must return true.
#   define HAS_MOUSE_SELECTIONS
#endif

bool isMainThread();

const QMimeData *clipboardData(QClipboard::Mode mode = QClipboard::Clipboard);

/** Clone data for given formats (text or HTML will be UTF8 encoded). */
QVariantMap cloneData(const QMimeData &data, QStringList formats);

/** Clone all data as is. */
QVariantMap cloneData(const QMimeData &data);

QMimeData* createMimeData(const QVariantMap &data);

/** Return true only if data map contains useful data. */
bool containsAnyData(const QVariantMap &data);

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
 * @return shortened text (or same text if not too long)
 */
QString elideText(const QString &text, const QFont &font = QFont(),
                  const QString &format = QString(), bool escapeAmpersands = false,
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
QString textLabelForData(const QVariantMap &data, const QFont &font = QFont(),
                         const QString &format = QString(), bool escapeAmpersands = false,
                         int maxWidthPixels = -1, int maxLines = 1);

void renameToUnique(QString *name, const QStringList &names);

bool openTemporaryFile(QTemporaryFile *file, const QString &suffix = ".ini");

void initSingleShotTimer(QTimer *timer, int milliseconds, const QObject *object = nullptr, const char *slot = nullptr);

QString dataToText(const QByteArray &bytes, const QString &mime);

bool clipboardContains(QClipboard::Mode mode, const QVariantMap &data);

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
