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

#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>

class Arguments;
class ClientSocket;

class Client : public QObject
{
    Q_OBJECT
public:
    explicit Client(QObject *parent = nullptr);

protected:
    void startClientSocket(const QString &serverName, int argc, char **argv, int skipArgc, int messageCode);

    void sendMessage(const QByteArray &message, int messageCode);

private slots:
    /** Message received from server. */
    virtual void onMessageReceived(const QByteArray &message, int messageCode) = 0;

    /** Server connection closed. */
    virtual void onDisconnected() = 0;

    /** Called if connection cannot be estabilished. */
    virtual void onConnectionFailed() = 0;

private:
    ClientSocket *m_socket;
};

#endif // CLIENT_H
