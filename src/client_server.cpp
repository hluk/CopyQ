#include "client_server.h"
#include <iostream>
#include <QDebug>
#include <QThread>
#include <unistd.h>

// msleep function (portable)
class Sleeper : public QThread
{
public:
    static void msleep(unsigned long msecs)
    {
        QThread::msleep(msecs);
    }
};

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
