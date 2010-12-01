#ifndef CLIENT_SERVER_H
#define CLIENT_SERVER_H
#include <QString>
#include <QList>
#include <QMimeData>
#include <QStringList>
#include <QByteArray>

#include <QLocalServer>
#include <QLocalSocket>


bool readBytes(QIODevice *socket, QByteArray &msg);
QLocalServer *newServer(const QString &name, QObject *parent=NULL);
QString serverName(const QString &name);
QMimeData *cloneData(const QMimeData &data, const QStringList *formats=NULL);

#endif // CLIENT_SERVER_H
