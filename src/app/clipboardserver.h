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
#include "common/client_server.h"

#include <QMap>
#include <QProcess>
#include <QThreadPool>

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
    ClipboardServer(int &argc, char **argv, const QString &sessionName = QString());
    ~ClipboardServer();

    /** Returns true if server is listening to incoming client connections. */
    bool isListening() const;

    /**
     * Execute command in different thread.
     */
    void doCommand(
            const Arguments &args, //!< Contains command and its arguments.
            QLocalSocket *client = NULL //!< For sending responses.
            );

    /** Stop monitor application. */
    void stopMonitoring();

    /** Start monitor application. */
    void startMonitoring();

    /** Send configuration to monitor. */
    void loadMonitorSettings();

    /** Return true if monitor is running. */
    bool isMonitoring();

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

signals:
    void terminateClientThreads();

private:
    QLocalServer *m_server;
    MainWindow* m_wnd;
    RemoteProcess *m_monitor;
    bool m_checkclip;
    uint m_lastHash;
    QMap<QxtGlobalShortcut*, Arguments> m_shortcutActions;
    QThreadPool m_clientThreads;

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

    /** Send message to client. */
    void sendMessage(
            QLocalSocket* client, //!< Client socket.
            const QByteArray &message, //!< Message for client.
            int exitCode = 0 //!< Exit code for client (non-zero for an error).
            );
};

#endif // CLIPBOARDSERVER_H
