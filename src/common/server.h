/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SERVER_H
#define SERVER_H

#include <QObject>
#include <QSharedPointer>

class Arguments;
class ClientSocket;
class QLocalServer;

class Server : public QObject
{
    Q_OBJECT
public:
    static Server *create(const QString &name);

    void startInThread();

signals:
    void newConnection(const Arguments &args, ClientSocket *socket);

private slots:
    void onNewConnection();
    void start();
    void close();

protected:
    Server(QLocalServer *server);

private:
    QLocalServer *m_server;
};

#endif // SERVER_H
