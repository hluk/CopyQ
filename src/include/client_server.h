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
#include <QtGlobal> // Q_WS_*
#include <QWidget> // WId

// Application version
#define COPYQ_VERSION "1.8.0"

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
    CommandActivateWindow
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
#else
#   define COPYQ_LOG(msg)
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

QString escapeHtml(const QString &str);

void log(const QString &text, const LogLevel level = LogNote);

bool isMainThread();

const QMimeData *clipboardData(QClipboard::Mode mode = QClipboard::Clipboard);

bool readBytes(QIODevice *socket, qint64 size, QByteArray *bytes);
bool readMessage(QIODevice *socket, QByteArray *msg);
void writeMessage(QIODevice *socket, const QByteArray &msg);

QLocalServer *newServer(const QString &name, QObject *parent=NULL);
QString serverName(const QString &name);
QString clipboardServerName();
QString clipboardMonitorServerName();

uint hash(const QMimeData &data, const QStringList &formats);

QMimeData *cloneData(const QMimeData &data, const QStringList *formats=NULL);

QString elideText(const QString &text, int maxLength);
void elideText(QAction *act);

#endif // CLIENT_SERVER_H
