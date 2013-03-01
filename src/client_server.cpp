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

#include "client_server.h"

#include <cstdio>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMimeData>
#include <QObject>
#include <QProcessEnvironment>
#include <QThread>
#if QT_VERSION < 0x050000
#   include <QTextDocument> // Qt::escape()
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

    if (level == LogNote)
        level_id = QObject::tr("CopyQ: %1\n");
    else if (level == LogWarning)
        level_id = QObject::tr("CopyQ warning: %1\n");
    else if (level == LogError)
        level_id = QObject::tr("CopyQ ERROR: %1\n");
#ifdef COPYQ_LOG_DEBUG
    else if (level == LogDebug)
        level_id = QObject::tr("CopyQ DEBUG: %1\n");
#endif

    msg = level_id.arg(text);

    QFile f;
    f.open(stderr, QIODevice::WriteOnly);
    f.write( msg.toLocal8Bit() );
}

bool isMainThread()
{
    return QThread::currentThread() == QApplication::instance()->thread();
}

const QMimeData *clipboardData(QClipboard::Mode mode)
{
    Q_ASSERT( isMainThread() );
    COPYQ_LOG( QString("Getting %1 data.").arg(mode == QClipboard::Clipboard ? "clipboard"
                                                                             : "selection") );
    const QMimeData *data = QApplication::clipboard()->mimeData(mode);
    COPYQ_LOG(data != NULL ? "Got data." : "Data is NULL!");
    return data;
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

    if ( readBytes(socket, sizeof(len), &bytes) ) {
        QDataStream(bytes) >> len;

        if ( readBytes(socket, len, msg) ) {
            COPYQ_LOG( QString("Message read (%1 bytes).").arg(msg->size()) );
            return true;
        }
    }

    COPYQ_LOG("ERROR: Incorrect message!");

    return false;
}

void writeMessage(QIODevice *socket, const QByteArray &msg)
{
    COPYQ_LOG( QString("Write message (%1 bytes).").arg(msg.size()) );

    QDataStream out(socket);
    // length is serialized as a quint32, followed by msg
    out.writeBytes( msg.constData(), msg.length() );
}

QLocalServer *newServer(const QString &name, QObject *parent)
{
    COPYQ_LOG( QString("Starting server \"%1\".").arg(name) );

    QLocalServer *server = new QLocalServer(parent);

    // check if other server is running
    QLocalSocket socket;
    socket.connectToServer(name);
    if ( socket.waitForConnected(2000) ) {
        // server is running
        QDataStream out(&socket);
        out << (quint32)0;
        COPYQ_LOG( QString("Server \"%1\" already running!").arg(name) );
    } else {
        QLocalServer::removeServer(name);
        server->listen(name);
        COPYQ_LOG( QString("Server \"%1\" started.").arg(name) );
    }

    return server;
}

QString serverName(const QString &name)
{
    return name + QString("_")
#ifdef Q_OS_WIN
            + qgetenv("USERNAME")
#else
            + qgetenv("USER")
#endif
            ;
}

QString clipboardServerName()
{
    return serverName("CopyQ_server");
}

QString clipboardMonitorServerName()
{
    return serverName("CopyQ_monitor_server");
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

void elideText(QAction *act)
{
    QFont font = act->font();
    act->setFont(font);

    QFontMetrics fm(font);
    QString text = act->text().trimmed();
    text = fm.elidedText( text.left(512).simplified(), Qt::ElideRight, 240 );

    // Escape all ampersands except first one (key hint).
    text.replace( QChar('&'), QString("&&") );
    int i = text.indexOf( QChar('&') );
    act->setText( text.left(qMax(0, i)) + text.mid(i + 1) );
}
