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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "gui/clipboardbrowser.h"
#include "gui/configtabshortcuts.h"

#include "platform/platformnativeinterface.h"

#include <QClipboard>
#include <QMainWindow>
#include <QMap>
#include <QModelIndex>
#include <QPointer>
#include <QSystemTrayIcon>

class ActionHandler;
class CommandDialog;
class NotificationDaemon;
class QAction;
class TrayMenu;
struct Command;
struct MainWindowOptions;

namespace Ui
{
    class MainWindow;
}

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
    explicit MainWindow(QWidget *parent = NULL);
    ~MainWindow();

    /** Return true if in browse mode (i.e. search field is hidden). */
    bool browseMode() const;

    bool hasRunningAction() const;

    QWidget *trayMenu();

    QByteArray getActionData(const QByteArray &actionId, const QString &format);

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
     * Load items if not loaded yet.
     * @return found tab or NULL
     */
    ClipboardBrowser *findTab(const QString &name);

    /**
     * Find tab with given @a name.
     * @return found tab index or -1
     */
    int findTabIndex(const QString &name);

    /**
     * Create tab with given @a name if it doesn't exist.
     * @return Existing or new tab with given @a name.
     */
    ClipboardBrowser *createTab(
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
    /** Show window and given tab and give focus to the tab. */
    void showBrowser(int index);
    /** Enter browse mode and reset search. */
    void resetStatus();

    /** Open action dialog for given @a row (or current) in current tab. */
    void openActionDialog(int row = -1);

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

    /** Set next or first tab as current. */
    void nextTab();
    /** Set previous or last tab as current. */
    void previousTab();

    /** Called after clipboard content changes. */
    void clipboardChanged(const QVariantMap &data);

    /** Temporarily disable monitoring (i.e. adding new clipboard content to the first tab). */
    void disableClipboardStoring(bool disable);

    /** Return true only if monitoring is enabled. */
    bool isMonitoringEnabled() const;

    /** Return true if clipboard storing was disabled. */
    bool isClipboardStoringDisabled() const { return m_clipboardStoringDisabled; }

    /** Return clipboard data. If MIME type is "?" return list of available MIME types. */
    QByteArray getClipboardData(const QString &mime,
                                QClipboard::Mode mode = QClipboard::Clipboard);

    /** Ignore current clipboard content. */
    void ignoreCurrentClipboard();

    bool isClipboardIgnored() const { return m_ignoreCurrentClipboard; }

    QStringList tabs() const;

    QVariant config(const QString &name, const QString &value);

    QString selectedTab() const;

    QList<int> selectedItems() const;

public slots:
    /** Close main window and exit the application. */
    void exit();

    /** Load settings. */
    void loadSettings();

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

    /** Show window and given tab and give focus to the tab. */
    void showBrowser(const ClipboardBrowser *browser);

    /** Show error popup message. */
    void showError(const QString &msg);

    /** Execute command on given input data. */
    void action(const QVariantMap &data, const Command &cmd,
                const QModelIndex &outputIndex = QModelIndex());

    /** Add @a data to tab with given name (create if tab doesn't exist). */
    void addToTab(
            const QVariantMap &data,
            //!< Item data (it may be updated if item with same text exists).
            const QString &tabName = QString(),
            //!< Tab name of target tab (first tab if empty).
            bool moveExistingToTop = false
            //!< If item already exists, move it to top and select it.
            );

    /** Set clipboard. */
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

signals:
    /** Request clipboard change. */
    void changeClipboard(const QVariantMap &data);

    void tabGroupSelected(bool selected);

    void requestExit();

    void commandsSaved();

protected:
    void keyPressEvent(QKeyEvent *event);
    void keyReleaseEvent(QKeyEvent *event);
    bool event(QEvent *event);

    /** Hide (minimize to tray) window on close. */
    void closeEvent(QCloseEvent *event);

    void showEvent(QShowEvent *event);

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
    void trayActivated(QSystemTrayIcon::ActivationReason reason);
    void onTrayActionTriggered(uint clipboardItemHash);
    void enterSearchMode(const QString &txt);
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

    void onItemMenuUpdated();

    /** Closes the main window, and returns focus to previous window. */
    void closeAndReturnFocus();

    /** Update tray and window icon depending on current state. */
    void updateIcon();

    void onAboutToQuit();

    void onCommandDialogSaved();

    void onSaveCommand(const Command &command);

    void on_tabWidget_dropItems(const QString &tabName, QDropEvent *event);

private:
    /** Create menu bar and tray menu with items. Called once. */
    void createMenu();

    /** Create context menu for @a tab. It will be automatically deleted after closed. */
    void popupTabBarMenu(const QPoint &pos, const QString &tab);

    void updateNotifications();

    void updateWindowTransparency(bool mouseOver = false);

    /** Update name and icon of "disable/enable monitoring" menu actions. */
    void updateMonitoringActions();

    /** Return browser index with given ID. */
    int findBrowserIndex(const QString &id);

    /** Return browser widget in given tab @a index. */
    ClipboardBrowser *getBrowser(int index) const;

    /** Return browser widget in current tab. */
    ClipboardBrowser *getBrowser() const;

    /** Return true only if window ID is valid. */
    bool isValidWindow(WId wid) const;

    /** Call updateFocusWindows() after a small delay. */
    void delayedUpdateFocusWindows();

    /** Show/hide tab bar. **/
    void setHideTabs(bool hide);

    /**
     * Return true if window should be minimized instead of closed/hidden.
     *
     * If tray icon is not available, window should be minimized so that it can be opened with
     * mouse. Some window managers (dwm, wmfs etc.) doesn't support minimizing, in that case
     * window can be hidden instead.
     */
    bool closeMinimizes() const;

    /**
     * Trigger all matching automatic commands on @a data.
     * If Command::remove of an applied command is true the method immediately returns false.
     */
    bool triggerActionForData(const QVariantMap &data, const QString &sourceTab);

    /** Return notification daemon (create if doesn't exist). */
    NotificationDaemon *notificationDaemon();

    ClipboardBrowser *createTab(const QString &name, bool *needSave);

    QAction *createAction(Actions::Id id, const char *slot, QMenu *menu);

    QAction *addTrayAction(Actions::Id id);

    void updateTabIcon(const QString &newName, const QString &oldName);

    Ui::MainWindow *ui;

    QMenu *m_menuItem;
    TrayMenu *m_trayMenu;

    QSystemTrayIcon *m_tray;

    QScopedPointer<MainWindowOptions> m_options;

    bool m_clipboardStoringDisabled;
    QPointer<QAction> m_actionToggleClipboardStoring;

    ClipboardBrowserSharedPtr m_sharedData;

    PlatformWindowPtr m_lastWindow;

    QTimer *m_timerUpdateFocusWindows;
    QTimer *m_timerShowWindow;
    QTimer *m_trayTimer;
    QTimer *m_trayIconSnipTimer;

    NotificationDaemon *m_notifications;

    QTimer *m_timerMiminizing;
    bool m_minimizeUnsupported;

    ActionHandler *m_actionHandler;

    QVariantMap m_clipboardData;
    bool m_ignoreCurrentClipboard;

    ClipboardBrowser *m_trayTab;

    QPointer<CommandDialog> m_commandDialog;
};

#endif // MAINWINDOW_H
