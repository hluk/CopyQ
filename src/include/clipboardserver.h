/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#ifndef CLIPBOARDSERVER_H
#define CLIPBOARDSERVER_H

#include "app.h"
#include "client_server.h"

#include <QProcess>
#include <QMap>

class Arguments;
class ClipboardBrowser;
class ClipboardItem;
class MainWindow;
class RemoteProcess;
class QLocalServer;
class QLocalSocket;
class QxtGlobalShortcut;

/**
 * The application main server.
 *
 * Creates clipboard monitor process when necessary and serves client commands.
 *
 * If user already run this server isListening() returns false.
 */
class ClipboardServer : public QObject, public App
{
    Q_OBJECT

public:
    ClipboardServer(int &argc, char **argv);
    ~ClipboardServer();

    /** Returns true if server is listening to incoming client connections. */
    bool isListening() const;

    /**
     * Execute command.
     * @return Command status.
     */
    CommandStatus doCommand(
            Arguments &args, //!< Contains command and its arguments.
            QByteArray *response, //!< For storing command response message.
            QLocalSocket *client = NULL //!< For sending responses.
            );

    /** Send message to client. */
    void sendMessage(
            QLocalSocket* client, //!< Client socket.
            const QByteArray &message, //!< Message for client.
            int exitCode = 0 //!< Exit code for client (non-zero for an error).
            );

    /** Helper function for sending a message to a client. */
    void sendMessage(QLocalSocket* client, const QString &message,
                     int exitCode = 0)
    {
        sendMessage( client, message.toLocal8Bit(), exitCode );
    }

    /** Stop monitor application. */
    void stopMonitoring();

    /** Start monitor application. */
    void startMonitoring();

    /** Send configuration to monitor. */
    void loadMonitorSettings();

    /** Return true if monitor is running. */
    bool isMonitoring();

    /** Socket name for server. */
    static QString serverName();

    /** Socket name for monitor. */
    static QString monitorServerName();

    /**
     * Create global shortcut.
     *
     * If shortcut is pressed on host system doCommand() is called with the new
     * arguments assigned to shortcut.
     *
     * @return New arguments assigned to shortcut.
     *
     * @see shortcutActivated()
     */
    Arguments *createGlobalShortcut(
            const QString &shortcut //!< Shortcut string (e.g. "CTRL+ALT+X").
            );

protected:
    bool eventFilter(QObject *object, QEvent *ev);

private:
    QLocalServer *m_server;
    MainWindow* m_wnd;
    RemoteProcess *m_monitor;
    uint m_lastHash;
    QMap<QxtGlobalShortcut*, Arguments> m_shortcutActions;

public slots:
    /** Load @a item data to clipboard. */
    void changeClipboard(const ClipboardItem *item);

    /** Load settings. */
    void loadSettings();

private slots:
    /** A new client connected. */
    void newConnection();

    /** New message from monitor process. */
    void newMonitorMessage(const QByteArray &message);

    /** An error occurred on monitor connection. */
    void monitorConnectionError();

    /** Monitor state changed. */
    void monitorStateChanged(QProcess::ProcessState newState);

    /** Message can be read from monitor standard error output. */
    void monitorStandardError();

    /** Shortcut was pressed on host system. */
    void shortcutActivated(QxtGlobalShortcut *shortcut);
};

#endif // CLIPBOARDSERVER_H
