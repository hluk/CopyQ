/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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

#include <QMetaType>
#include <QObject>

#include <memory>

class ClientSocket;
class QLocalServer;

using ClientSocketPtr = std::shared_ptr<ClientSocket>;
Q_DECLARE_METATYPE(ClientSocketPtr)

class Server : public QObject
{
    Q_OBJECT
public:
    explicit Server(const QString &name, QObject *parent = nullptr);

    ~Server();

    void start();

    bool isListening() const;

public slots:
    void close();

signals:
    void newConnection(const ClientSocketPtr &socket);

private slots:
    void onNewConnection();
    void onSocketDestroyed();

private:
    QLocalServer *m_server;
    QObject *m_systemMutex;
    int m_socketCount;
};

#endif // SERVER_H
