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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "common/commandtester.h"
#include "gui/clipboardbrowsershared.h"
#include "gui/menuitems.h"
#include "gui/notificationbutton.h"

#include "platform/platformnativeinterface.h"

#include <QAction>
#include <QClipboard>
#include <QMainWindow>
#include <QPointer>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QModelIndex>

class Action;
class ActionHandler;
class ClipboardBrowser;
class ClipboardBrowserPlaceholder;
class CommandAction;
class CommandDialog;
class ConfigurationManager;
class ItemFactory;
class NotificationDaemon;
class Theme;
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

enum class ImportOptions {
    /// Select what to import/export in dialog.
    Select,
    /// Import/export everything without asking.
    All
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
        , trayMenuOpenOnLeftClick(false)
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
    bool trayMenuOpenOnLeftClick;
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

class Callable {
public:
    Callable() = default;
    Callable(Callable &&) = default;
    virtual ~Callable() = default;
    virtual void operator()() = 0;
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
    explicit MainWindow(ItemFactory *itemFactory, QWidget *parent = nullptr);
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

    /** Return browser containing item or nullptr. */
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
    bool toggleMenu();
    bool toggleMenu(const QString &tabName, int itemCount, const QPoint &position);

    /** Switch between browse and search mode. */
    void enterBrowseMode();

    void enterSearchMode();

    void enterSearchMode(const QString &txt);

    /** Show notification. */
    void showMessage(const QString &title, //!< Message title.
            const QString &msg, //!< Message text.
            ushort icon = QSystemTrayIcon::Information,
            //!< Type of popup.
            int msec = 8000, //!< Show interval.
            const QString &notificationId = QString(), //!< ID of notification.
            const NotificationButtons &buttons = NotificationButtons());

    /** Show popup with icon. */
    void showMessage(const QString &title,
            const QString &msg,
            const QString &icon,
            int msec,
            const QString &notificationId = QString(),
            const NotificationButtons &buttons = NotificationButtons());

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
            int tabIndex //!< Tab index or current tab.
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
            int tabIndex = -1 //!< Tab index or current tab.
            );

    /** Save all unsaved tabs. */
    void saveTabs();

    /**
     * Load saved items to new tab.
     * @return True only if all items were successfully loaded.
     */
    bool loadTab(const QString &fileName);

    /**
     * Import tabs, settings etc.
     * @return True only if all data were successfully loaded.
     */
    bool importData(const QString &fileName, ImportOptions options);

    /**
     * Export tabs, settings etc.
     * @return True only if all data were successfully saved.
     */
    bool exportAllData(const QString &fileName);

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
    void updateFirstItem(QVariantMap data);

    /// Used by config() command.
    QStringList config(const QStringList &nameValue);

    QVariantMap actionData(int id) const;
    void setActionData(int id, const QVariantMap &data);

    void setCommands(const QList<Command> &commands);

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

    /** Open action dialog with given input data. */
    void openActionDialog(const QVariantMap &data);

    /** Open action dialog with input data from selected items. */
    void openActionDialog();

    /** Open preferences dialog. */
    void openPreferences();

    /** Open commands dialog. */
    void openCommands();

    /** Sort selected items. */
    void sortSelectedItems();
    /** Reverse order of selected items. */
    void reverseSelectedItems();

    /**
     * Import tabs, settings etc. (select file in dialog).
     * @return True only if all data were successfully loaded.
     */
    bool importData();

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

    /** Open command dialog and add commands. */
    void addCommands(const QList<Command> &commands);

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
    void runAutomaticCommands(QVariantMap data);

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
     */
    void addTab(const QString &name);

    /** Toggle monitoring (i.e. adding new clipboard content to the first tab). */
    void toggleClipboardStoring();

    /**
     * Export tabs, settings etc. (select in file dialog).
     * @return True only if all data were successfully saved.
     */
    bool exportData();

    /** Set next or first tab as current. */
    void nextTab();
    /** Set previous or last tab as current. */
    void previousTab();

    /** Set window title and tray tool tip from data. */
    void updateTitle(const QVariantMap &data);

    bool canUpdateTitleFromScript() const;

    /** Set text for filtering items. */
    void setFilter(const QString &text);

    void hideSearchBar();

    void invoke(Callable *callable);

#ifdef HAS_TESTS
    /**
     * Send key clicks to currently focused widget.
     *
     * This function will block if it opens a modal dialog.
     * So use sendKeyClicks() instead.
     *
     * Increments key clicks sequence number returned by lastReceivedKeyClicks().
     */
    void keyClicks(const QString &keys, int delay);

    /**
     * Send key clicks to focused widget.
     * @return Key clicks sequence number.
     */
    uint sendKeyClicks(const QString &keys, int delay);

    /**
     * @return Last key clicks sequence number received by widgets.
     */
    uint lastReceivedKeyClicks();

    /**
     * Remove all tab and reset configuration (work only in test session).
     */
    void resetTestSession(const QString &clipboardTabName);
#endif

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
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    bool event(QEvent *event) override;

    /** Hide (minimize to tray) window on close. */
    void closeEvent(QCloseEvent *event) override;

    void showEvent(QShowEvent *event) override;

    bool focusNextPrevChild(bool next) override;

#if QT_VERSION < 0x050000
#   ifdef COPYQ_WS_X11
    bool x11Event(XEvent *event) override;
#   elif defined(Q_OS_WIN)
    bool winEvent(MSG *message, long *result) override;
#   elif defined(Q_OS_MAC)
    bool macEvent(EventHandlerCallRef caller, EventRef event) override;
#   endif
#else
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
#endif

private slots:
    ClipboardBrowser *getTabForMenu();
    ClipboardBrowser *getTabForTrayMenu();
    void updateTrayMenuItems();
    void addMenuItems(const QString &searchText);
    void addTrayMenuItems(const QString &searchText);
    void trayActivated(QSystemTrayIcon::ActivationReason reason);
    void onMenuActionTriggered(uint itemHash, bool omitPaste);
    void onTrayActionTriggered(uint itemHash, bool omitPaste);
    void findNextOrPrevious();
    void tabChanged(int current, int previous);
    void saveTabPositions();
    void doSaveTabPositions();
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

