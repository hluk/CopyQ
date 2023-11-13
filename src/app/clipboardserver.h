// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CLIPBOARDSERVER_H
#define CLIPBOARDSERVER_H

#include "app.h"
#include "common/clipboardmode.h"
#include "common/command.h"
#include "common/server.h"
#include "common/clientsocket.h"
#include "gui/clipboardbrowsershared.h"

#include <QMap>
#include <QPointer>
#include <QTimer>

class Action;
class ActionHandler;
class AppConfig;
class ItemFactory;
class MainWindow;
class NotificationDaemon;
class ScriptableProxy;
class Server;
class QxtGlobalShortcut;
class QApplication;
class QSessionManager;
struct NotificationButton;

/**
 * The application main server.
 *
 * Creates clipboard monitor process when necessary and serves client commands.
 *
 * If user already run this server isListening() returns false.
 */
class ClipboardServer final : public QObject, public App
{
    Q_OBJECT

public:
    ClipboardServer(QApplication *app, const QString &sessionName);
    ~ClipboardServer();

    /** Stop monitor application. */
    void stopMonitoring();

    /** Start monitor application. */
    void startMonitoring();

    /**
     * Create global shortcut for command.
     *
     * @see shortcutActivated()
     */
    void createGlobalShortcut(const QKeySequence &shortcut, const Command &command);

    /** Load settings. */
    void loadSettings(AppConfig *appConfig);

signals:
    void closeClients();

protected:
    bool eventFilter(QObject *object, QEvent *ev) override;

private:
    void onClientNewConnection(const ClientSocketPtr &client);
    void onClientMessageReceived(const QByteArray &message, int messageCode, ClientSocketId clientId);
    void onClientDisconnected(ClientSocketId clientId);
    void onClientConnectionFailed(ClientSocketId clientId);

    /** An error occurred on monitor connection. */
    void onMonitorFinished();

    void onNotificationButtonClicked(const NotificationButton &button);

    /** Shortcut was pressed on host system. */
    void shortcutActivated(QxtGlobalShortcut *shortcut);

    void removeGlobalShortcuts();

    /** Called when new commands are available. */
    void onCommandsSaved(const QVector<Command> &commands);

    /** Clean up before quitting. */
    void onAboutToQuit();

    /**
     * Save all data before quitting and if some commands are still executing,
     * allow user to cancel quitting (if session manager supports it).
     */
    void onCommitData(QSessionManager &sessionManager);

    /**
     * Save session name for next time.
     */
    void onSaveState(QSessionManager &sessionManager);

    void onDisableClipboardStoringRequest(bool disabled);

    /** Quit application, but ask to cancel exit if there are any active commands. */
    void maybeQuit();

    /** Ask to cancel application exit if there are any active commands. */
    bool askToQuit();

    bool hasRunningCommands() const;

    void terminateClients(int waitMs);

    void waitForClientsToFinish(int waitMs);
    void waitForCallbackToFinish();

    void callback(const QString &scriptFunction);

    ClientSocketPtr findClient(int actionId);

    void sendActionData(int actionId, const QByteArray &bytes);

    void cleanDataFiles();

    Server *m_server = nullptr;
    MainWindow* m_wnd = nullptr;
    QPointer<Action> m_monitor;
    QPointer<Action> m_callback;
    bool m_exitting = false;
    bool m_ignoreNewConnections = false;
    QMap<QxtGlobalShortcut*, Command> m_shortcutActions;
    QTimer m_ignoreKeysTimer;
    QTimer m_updateThemeTimer;
    uint m_monitorCommandsStateHash = 0;
    int m_textTabSize = 8;
    bool m_saveOnDeactivate = true;

    ClipboardBrowserSharedPtr m_sharedData;

    QMap<int, QByteArray> m_actionDataToSend;
    QTimer m_timerClearUnsentActionData;
    QTimer m_timerCleanItemFiles;

    struct ClientData {
        ClientData() = default;
        ClientData(const ClientSocketPtr &client, ScriptableProxy *proxy)
            : client(client)
            , proxy(proxy)
        {
        }

        bool isValid() const
        {
            return client && proxy;
        }

        ClientSocketPtr client;
        ScriptableProxy *proxy = nullptr;
    };
    QMap<ClientSocketId, ClientData> m_clients;
};

#endif // CLIPBOARDSERVER_H
