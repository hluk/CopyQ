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

void serialize_args(const DataList &args, QByteArray &msg)
{
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out << args;
    msg = qCompress(bytes);
}

void deserialize_args(const QByteArray &msg, DataList &args)
{
    QByteArray bytes = qUncompress(msg);
    QDataStream in(&bytes, QIODevice::ReadOnly);
    in >> args;
}

/**
  @fn parse
  @arg list list of arguments to parse
  @arg str pointer to store string or NULL
  @arg num pointer to store number or NULL
  @return true only if
          str not NULL and list not empty or
          str NULL and num not NULL and first item in list is number
          str and num NULL (skip list item)
  @par Parses first item in list as string or as number (if str is NULL).
       If successfully parsed the item is saved in appropriate argument
       (str or num) and remove from the list.
  */
bool parse(DataList &list, QString *str, int *num)
{
    if ( list.isEmpty() )
        return false;

    QString tmp = list[0];
    if (str)
        *str = tmp;
    else if (num) {
        bool ok = false;
        *num = tmp.toInt(&ok);
        if (!ok) {
            *num = 0;
            return false;
        }
    }

    // remove parsed argument from list
    list.pop_front();
    return true;
}
