#ifndef CLIENT_SERVER_H
#define CLIENT_SERVER_H
#include <QString>
#include <QList>
#include <QMimeData>
#include <QStringList>
#include <QByteArray>

#include <QLocalServer>
#include <QLocalSocket>

typedef QList<QByteArray> DataList;

bool readBytes(QIODevice *socket, QByteArray &msg);
QLocalServer *newServer(const QString &name, QObject *parent=NULL);
QString serverName(const QString &name);
QMimeData *cloneData(const QMimeData &data, const QStringList *formats=NULL);
void serialize_args(const DataList &args, QByteArray &msg);
void deserialize_args(const QByteArray &msg, DataList &args);
bool parse(DataList &list, QString *str = NULL, int *num = NULL);

#endif // CLIENT_SERVER_H
