#include "client_server.h"
#include <iostream>
#include <QDebug>
#include <QThread>

// msleep function (portable)
class Sleeper : public QThread
{
public:
    static void msleep(unsigned long msecs)
    {
        QThread::msleep(msecs);
    }
};

void serialize_args(const DataList &args, QString &msg) {
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out << args;
    msg = qCompress(bytes).toBase64();
}

void deserialize_args(const QString &msg, DataList &args) {
    QByteArray bytes;
    bytes = qUncompress( QByteArray::fromBase64(msg.toAscii()) );
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

void Client::connect() {
    int argc = 0;

    while(1) {
        if (client)
            delete client;
        client = new QtSingleApplication( QString("CopyQclient"), argc, (char**)NULL );

        if ( !client->isRunning() )
            break;

        // wait for other client to be close
        Sleeper::msleep(200);
    }

    // catch messages
    QObject::connect(
            client, SIGNAL(messageReceived(const QString&)),
            this, SLOT(handleMessage(const QString&)) );
}

void Client::handleMessage(const QString &message)
{
    DataList list;
    deserialize_args(message,list);

    if ( list.isEmpty() )
        return;

    // first item is client output
    QByteArray bytes = list.takeFirst();
    std::cout.write( bytes.constData(), bytes.length() );

    if ( list.isEmpty() )
        return;

    // second item (if defined) is exit code
    client->exit( list.takeFirst().toInt() );
}

int Client::exec()
{
    if ( !client || client->isRunning() )
        return -1;

    // run client
    return client->exec();
}
