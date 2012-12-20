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

#include "client_server.h"

#include <iostream>
#include <cstdio>
#include <assert.h>

#ifdef Q_OS_WIN
#  include <stdlib.h> // getenv()
#else
#  include <unistd.h> // getuid()
#endif

#include <QObject>
#include <QLocalSocket>
#include <QApplication>
#include <QLocalServer>
#include <QThread>
#if QT_VERSION < 0x050000
#   include <QTextDocument> // Qt::escape()
#endif

#ifdef Q_WS_X11
#include <X11/Xlib.h>
#include <X11/keysym.h>
#elif defined Q_OS_WIN
#   include <windows.h>
#endif

QString escapeHtml(const QString &str)
{
#if QT_VERSION < 0x050000
    return Qt::escape(str);
#else
    return str.toHtmlEscaped();
#endif
}

void log(const QString &text, const LogLevel level)
{
    QString msg;
    QString level_id;
    QTextStream err(stderr);

    if (level == LogNote)
        level_id = QObject::tr("CopyQ: %1\n");
    else if (level == LogWarning)
        level_id = QObject::tr("CopyQ warning: %1\n");
    else if (level == LogError)
        level_id = QObject::tr("CopyQ ERROR: %1\n");

    msg = level_id.arg(text);
    err << msg;
}

bool isMainThread()
{
    return QThread::currentThread() == QApplication::instance()->thread();
}

const QMimeData *clipboardData(QClipboard::Mode mode)
{
    assert( isMainThread() );
    return QApplication::clipboard()->mimeData(mode);
}

void setClipboardData(QMimeData *data, QClipboard::Mode mode)
{
    assert( isMainThread() );
    QApplication::clipboard()->setMimeData(data, mode);
}

bool readBytes(QIODevice *socket, qint64 size, QByteArray *bytes)
{
    qint64 avail, read = 0;
    bytes->clear();
    while (read < size) {
        if ( socket->bytesAvailable() == 0 && !socket->waitForReadyRead(1000) )
            return false;
        avail = qMin( socket->bytesAvailable(), size-read );
        bytes->append( socket->read(avail) );
        read += avail;
    }

    return true;
}

bool readMessage(QIODevice *socket, QByteArray *msg)
{
    QByteArray bytes;
    quint32 len;

    if ( !readBytes(socket, sizeof(len), &bytes) )
        return false;
    QDataStream(bytes) >> len;

    if ( !readBytes(socket, len, msg) )
        return false;

    return true;
}

void writeMessage(QIODevice *socket, const QByteArray &msg)
{
    QDataStream out(socket);
    // length is serialized as a quint32, followed by msg
    out.writeBytes( msg.constData(), msg.length() );
}

QLocalServer *newServer(const QString &name, QObject *parent)
{
    QLocalServer *server = new QLocalServer(parent);

    // check if other server is running
    QLocalSocket socket;
    socket.connectToServer(name);
    if ( socket.waitForConnected(2000) ) {
        // server is running
        QDataStream out(&socket);
        out << (quint32)0;
    } else {
        QLocalServer::removeServer(name);
        server->listen(name);
    }

    return server;
}

QString serverName(const QString &name)
{
#ifdef Q_OS_WIN
    QString sessionID( getenv("USERNAME") );
#else
    QString sessionID( QString::number(getuid(), 16) );
#endif
    return name + sessionID;
}

uint hash(const QMimeData &data, const QStringList &formats)
{
    uint hash = 0;

    QByteArray bytes;
    foreach ( const QString &mime, formats ) {
        bytes = data.data(mime);
        hash ^= qHash(bytes) + qHash(mime);
    }

    return hash;
}

QMimeData *cloneData(const QMimeData &data, const QStringList *formats)
{
    QMimeData *newdata = new QMimeData;
    if (formats) {
        foreach (const QString &mime, *formats) {
            QByteArray bytes = data.data(mime);
            if ( !bytes.isEmpty() )
                newdata->setData(mime, bytes);
        }
    } else {
        foreach ( const QString &mime, data.formats() ) {
            // ignore uppercase mimetypes (e.g. UTF8_STRING, TARGETS, TIMESTAMP)
            if ( !mime.isEmpty() && mime[0].isLower() )
                newdata->setData(mime, data.data(mime));
        }
    }
    return newdata;
}

void raiseWindow(WId wid)
{
#ifdef Q_WS_X11
    Display *dsp = XOpenDisplay(NULL);
    if (dsp) {
        XEvent e;
        e.xclient.type = ClientMessage;
        e.xclient.message_type = XInternAtom(dsp, "_NET_ACTIVE_WINDOW", False);
        e.xclient.display = dsp;
        e.xclient.window = wid;
        e.xclient.format = 32;
        e.xclient.data.l[0] = 1;
        e.xclient.data.l[1] = CurrentTime;
        e.xclient.data.l[2] = 0;
        e.xclient.data.l[3] = 0;
        e.xclient.data.l[4] = 0;
        XSendEvent(dsp, DefaultRootWindow(dsp),
                   false, SubstructureNotifyMask | SubstructureRedirectMask, &e);
        XRaiseWindow(dsp, wid);
        XSetInputFocus(dsp, wid, RevertToPointerRoot, CurrentTime);
        XCloseDisplay(dsp);
    }
#elif defined Q_OS_WIN
    SetForegroundWindow(wid);
    SetWindowPos(wid, HWND_TOP, 0, 0, 0, 0,
                 SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
#else
    Q_UNUSED(wid);
#endif // TODO: focus window on Mac
}
