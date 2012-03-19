#include "client_server.h"

#include <iostream>
#include <unistd.h>
#include <cstdio>
#include <assert.h>

#include <QObject>
#include <QThread>
#include <QLocalSocket>
#include <QApplication>

#ifdef Q_WS_X11
#include <X11/Xlib.h>
#include <X11/keysym.h>
#elif defined Q_WS_WIN
#include <windows.h>
#endif

// msleep function (portable)
class Sleeper : public QThread
{
public:
    static void msleep(unsigned long msecs)
    {
        QThread::msleep(msecs);
    }
};

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

    msg = level_id.arg(text).toLocal8Bit();
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

    if ( !readBytes(socket, (qint64)sizeof(len), &bytes) )
        return false;
    QDataStream(bytes) >> len;

    if ( !readBytes(socket, (qint64)len, msg) )
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

    if ( !server->listen(name) ) {
        QLocalSocket socket;
        socket.connectToServer(name);
        if ( socket.waitForConnected(2000) ) {
            QDataStream out(&socket);
            out << (quint32)0;
        } else {
            // server is not running but socket is open -> remove socket
            QLocalServer::removeServer(name);
            server->listen(name);
        }
    }

    return server;
}

QString serverName(const QString &name)
{
#ifdef Q_WS_WIN
    QString sessionID( getenv("USERNAME") );
#else
    QString sessionID( QString::number(getuid(), 16) );
#endif
    return name + sessionID;
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
            if ( mime[0].isLower() )
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
#elif defined Q_WS_WIN
    SetForegroundWindow(wid);
    SetWindowPos(wid, HWND_TOP, 0, 0, 0, 0,
                 SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
#endif // TODO: focus window on Mac
}
