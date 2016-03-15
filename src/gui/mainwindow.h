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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "common/commandtester.h"
#include "gui/clipboardbrowser.h"
#include "gui/menuitems.h"

#include "platform/platformnativeinterface.h"

#include <QAction>
#include <QClipboard>
#include <QMainWindow>
#include <QPointer>
#include <QSystemTrayIcon>

class Action;
class ActionHandler;
class CommandDialog;
class ConfigurationManager;
class NotificationDaemon;
class QModelIndex;
class TrayMenu;
struct Command;
struct MainWindowOptions;

Q_DECLARE_METATYPE(QPersistentModelIndex)
Q_DECLARE_METATYPE(QList<QPersistentModelIndex>)

namespace Ui
{
    class MainWindow;
}

enum ItemActivationCommand {
    ActivateNoCommand = 0x0,
    ActivateCloses = 0x1,
    ActivateFocuses = 0x2,
    ActivatePastes = 0x4
};

struct MainWindowOptions {
    MainWindowOptions()
        : confirmExit(true)
        , viMode(false)
        , trayCommands(false)
        , trayCurrentTab(false)
        , trayTabName()
        , trayItems(5)
        , trayImages(true)
        , itemPopupInterval(0)
        , clipboardNotificationLines(0)
        , transparency(0)
        , transparencyFocused(0)
        , hideTabs(false)
        , hideMainWindow(false)
        , itemActivationCommands(ActivateCloses)
        , clearFirstTab(false)
        , trayItemPaste(true)
        , clipboardTab()
    {}

    bool activateCloses() const { return itemActivationCommands & ActivateCloses; }
    bool activateFocuses() const { return itemActivationCommands & ActivateFocuses; }
    bool activatePastes() const { return itemActivationCommands & ActivatePastes; }

    bool confirmExit;
    bool viMode;
    bool trayCommands;
    bool trayCurrentTab;
    QString trayTabName;
    int trayItems;
    bool trayImages;
    int itemPopupInterval;
    int clipboardNotificationLines;
    int transparency;
    int transparencyFocused;

    bool hideTabs;

    bool hideMainWindow;

    int itemActivationCommands;

    bool clearFirstTab;

    bool trayItemPaste;

    QString clipboardTab;
};

