/*
    Copyright (c) 2012, Lukas Holecek <hluk@email.cz>

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

#include <QWidget> // WId
#include <QMimeData>
#include <QClipboard>

class QString;
class QIODevice;
class QStringList;
class QByteArray;
class QLocalServer;

#if !defined(Q_WS_X11) && !defined(Q_WS_WIN) && !defined(Q_WS_MAC)
#define NO_GLOBAL_SHORTCUTS
#endif

enum LogLevel {
    LogNote,
    LogWarning,
    LogError
};

void log(const QString &text, const LogLevel level = LogNote);
const QMimeData *clipboardData(QClipboard::Mode mode = QClipboard::Clipboard);
void setClipboardData(QMimeData *data, QClipboard::Mode mode);
bool readBytes(QIODevice *socket, qint64 size, QByteArray *bytes);
bool readMessage(QIODevice *socket, QByteArray *msg);
void writeMessage(QIODevice *socket, const QByteArray &msg);
QLocalServer *newServer(const QString &name, QObject *parent=NULL);
QString serverName(const QString &name);
uint hash(const QMimeData &data, const QStringList &formats);
QMimeData *cloneData(const QMimeData &data, const QStringList *formats=NULL);
void raiseWindow(WId wid);

#endif // CLIENT_SERVER_H
