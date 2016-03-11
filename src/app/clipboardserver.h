/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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
#include "common/server.h"
#include "gui/configtabshortcuts.h"
#include "gui/mainwindow.h"

#include <QMap>
#include <QProcess>
#include <QTimer>
#include <QThreadPool>
#include <QVariantMap>
#include <QWidget>

class Arguments;
class ClientSocket;
class ItemFactory;
class RemoteProcess;
class QxtGlobalShortcut;
class QSessionManager;

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

    /** Stop monitor application. */
    void stopMonitoring();

    /** Start monitor application. */
    void startMonitoring();

    /** Return true if monitor is running. */
    bool isMonitoring();

    /**
     * Create global shortcut for command.
     *
     * @see shortcutActivated()
     */
    void createGlobalShortcut(const QKeySequence &shortcut, const Command &command);

public slots:
    /** Load @a item data to clipboard. */
    void changeClipboard(const QVariantMap &data, QClipboard::Mode mode);

    /** Load settings. */
    void loadSettings();

    /** Send configuration to monitor. */
    void loadMonitorSettings();

signals:
    void terminateClientThreads();

protected:
    bool eventFilter(QObject *object, QEvent *ev);

private slots:
    /**
     * Execute command in different thread.
     */
    void doCommand(
            const Arguments &args, //!< Contains command and its arguments.
            ClientSocket *client = NULL //!< For sending responses.
            );

    /** New message from monitor process. */
    void newMonitorMessage(const QByteArray &message);

    /** An error occurred on monitor connection. */
    void monitorConnectionError();

    /** Shortcut was pressed on host system. */
    void shortcutActivated(QxtGlobalShortcut *shortcut);

    void removeGlobalShortcuts();
    void createGlobalShortcuts();

    /** Clean up before quitting. */
    void onAboutToQuit();

    /**
     * Save all data before quitting and if some commands are still executing,
     * allow user to cancel quitting (if session manager supports it).
     */
    void onCommitData(QSessionManager &sessionManager);

    /** Quit application, but ask to cancel exit if there are any active commands. */
    void maybeQuit();

private:
    /** Ask to cancel application exit if there are any active commands. */
    bool askToQuit();

    void terminateThreads();

    MainWindow* m_wnd;
    RemoteProcess *m_monitor;
    QMap<QxtGlobalShortcut*, Command> m_shortcutActions;
    QThreadPool m_clientThreads;
    QTimer m_ignoreKeysTimer;
    ItemFactory *m_itemFactory;
};

#endif // CLIPBOARDSERVER_H
