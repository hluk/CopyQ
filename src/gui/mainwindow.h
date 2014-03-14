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
#include <QSharedPointer>
#include <QSystemTrayIcon>

class Action;
class ActionHandler;
class ClipboardItem;
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

public slots:
    /**
     * Return browser widget in given tab @a index (or current tab).
     * Load items if not loaded yet.
     */
    ClipboardBrowser *browser(int index = -1);

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

    /** Return tab index for browser widget (-1 if not found). */
    int tabIndex(const ClipboardBrowser *c) const;

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
    bool toggleMenu();

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
    void showMessage(const QString &title, const QString &msg, const QPixmap &icon, int msec,
                     int notificationId);

    /** Show clipboard content in notification. */
    void showClipboardMessage(const QVariantMap &data);

    /** Show error popup message. */
    void showError(const QString &msg);

    /** Show and focus main window. */
    void showWindow();
    /** Hide window to tray or minimize if tray is not available. */
    void hideWindow();
    /** Show/hide main window. Return true only if window is shown. */
    bool toggleVisible();
    /** Show window and given tab and give focus to the tab. */
    void showBrowser(const ClipboardBrowser *browser);
    /** Show window and given tab and give focus to the tab. */
    void showBrowser(int index);
    /** Enter browse mode and reset search. */
    void resetStatus();

    /** Close main window and exit the application. */
    void exit();
    /** Load settings. */
    void loadSettings();

    /** Open about dialog. */
    void openAboutDialog();

    /** Open dialog with clipboard content. */
    void showClipboardContent();

    /** Open action dialog for given @a row (or current) in current tab. */
    void openActionDialog(int row = -1);
    /** Open action dialog with given input @a text. */
    WId openActionDialog(const QVariantMap &data);

    /** Open preferences dialog. */
    void openPreferences();

    /** Execute command on given input data. */
    void action(const QVariantMap &data, const Command &cmd,
                const QModelIndex &outputIndex = QModelIndex());

    /** Open tab creation dialog. */
    void newTab(const QString &name = QString());
    /** Open tab group renaming dialog. */
    void renameTabGroup(const QString &name);
    /** Rename current tab to given name (if possible). */
    void renameTabGroup(const QString &newName, const QString &oldName);
    /** Open tab renaming dialog (for given tab index or current tab). */
    void renameTab(int tab = -1);
    /** Rename current tab to given name (if possible). */
    void renameTab(const QString &name, int tabIndex);
    /** Remove all tab in group. */
    void removeTabGroup(const QString &name);
    /** Remove tab. */
    void removeTab();
    /** Remove tab. */
    void removeTab(
            bool ask, //!< Ask before removing.
            int tab_index //!< Tab index or current tab.
            );

    /**
     * Add tab with given name if doesn't exist and focus the tab.
     * @return New or existing tab with given name.
     */
    ClipboardBrowser *addTab(const QString &name);

    /** Create new item in current tab. */
    void editNewItem();
    /** Paste items to current tab. */
    void pasteItems();
    /** Copy selected items in current tab. */
    void copyItems();

    /**
     * Save all items in tab to file.
     * @return True only if all items were successfully saved.
     */
    bool saveTab(
            const QString &fileName,
            int tab_index = -1 //!< Tab index or current tab.
            );
    /** Save all items in tab. Show file dialog. */
    bool saveTab(
            int tab_index = -1 //!< Tab index or current tab.
            );

    /** Save all unsaved tabs. */
    void saveTabs();

    /**
     * Load saved items to new tab.
     * @return True only if all items were successfully loaded.
     */
    bool loadTab(const QString &fileName);
    /**
     * Load saved items to new tab.
     * Show file dialog and focus the new tab.
     */
    bool loadTab();

    /** Sort selected items. */
    void sortSelectedItems();
    /** Reverse order of selected items. */
    void reverseSelectedItems();

    /** Add @a data to tab with given name (create if tab doesn't exist). */
    void addToTab(
            const QVariantMap &data,
            //!< Item data (it may be updated if item with same text exists).
            const QString &tabName = QString(),
            //!< Tab name of target tab (first tab if empty).
            bool moveExistingToTop = false
            //!< If item already exists, move it to top and select it.
            );

    /** Set next or first tab as current. */
    void nextTab();
    /** Set previous or last tab as current. */
    void previousTab();

    /** Called after clipboard content changes. */
    void clipboardChanged(const QVariantMap &data);

    /** Set clipboard. */
    void setClipboard(const QVariantMap &data);

    /** Activate current item. */
    void activateCurrentItem();

    /** Temporarily disable monitoring (i.e. adding new clipboard content to the first tab). */
    void disableClipboardStoring(bool disable);

    /** Toggle monitoring (i.e. adding new clipboard content to the first tab). */
    void toggleClipboardStoring();

    /** Return true if clipboard storing was disabled. */
    bool isClipboardStoringDisabled() const { return m_clipboardStoringDisabled; }

    /** Return clipboard data. If MIME type is "?" return list of available MIME types. */
    QByteArray getClipboardData(const QString &mime,
                                QClipboard::Mode mode = QClipboard::Clipboard);

    /** Paste clipboard content to current window. */
    void pasteToCurrentWindow();

    QStringList tabs() const;

    QVariant config(const QString &name, const QString &value);

    QString selectedTab() const;

    QString selectedItems() const;