/**
 * Application's main window.
 *
 * Contains search bar and tab widget.
 * Each tab contains one clipboard browser widget.
 *
 * It operates in two modes:
 *  * browse mode with search bar hidden and empty (default) and
 *  * search mode with search bar shown and not empty.
 *
 * If user starts typing text the search mode will become active and
 * the search bar focused.
 * If the text is deleted or escape pressed the browse mode will become active.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(ItemFactory *itemFactory, QWidget *parent = NULL);
    ~MainWindow();

    /** Return true if in browse mode (i.e. search field is hidden). */
    bool browseMode() const;

    bool hasRunningAction() const;

    /**
     * Try to close command dialog and return true on success.
     *
     * Note that dialog won't be closed if it has unsaved changes
     * and user canceles the closing.
     */
    bool maybeCloseCommandDialog();

    QWidget *trayMenu();

    /**
     * Return browser widget in given tab @a index.
     * Load items if not loaded yet.
     */
    ClipboardBrowser *browser(int index);

    /**
     * Return browser widget in current tab.
     * Load items if not loaded yet.
     */
    ClipboardBrowser *browser();

    /** Return browser containing item or NULL. */
    ClipboardBrowser *browserForItem(const QModelIndex &index);

    /**
     * Find tab with given @a name.
     * @return found tab index or -1
     */
    int findTabIndex(const QString &name);

    /**
     * Tries to find tab with exact or similar name (ignores
     * key hints '&') or creates new one.
     * @return Existing or new tab with given @a name.
     */
    ClipboardBrowser *tab(
            const QString &name //!< Name of the new tab.
            );

    /**
     * Show/hide tray menu. Return true only if menu is shown.
     */
    bool toggleMenu(ClipboardBrowser *browser = NULL);

    /** Switch between browse and search mode. */
    void enterBrowseMode(bool browsemode = true);

    /** Show notification. */
    void showMessage(
            const QString &title, //!< Message title.
            const QString &msg, //!< Message text.
            QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information,
            //!< Type of popup.
            int msec = 8000, //!< Show interval.
            int notificationId = -1 //!< ID of notification.
            );

    /** Show popup with icon. */
    void showMessage(const QString &title, const QString &msg, ushort icon, int msec,
                     int notificationId);

    /** Show clipboard content in notification. */
    void showClipboardMessage(const QVariantMap &data);

    /** Show and focus main window. */
    void showWindow();
    /** Hide window to tray or minimize if tray is not available. */
    void hideWindow();
    /** Minimize window (hide if option is set). */
    void minimizeWindow();
    /** Set current tab. */
    bool setCurrentTab(int index);
    /** Show window and given tab and give focus to the tab. */
    void showBrowser(int index);
    /** Enter browse mode and reset search. */
    void resetStatus();

    /** Open tab group renaming dialog. */
    void renameTabGroup(const QString &name);
    /** Remove all tab in group. */
    void removeTabGroup(const QString &name);
    /** Remove tab. */
    void removeTab(
            bool ask, //!< Ask before removing.
            int tab_index //!< Tab index or current tab.
            );
    /** Set icon for tab or tab group. */
    void setTabIcon(const QString &tabName);

    void setTabIcon(const QString &tabName, const QString &icon);

    /**
     * Save all items in tab to file.
     * @return True only if all items were successfully saved.
     */
    bool saveTab(
            const QString &fileName,
            int tab_index = -1 //!< Tab index or current tab.
            );

    /** Save all unsaved tabs. */
    void saveTabs();

    /**
     * Load saved items to new tab.
     * @return True only if all items were successfully loaded.
     */
    bool loadTab(const QString &fileName);

    /** Called after clipboard content changes. */
    void clipboardChanged(const QVariantMap &data);

    /** Temporarily disable monitoring (i.e. adding new clipboard content to the first tab). */
    void disableClipboardStoring(bool disable);

    /** Return true only if monitoring is enabled. */
    bool isMonitoringEnabled() const;

    /** Abort execution of automatic commands. */
    void abortAutomaticCommands();

    QStringList tabs() const;

    /** Update the first item in the first tab. */
    void updateFirstItem(const QVariantMap &data);

    /// Get description for all user options (used by config() command).
    QString getUserOptionsDescription() const;

    /// Get description for an user option (used by config() command).
    QString getUserOptionValue(const QString &name) const;

    /// Set value of user option (used by config() command).
    void setUserOptionValue(const QString &name, const QString &value);

    /// Return true only if user option is available (used by config() command).
    bool hasUserOption(const QString &name) const;

public slots:
    /** Close main window and exit the application. */
    void exit();

    /** Load settings. */
    void loadSettings();

    /** Open log dialog. */
    void openLogDialog();

    /** Open about dialog. */
    void openAboutDialog();

    /** Open dialog with clipboard content. */
    void showClipboardContent();

    /** Open dialog with active commands. */
    void showProcessManagerDialog();

    /** Open action dialog with given input @a text. */
    WId openActionDialog(const QVariantMap &data);

    /** Open preferences dialog. */
    void openPreferences();

    /** Open commands dialog. */
    void openCommands();

    /** Sort selected items. */
    void sortSelectedItems();
    /** Reverse order of selected items. */
    void reverseSelectedItems();

    /**
     * Load saved items to new tab.
     * Show file dialog and focus the new tab.
     */
    bool loadTab();

    /** Create new item in current tab. */
    void editNewItem();
    /** Paste items to current tab. */
    void pasteItems();
    /** Copy selected items in current tab. */
    void copyItems();

    /** Activate current item. */
    void activateCurrentItem();

    /** Set current tab. */
    void setCurrentTab(const ClipboardBrowser *browser);

    /** Show window and given tab and give focus to the tab. */
    void showBrowser(const ClipboardBrowser *browser);

    /** Show error popup message. */
    void showError(const QString &msg);

    /** Execute command on given input data. */
    Action *action(
            const QVariantMap &data,
            const Command &cmd,
            const QModelIndex &outputIndex = QModelIndex());

    /** Add @a data to tab with given name (create if tab doesn't exist). */
    void addToTab(
            /// Item data (it may be updated if item with same text exists).
            const QVariantMap &data,
            /// Tab name of target tab.
            const QString &tabName
            );

    /**
     * Run automatic commands and add @a new clipboard to the first tab
     * if commands didn't remove or transform the data.
     */
    void runAutomaticCommands(const QVariantMap &data);

    /** Set clipboard. */
    void setClipboard(const QVariantMap &data, QClipboard::Mode mode);

    /** Set clipboard and synchronize selection if needed. */
    void setClipboard(const QVariantMap &data);

    /** Show/hide main window. Return true only if window is shown. */
    bool toggleVisible();

    /** Set icon for current tab or tab group. */
    void setTabIcon();

    /** Open tab creation dialog. */
    void newTab(const QString &name = QString());

    /** Remove tab. */
    void removeTab();

    /** Rename current tab to given name (if possible). */
    void renameTabGroup(const QString &newName, const QString &oldName);
    /** Open tab renaming dialog (for given tab index or current tab). */
    void renameTab(int tab = -1);
    /** Rename current tab to given name (if possible). */
    void renameTab(const QString &name, int tabIndex);

    /**
     * Add tab with given name if doesn't exist and focus the tab.
     * @return New or existing tab with given name.
     */
    ClipboardBrowser *addTab(const QString &name);

    /** Toggle monitoring (i.e. adding new clipboard content to the first tab). */
    void toggleClipboardStoring();

    /** Save all items in tab. Show file dialog. */
    bool saveTab(
            int tab_index = -1 //!< Tab index or current tab.
            );

    /** Set next or first tab as current. */
    void nextTab();
    /** Set previous or last tab as current. */
    void previousTab();

    /** Set window title and tray tool tip from data. */
    void updateTitle(const QVariantMap &data);

    bool canUpdateTitleFromScript() const;

    /** Set text for filtering items. */
    void setFilter(const QString &text);

