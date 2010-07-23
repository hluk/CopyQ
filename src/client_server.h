#ifndef CLIENT_SERVER_H
#define CLIENT_SERVER_H
#include <QString>
#include <QStringList>
#include <qtsingleapplication.h>

void serialize_args(const QStringList &args, QString &msg);
void deserialize_args(const QString &msg, QStringList &args);
bool parse(QStringList &list, QString *str = NULL, int *num = NULL);

// NOTE: server part is MainWindow
class Client : public QObject
{
    Q_OBJECT

    public:
        Client(): client(NULL) {};
        void connect();
        int exec();
    public slots:
        void handleMessage(const QString& message);
    private:
        QtSingleApplication *client;
};
#endif // CLIENT_SERVER_H