    void updateItemPreview();
    void setItemPreviewVisible(bool visible);

    /** Update icon snip animation. */
    void updateIconSnip();

    void onAboutToQuit();

    void onCommandDialogSaved();

    void onSaveCommand(const Command &command);

    void onItemCommandActionTriggered(CommandAction *commandAction, const QString &triggeredShortcut);
    void onClipboardCommandActionTriggered(CommandAction *commandAction, const QString &triggeredShortcut);

    void on_tabWidget_dropItems(const QString &tabName, const QMimeData *data);

    void showContextMenu(const QPoint &position);

    void updateContextMenu(const ClipboardBrowser *browser);

    void automaticCommandTestFinished(const Command &command, bool passed);

    QAction *enableActionForCommand(QMenu *menu, const Command &command, bool enable);
    void addCommandsToItemMenu(const Command &command, bool passed);
    void addCommandsToTrayMenu(const Command &command, bool passed);

    void nextItemFormat();
    void previousItemFormat();

    void moveUp();
    void moveDown();
    void moveToTop();
    void moveToBottom();

    void onBrowserCreated(ClipboardBrowser *browser);

    void onNotificationButtonClicked(const NotificationButton &button);

private:
    enum TabNameMatching {
        MatchExactTabName,
        MatchSimilarTabName
    };

    ClipboardBrowserPlaceholder *createTab(const QString &name, TabNameMatching nameMatch);

    int findTabIndexExactMatch(const QString &name);

    void clearTitle() { updateTitle(QVariantMap()); }

    /** Create menu bar and tray menu with items. Called once. */
    void createMenu();

    /** Create context menu for @a tab. It will be automatically deleted after closed. */
    void popupTabBarMenu(const QPoint &pos, const QString &tab);

    void updateContextMenu();

    void updateNotifications();

    void updateWindowTransparency(bool mouseOver = false);

    /** Update name and icon of "disable/enable monitoring" menu actions. */
    void updateMonitoringActions();

    /** Return browser widget in given tab @a index. */
    ClipboardBrowserPlaceholder *getPlaceholder(int index) const;

    /** Return browser widget in current tab. */
    ClipboardBrowserPlaceholder *getPlaceholder() const;

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

    QList<Command> commandsForMenu(const QVariantMap &data, const QString &tabName);
    void addCommandsToItemMenu(ClipboardBrowser *c);
    void addCommandsToTrayMenu(const QVariantMap &clipboardData);

    bool isItemMenuDefaultActionValid() const;

    void updateToolBar();

    void updateWindowTitle();

    void updateTrayTooltip();

    void updateTrayIcon();

    void initTray();

    void runAutomaticCommand(const Command &command);

    bool isWindowVisible() const;

    void onEscape();

    /** Disable shortcuts for all default actions. */
    void setDisabledShortcuts(const QList<QKeySequence> &shortcuts);

    void updateActionShortcuts(int id);
    void updateActionShortcuts();

    void pasteClipboard(const PlatformWindowPtr &window);

    QAction *actionForMenuItem(int id, QWidget *parent, Qt::ShortcutContext context);

    void addMenuItems(TrayMenu *menu, ClipboardBrowser *c, int maxItemCount, const QString &searchText);
    void onMenuActionTriggered(ClipboardBrowser *c, uint itemHash, bool omitPaste);
    QWidget *toggleMenu(TrayMenu *menu, const QPoint &pos);
    QWidget *toggleMenu(TrayMenu *menu);

    bool exportData(const QString &fileName, const QStringList &tabs, bool exportConfiguration, bool exportCommands);
    bool exportDataV3(QDataStream *out, const QStringList &tabs, bool exportConfiguration, bool exportCommands);
    bool importDataV3(QDataStream *in, ImportOptions options);

    const Theme &theme() const;

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

    PlatformWindowPtr m_lastWindow;

    QTimer m_timerUpdateFocusWindows;
    QTimer m_timerUpdateContextMenu;
    QTimer m_timerShowWindow;
    QTimer m_timerTrayAvailable;
    QTimer m_timerTrayIconSnip;
    QTimer m_timerSaveTabPositions;

    NotificationDaemon *m_notifications;

    ActionHandler *m_actionHandler;

    QVariantMap m_clipboardData;

    TrayMenu *m_menu;
    QString m_menuTabName;
    int m_menuMaxItemCount;

    QPointer<CommandDialog> m_commandDialog;

    CommandTester m_itemMenuCommandTester;
    CommandTester m_trayMenuCommandTester;
    CommandTester m_automaticCommandTester;

    QPointer<Action> m_currentAutomaticCommand;
    bool m_canUpdateTitleFromScript;

    bool m_iconSnip;

    bool m_wasMaximized;

    bool m_showItemPreview;

    QList<QKeySequence> m_disabledShortcuts;

    QVector< QPointer<QAction> > m_actions;
    MenuItems m_menuItems;

#ifdef HAS_TESTS
    /// Key clicks sequence number last returned by sendKeyClicks().
    uint m_sentKeyClicks = 0;
    /// Key clicks sequence number received by widgets.
    uint m_receivedKeyClicks = 0;
#endif
};

#endif // MAINWINDOW_H
