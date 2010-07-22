#include "client_server.h"

void serialize_args(const QStringList &args, QString &msg) {
    // serialize arguments to QString
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out << args;
    foreach( char byte, bytes.toBase64() )
        msg.append(byte);
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
            ok = 0;
            return false;
        }
    }

    // remove parsed argument from list
    list.pop_front();
    return true;
}
