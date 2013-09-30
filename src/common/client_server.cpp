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

#include "common/client_server.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMimeData>
#include <QObject>
#include <QThread>
#if QT_VERSION < 0x050000
#   include <QTextDocument> // Qt::escape()
#endif

const QString mimeWindowTitle = "application/x-copyq-owner-window-title";
const QString mimeItems = "application/x-copyq-item";
const QString mimeItemNotes = "application/x-copyq-item-notes";
const QString mimeApplicationSettings = "application/x-copyq-settings";
const QString mimeMessage = "application/x-copyq-message";
#ifdef COPYQ_WS_X11
const QString mimeClipboardMode = "application/x-copyq-clipboard-mode";
#endif

QString quoteString(const QString &str)
{
#if QT_VERSION >= 0x040800
    return QLocale::system().quoteString(str);
#else
    return '"' + str + '"';
#endif
}

QString escapeHtml(const QString &str)
{
#if QT_VERSION < 0x050000
    return Qt::escape(str);
#else
    return str.toHtmlEscaped();
#endif
}

QString createLogMessage(const QString &label, const QString &text, const LogLevel level)
{
    QString levelId;

    if (level == LogNote)
        levelId = QString("%1");
    else if (level == LogWarning)
        levelId = QObject::tr("warning: %1");
    else if (level == LogError)
        levelId = QObject::tr("ERROR: %1");
#ifdef COPYQ_LOG_DEBUG
    else if (level == LogDebug)
        levelId = QString("DEBUG: %1");
#endif

    return label + " " + levelId.arg(text) + "\n";
}

void log(const QString &text, const LogLevel level)
{
    const QString msg = createLogMessage("CopyQ", text, level);

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

    COPYQ_LOG_VERBOSE("Reading message.");

    if ( readBytes(socket, sizeof(len), &bytes) ) {
        QDataStream(bytes) >> len;

        if ( readBytes(socket, len, msg) ) {
            COPYQ_LOG_VERBOSE( QString("Message read (%1 bytes).").arg(msg->size()) );
            return true;
        }
    }

    COPYQ_LOG("ERROR: Incorrect message!");

    return false;
}

bool writeMessage(QIODevice *socket, const QByteArray &msg)
{
    COPYQ_LOG_VERBOSE( QString("Write message (%1 bytes).").arg(msg.size()) );

    QDataStream out(socket);
    // length is serialized as a quint32, followed by msg
    out.writeBytes( msg.constData(), msg.length() );

    if (out.status() != QDataStream::Ok) {
        COPYQ_LOG("Cannot write message!");
        return false;
    }

    COPYQ_LOG_VERBOSE("Message written.");
    return true;
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
    return QCoreApplication::applicationName() + "_"
#ifdef Q_OS_WIN
            + qgetenv("USERNAME")
#else
            + qgetenv("USER")
#endif
            + "_" + name;
}

QString clipboardServerName()
{
    return serverName("s");
}

QString clipboardMonitorServerName()
{
    return serverName("m%1");
}

uint hash(const QMimeData &data, const QStringList &formats)
{
    uint hash = 0;

    QByteArray bytes;
    foreach ( const QString &mime, formats ) {
        // Skip some special data.
        if (mime == mimeWindowTitle)
            continue;
#ifdef COPYQ_WS_X11
        if (mime == mimeClipboardMode)
            continue;
#endif
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

QString elideText(const QString &text, const QFont &font, const QString &format,
                  bool escapeAmpersands, int maxWidthPixels, int maxLines)
{
    const int oldLines = text.count('\n');

    QString newText = text;
    newText.remove(QRegExp("^\\s+"));

    const int newLines = newText.count('\n');

    int lines = 0;
    QString result;
    foreach ( QString line, newText.split('\n') ) {
        if (++lines > maxLines) {
            result.append( QString("...") );
            break;
        }

        // Show triple-dot in middle if text is too long.
        QFontMetrics fm(font);
        const int formatWidth = format.isEmpty() ? 0 : fm.width(format.arg(QString()));
        line = fm.elidedText(line.simplified(), Qt::ElideMiddle, maxWidthPixels - formatWidth);

        if ( !line.isEmpty() ) {
            if ( !result.isEmpty() )
                result.append('\n');
            result.append(line);
        }
    }

    // Escape all ampersands.
    if (escapeAmpersands)
        result.replace( QChar('&'), QString("&&") );

    if (newLines < oldLines && result != "...")
        result.prepend("...");

    return format.isEmpty() ? result : format.arg(result);
}

QString textLabelForData(const QMimeData &data, const QFont &font, const QString &format,
                         bool escapeAmpersands, int maxWidthPixels, int maxLines)
{
    const QStringList formats = data.formats();
    QString label;

    if ( formats.indexOf("text/plain") != -1 ) {
        const QString text = data.text();
        const int n = text.count(QChar('\n')) + 1;

        if (n > 1)
            label = QObject::tr("%1 (%n lines)", "Label for multi-line text in clipboard", n);
        else
            label = QString("%1");

        label = label.arg( quoteString("%1") );

        if (!format.isEmpty())
            label = format.arg(label);

        return elideText(text, font, label, escapeAmpersands, maxWidthPixels, maxLines);
    } else if ( formats.indexOf(QRegExp("^image/.*")) != -1 ) {
        label = QObject::tr("<IMAGE>", "Label for image in clipboard");
    } else if ( formats.indexOf(QString("text/uri-list")) != -1 ) {
        label = QObject::tr("<FILES>", "Label for URLs/files in clipboard");
    } else if ( formats.isEmpty() || (formats.size() == 1 && formats[0] == mimeWindowTitle) ) {
        label = QObject::tr("<EMPTY>", "Label for empty clipboard");
    } else {
        label = QObject::tr("<DATA>", "Label for data in clipboard");
    }

    if (!format.isEmpty())
        label = format.arg(label);

    return label;
}