signals:
    /** Request clipboard change. */
    void changeClipboard(const QVariantMap &data);

    void tabGroupSelected(bool selected);

    void requestExit();

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
    void tabMoved(int from, int to);
    void tabMoved(const QString &oldPrefix, const QString &newPrefix, const QString &afterPrefix);
    void tabMenuRequested(const QPoint &pos, int tab);
    void tabMenuRequested(const QPoint &pos, const QString &groupPath);
    void tabCloseRequested(int tab);
    void onFilterChanged(const QRegExp &re);
    void onTrayTimer();
    void saveGeometry();
    void loadGeometry();

    /** Update WId for paste and last focused window if needed. */
    void updateFocusWindows();

    void onItemMenuUpdated();

    /** Closes the main window, and returns focus to previous window. */
    void closeAndReturnFocus();

    /** Update tray and window icon depending on current state. */
    void updateIcon();

    void onBrowserError(const QString &errorString);

    void onAboutToQuit();

private:
    /** Create menu bar and tray menu with items. Called once. */
    void createMenu();

    /** Create context menu for @a tab. It will be automatically deleted after closed. */
    void popupTabBarMenu(const QPoint &pos, const QString &tab);

    void updateNotifications();

    void updateWindowTransparency(bool mouseOver = false);

    /** Update name and icon of "disable/enable monitoring" menu actions. */
    void updateMonitoringActions();

    /** Return browser with given ID. */
    ClipboardBrowser *findBrowser(const QString &id);

    /** Return browser index with given ID. */
    int findBrowserIndex(const QString &id);

    /** Return browser widget in given tab @a index (or current tab). */
    ClipboardBrowser *getBrowser(int index = -1) const;

    /** Return true only if window ID is valid. */
    bool isValidWindow(WId wid) const;

    /** Call updateFocusWindows() after a small delay. */
    void delayedUpdateFocusWindows();

    /** Show/hide tab bar. **/
    void setHideTabs(bool hide);

    void saveCollapsedTabs();

    void loadCollapsedTabs();

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

    bool canSaveGeometry() const;

    /** Return notification daemon (create if doesn't exist). */
    NotificationDaemon *notificationDaemon();

    ClipboardBrowser *createTab(const QString &name, bool *needSave);

    QAction *createAction(Actions::Id id, const char *slot, QMenu *menu);

    QAction *addTrayAction(Actions::Id id);

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
    QTimer *m_timerSaveGeometry;
    QTimer *m_trayTimer;

    NotificationDaemon *m_notifications;

    QTimer *m_timerMiminizing;
    bool m_minimizeUnsupported;

    ActionHandler *m_actionHandler;
};

typedef QSharedPointer<MainWindow> MainWindowPtr;

#endif // MAINWINDOW_H