signals:
    /** Request clipboard change. */
    void changeClipboard(const QVariantMap &data, QClipboard::Mode mode);

    void tabGroupSelected(bool selected);

    void requestExit();

    void commandsSaved();

    void stopItemMenuCommandTester();
    void stopTrayMenuCommandTester();

    void configurationChanged();

protected:
    void keyPressEvent(QKeyEvent *event);
    void keyReleaseEvent(QKeyEvent *event);
    bool event(QEvent *event);

    /** Hide (minimize to tray) window on close. */
    void closeEvent(QCloseEvent *event);

    void showEvent(QShowEvent *event);

    void hideEvent(QHideEvent *event);

#if QT_VERSION < 0x050000
#   ifdef COPYQ_WS_X11
    bool x11Event(XEvent *event);
#   elif defined(Q_OS_WIN)
    bool winEvent(MSG *message, long *result);
#   elif defined(Q_OS_MAC)
    bool macEvent(EventHandlerCallRef caller, EventRef event);
#   endif
#else
    bool nativeEvent(const QByteArray &eventType, void *message, long *result);
#endif

private slots:
    ClipboardBrowser *getTabForTrayMenu();
    void updateTrayMenuItems();
    void clearTrayMenu();
    void trayActivated(QSystemTrayIcon::ActivationReason reason);
    void onTrayActionTriggered(uint clipboardItemHash, bool omitPaste);
    void enterSearchMode(const QString &txt);
    void findNext(int where = 1);
    void findPrevious();
    void tabChanged(int current, int previous);
    void saveTabPositions();
    void tabsMoved(const QString &oldPrefix, const QString &newPrefix);
    void tabMenuRequested(const QPoint &pos, int tab);
    void tabMenuRequested(const QPoint &pos, const QString &groupPath);
    void tabCloseRequested(int tab);
    void onFilterChanged(const QRegExp &re);
    void createTrayIfSupported();

    /** Update WId for paste and last focused window if needed. */
    void updateFocusWindows();

    /** Update tray and window icon depending on current state. */
    void updateIcon();
    void updateIconSnipTimeout();

    void updateContextMenuTimeout();

    /** Update icon snip animation. */
    void updateIconSnip();

    void onAboutToQuit();

    void onCommandDialogSaved();

    void onSaveCommand(const Command &command);

    void onCommandActionTriggered(const Command &command, const QVariantMap &data, int commandType);

    void on_tabWidget_dropItems(const QString &tabName, QDropEvent *event);

    void showContextMenu(const QPoint &position);

    void updateContextMenu();

    void action();

    void automaticCommandTestFinished(const Command &command, bool passed);

    void enableActionForCommand(QMenu *menu, const Command &command, bool enable);
    void addCommandsToItemMenu(const Command &command, bool enable);
    void addCommandsToTrayMenu(const Command &command, bool enable);

    void nextItemFormat();
    void previousItemFormat();

    void moveUp();
    void moveDown();
    void moveToTop();
    void moveToBottom();

