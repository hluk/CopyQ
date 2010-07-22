#ifndef CLIENT_SERVER_H
#define CLIENT_SERVER_H
#include <QString>
#include <QStringList>

void serialize_args(const QStringList &args, QString &msg);
void deserialize_args(const QString &msg, QStringList &args);
bool parse(QStringList &list, QString *str = NULL, int *num = NULL);
#endif // CLIENT_SERVER_H
