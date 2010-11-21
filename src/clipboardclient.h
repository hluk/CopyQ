#ifndef CLIPBOARDCLIENT_H
#define CLIPBOARDCLIENT_H

#include "app.h"
#include <QLocalSocket>

class ClipboardClient : public App
{
    Q_OBJECT
public:
    ClipboardClient(int &argc, char **argv);

private:
    QLocalSocket *m_client;
    QByteArray m_msg;

private slots:
    void sendMessage();
    void readyRead();
    void readFinnished();
    void error(QLocalSocket::LocalSocketError);
};

#endif // CLIPBOARDCLIENT_H