private:
    enum TabNameMatching {
        MatchExactTabName,
        MatchSimilarTabName
    };

    ClipboardBrowser *createTab(
            const QString &name, TabNameMatching nameMatch, bool *needSave = NULL);

    int findTabIndexExactMatch(const QString &name);

    void clearTitle() { updateTitle(QVariantMap()); }

    /** Create menu bar and tray menu with items. Called once. */
    void createMenu();

    /** Create context menu for @a tab. It will be automatically deleted after closed. */
    void popupTabBarMenu(const QPoint &pos, const QString &tab);

    void updateNotifications();

    void updateWindowTransparency(bool mouseOver = false);

    /** Update name and icon of "disable/enable monitoring" menu actions. */
    void updateMonitoringActions();

    /** Return browser widget in given tab @a index. */
    ClipboardBrowser *getBrowser(int index) const;

    /** Return browser widget in current tab. */
    ClipboardBrowser *getBrowser() const;

    /** Call updateFocusWindows() after a small delay. */
    void delayedUpdateFocusWindows();

    /** Show/hide tab bar. **/
    void setHideTabs(bool hide);

    /**
     * Return true if window should be minimized instead of closed/hidden.
     *
     * If tray icon is not available, window should be minimized so that it can be opened with
     * mouse.
     */
    bool closeMinimizes() const;

    /** Return notification daemon (create if doesn't exist). */
    NotificationDaemon *notificationDaemon();

    QAction *createAction(int id, const char *slot, QMenu *menu);

    QAction *addTrayAction(int id);

    void updateTabIcon(const QString &newName, const QString &oldName);

    QAction *addItemAction(int id, QObject *receiver, const char *slot);

    void addCommandsToMenu(QMenu *menu, const QVariantMap &data);

    void updateToolBar();

    void updateWindowTitle();

    void updateTrayTooltip();

    void updateTrayIcon();

    void initTray();

    void runAutomaticCommand(const Command &command);

    bool isWindowVisible() const;

    ClipboardBrowser *clipboardTab();

    void onEscape();

    /** Disable shortcuts for all default actions. */
    void setDisabledShortcuts(const QList<QKeySequence> &shortcuts);

    void updateActionShortcuts(int id);
    void updateActionShortcuts();

    QAction *actionForMenuItem(int id, QWidget *parent, Qt::ShortcutContext context);

    ConfigurationManager *cm;
    Ui::MainWindow *ui;

    QMenu *m_menuItem;
    TrayMenu *m_trayMenu;

    QSystemTrayIcon *m_tray;

    MainWindowOptions m_options;

    bool m_clipboardStoringDisabled;
    QPointer<QAction> m_actionToggleClipboardStoring;

    ClipboardBrowserSharedPtr m_sharedData;
    QList<Command> m_commands;
    QPointer<QAction> m_activateCurrentItemAction;

    PlatformWindowPtr m_lastWindow;

    QTimer m_timerUpdateFocusWindows;
    QTimer m_timerUpdateContextMenu;
    QTimer m_timerShowWindow;
    QTimer m_timerTrayAvailable;
    QTimer m_timerTrayIconSnip;

    NotificationDaemon *m_notifications;

    ActionHandler *m_actionHandler;

    QVariantMap m_clipboardData;

    ClipboardBrowser *m_trayTab;

    QPointer<CommandDialog> m_commandDialog;

    CommandTester m_itemMenuCommandTester;
    CommandTester m_trayMenuCommandTester;
    CommandTester m_automaticCommandTester;

    bool m_stopAutomaticCommands;
    QPointer<Action> m_currentAutomaticCommand;
    bool m_canUpdateTitleFromScript;

    bool m_iconSnip;

    bool m_wasMaximized;

    QList<QKeySequence> m_disabledShortcuts;

    QVector< QPointer<QAction> > m_actions;
    MenuItems m_menuItems;
};

#endif // MAINWINDOW_H
