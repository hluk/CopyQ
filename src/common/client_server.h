/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#ifndef CLIENT_SERVER_H
#define CLIENT_SERVER_H

#include <QClipboard>
#include <QFont>
#include <QFontMetrics>
#include <QtGlobal> // Q_WS_*

// Application version
#define COPYQ_VERSION "1.9.3 beta"

class QAction;
class QByteArray;
class QIODevice;
class QLocalServer;
class QMimeData;
class QString;
class QStringList;

/** Command status. */
typedef enum {
    /** Script finished */
    CommandFinished = 0,
    /** Command invocation error. */
    CommandError = 1,
    /** Bad command syntax. */
    CommandBadSyntax = 2,
    /** Command successfully invoked. */
    CommandSuccess,
    /** Activate window */
    CommandActivateWindow,
    /** Command to exit application. */
    CommandExit
} CommandStatus;

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

#ifdef COPYQ_LOG_DEBUG
#   define COPYQ_LOG(msg) log(msg, LogDebug)
#   define COPYQ_LOG_VERBOSE(msg) if ( qgetenv("COPYQ_VERBOSE") == "1" ) log(msg, LogDebug);
#else
#   define COPYQ_LOG(msg)
#   define COPYQ_LOG_VERBOSE(msg)
#endif

enum LogLevel {
#ifdef COPYQ_LOG_DEBUG
    LogDebug,
#endif
    LogNote,
    LogWarning,
    LogError
};

extern const QString mimeWindowTitle;
extern const QString mimeItems;
extern const QString mimeItemNotes;
extern const QString mimeApplicationSettings;
#ifdef COPYQ_WS_X11
extern const QString mimeClipboardMode;
#endif

QString escapeHtml(const QString &str);

void log(const QString &text, const LogLevel level = LogNote);

bool isMainThread();

const QMimeData *clipboardData(QClipboard::Mode mode = QClipboard::Clipboard);

bool readBytes(QIODevice *socket, qint64 size, QByteArray *bytes);
bool readMessage(QIODevice *socket, QByteArray *msg);
bool writeMessage(QIODevice *socket, const QByteArray &msg);

QLocalServer *newServer(const QString &name, QObject *parent=NULL);
QString serverName(const QString &name);
QString clipboardServerName();
QString clipboardMonitorServerName();

uint hash(const QMimeData &data, const QStringList &formats);

QMimeData *cloneData(const QMimeData &data, const QStringList *formats=NULL);

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
QString textLabelForData(const QMimeData &data, const QFont &font = QFont(),
                         const QString &format = QString(), bool escapeAmpersands = false,
                         int maxWidthPixels = 320, int maxLines = 1);

#endif // CLIENT_SERVER_H
