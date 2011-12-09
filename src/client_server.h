#ifndef CLIENT_SERVER_H
#define CLIENT_SERVER_H
#include <QString>
#include <QList>
#include <QMimeData>
#include <QStringList>
#include <QByteArray>

#include <QLocalServer>
#include <QLocalSocket>

enum LogLevel {
    LogNote,
    LogWarning,
    LogError
};

void log(const QString &text, const LogLevel level = LogNote);
bool readBytes(QIODevice *socket, qint64 size, QByteArray *bytes);
bool readMessage(QIODevice *socket, QByteArray *msg);
void writeMessage(QIODevice *socket, const QByteArray &msg);
QLocalServer *newServer(const QString &name, QObject *parent=NULL);
QString serverName(const QString &name);
QMimeData *cloneData(const QMimeData &data, const QStringList *formats=NULL);

#endif // CLIENT_SERVER_H
