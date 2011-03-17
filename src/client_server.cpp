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
    const char *msg;
    QString level_id;

    if (level == LogNote)
        level_id = QObject::tr("CopyQ: %1\n");
    else if (level == LogWarning)
        level_id = QObject::tr("CopyQ warning: %1\n");
    else if (level == LogError)
        level_id = QObject::tr("CopyQ ERROR: %1\n");

    msg = level_id.arg(text).toLocal8Bit().constData();
    fprintf(stderr, msg);
}

bool readBytes(QIODevice *socket, QByteArray &msg)
{
    QDataStream in(socket);

    quint32 sz;
    while( socket->bytesAvailable() < (int)sizeof(quint32) ) {
        if ( !socket->waitForReadyRead(4000) )
            return false;
    }
    in >> sz;

    while( socket->bytesAvailable() < sz ) {
        if ( !socket->waitForReadyRead(4000) )
            return false;
    }
    in >> msg;

    return true;
}

QLocalServer *newServer(const QString &name, QObject *parent)
{
    QLocalServer *server = new QLocalServer(parent);

    if ( !server->listen(name) ) {
        QLocalSocket socket;
        socket.connectToServer(name);
        if ( socket.waitForConnected(6000) ) {
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
    return name + QString::number(getuid(), 16);
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
            newdata->setData(mime, data.data(mime));
        }
    }
    return newdata;
}
