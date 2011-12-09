#include "client_server.h"
#include <iostream>
#include <QObject>
#include <QThread>
#include <unistd.h>
#include <cstdio>

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

bool readBytes(QIODevice *socket, qint64 size, QByteArray *bytes)
{
    qint64 avail, read = 0;
    bytes->clear();
    while (read < size) {
        if ( !socket->waitForReadyRead(1000) )
            return false;
        avail = qMin( socket->bytesAvailable(), size-read );
        bytes->append( socket->read(avail) );
        read += avail;
    }

    return true;
}

bool readMessage(QIODevice *socket, QByteArray *msg)
{
    if ( !socket->waitForReadyRead(1000) )
        return false;
    *msg = socket->readAll();
    return true;
}

void writeMessage(QIODevice *socket, const QByteArray &msg)
{
    socket->write(msg);
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
        for(int i=0; i<formats->length(); ++i) {
            const QString &mime = formats->at(i);
            QByteArray bytes = data.data(mime);
            if ( !bytes.isEmpty() )
                newdata->setData(mime, bytes);
        }
    } else {
        foreach( QString mime, data.formats() ) {
            // ignore uppercase mimetypes (e.g. UTF8_STRING, TARGETS, TIMESTAMP)
            if ( mime[0].isLower() )
                newdata->setData(mime, data.data(mime));
        }
    }
    return newdata;
}
