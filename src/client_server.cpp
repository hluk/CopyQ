#include "client_server.h"
#include <iostream>
#include <QDebug>

void serialize_args(const QStringList &args, QString &msg) {
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out << args;
    msg = bytes.toBase64().constData();
}

void deserialize_args(const QString &msg, QStringList &args) {
    QByteArray bytes(msg.toAscii());
    bytes = QByteArray::fromBase64(bytes);
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
bool parse(QStringList &list, QString *str, int *num)
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
#if defined(Q_OS_WIN)
        Sleep(DWORD(1000));
#else
        const struct timespec ts = { 1, 0 };
        nanosleep(&ts, NULL);
#endif
    }

    // catch messages
    QObject::connect(
            client, SIGNAL(messageReceived(const QString&)),
            this, SLOT(handleMessage(const QString&)) );
}

void Client::handleMessage(const QString &message)
{
    QStringList list;
    deserialize_args(message,list);

    if ( list.isEmpty() )
        return;

    // first item is client output
    std::cout << list.takeFirst().toLocal8Bit().constData();

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
