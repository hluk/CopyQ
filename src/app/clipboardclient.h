// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CLIPBOARDCLIENT_H
#define CLIPBOARDCLIENT_H

#include "app.h"

#include <QObject>
#include <QStringList>

/**
 * Application client.
 *
 * Sends a command to the server and exits after the command is executed.
 * Exit code is same as exit code send by ClipboardServer::sendMessage().
 * Also the received message is printed on standard output (if exit code is
 * zero) or standard error output.
 */
class ClipboardClient final : public QObject, public App
{
    Q_OBJECT

public:
    ClipboardClient(
            int &argc, char **argv, const QStringList &arguments, const QString &sessionName);

signals:
    void functionCallResultReceived(const QByteArray &returnValue);
    void inputDialogFinished(const QByteArray &data);
    void dataReceived(const QByteArray &data);

private:
    void onMessageReceived(const QByteArray &data, int messageCode);
    void onDisconnected();
    void onConnectionFailed();

    void start(const QStringList &arguments);
};

#endif // CLIPBOARDCLIENT_H
