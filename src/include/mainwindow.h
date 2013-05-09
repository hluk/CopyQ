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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QModelIndex>
#include <QPointer>
#include <QSharedPointer>
#include <QSystemTrayIcon>

class AboutDialog;
class Action;
class ActionDialog;
class ClipboardBrowser;
class ClipboardItem;
class QAction;
class QMimeData;
class TrayMenu;
struct ClipboardBrowserShared;
struct Command;

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

        /** Return true if in browse mode. */
        bool browseMode() const { return m_browsemode; }

        /** Save settings, items in browsers and window geometry. */
        void saveSettings();

        /** Hide (minimize to tray) window on close. */
        void closeEvent(QCloseEvent *event);

        /** Create new action dialog. */
        ActionDialog *createActionDialog();

        /**
         * Return browser widget in given tab @a index (or current tab).
         * Load items if not loaded yet.
         */
        ClipboardBrowser *browser(int index = -1);

        /** Return tab index for browser widget (-1 if not found). */
        int tabIndex(const ClipboardBrowser *c) const;

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
                const QString &name, //!< Name of the new tab.
                bool save
                //!< If true saveSettings() is called if tab is created.
                );

        /** Return tab names. */
        QStringList tabs() const;

        bool isTrayMenuVisible() const;

        /** Return window ID of tray menu. */
        WId trayMenuWinId() const;

    signals:
        /** Request clipboard change. */
        void changeClipboard(const ClipboardItem *item);

        /** User begins or stops to edit an item in a tab. */
        void editingActive(bool active);

    protected:
        void keyPressEvent(QKeyEvent *event);
        void dragEnterEvent(QDragEnterEvent *event);
        void dropEvent(QDropEvent *event);
        bool event(QEvent *event);

    public slots:
        /**
         * Show/hide tray menu.
         */
        void toggleMenu();

        /** Switch between browse and search mode. */
        void enterBrowseMode(bool browsemode = true);

        /** Show tray popup message. */
        void showMessage(
                const QString &title, //!< Message title.
                const QString &msg, //!< Message text.
                QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information,
                //!< Type of popup.
                int msec = 8000 //!< Show interval.
                );

        /** Show error in tray popup message. */
        void showError(const QString &msg);

        /** Show and focus main window. */
        void showWindow();
        /** Show/hide main window. */
        void toggleVisible();
        /** Show window and given tab and give focus to the tab. */
        void showBrowser(const ClipboardBrowser *browser);
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
        WId openActionDialog(const QMimeData &data);

        /** Open preferences dialog. */
        void openPreferences();

        /** Execute action. */
        void action(Action *action);

        /** Execute command on given input data. */
        void action(const QMimeData &data, const Command &cmd,
                    const QModelIndex &outputIndex = QModelIndex());

        /** Open tab creation dialog. */
        void newTab();
        /** Open tab renaming dialog (for given tab index or current tab). */
        void renameTab(int tab = -1);
        /** Rename current tab to given name (if possible). */
        void renameTab(const QString &name, int tabIndex);
        /** Remove tab. */
        void removeTab(
                bool ask = true, //!< Ask before removing.
                int tab_index = -1 //!< Tab index or current tab.
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
                const QMimeData *data,
                //!< Item data (it may be updated if item with same text exists).
                const QString &tabName = QString()
                //!< Tab name of target tab (first tab if empty).
                );

        /** Set next or first tab as current. */
        void nextTab();
        /** Set previous or last tab as current. */
        void previousTab();

        /** Called after clipboard content changes. */
        void clipboardChanged(const ClipboardItem *item);

        /** Set clipboard. */
        void setClipboard(const ClipboardItem *item);

        /** Activate current item. */
        void activateCurrentItem();

        /** Temporarily disable monitoring (i.e. adding new clipboard content to the first tab). */
        void disableMonitoring(bool disable);

        /** Toggle monitoring (i.e. adding new clipboard content to the first tab). */
        void toggleMonitoring();

    private slots:
        void updateTrayMenuItems();
        void trayActivated(QSystemTrayIcon::ActivationReason reason);
        void onTrayActionTriggered(uint clipboardItemHash);
        void enterSearchMode(const QString &txt);
        void tabChanged(int current);
        void tabMoved(int from, int to);
        void tabMenuRequested(const QPoint &pos, int tab);
        void tabCloseRequested(int tab);
        void addItems(const QStringList &items, const QString &tabName);
        void addItems(const QStringList &items, const QModelIndex &index);
        void addItem(const QByteArray &data, const QString &format, const QString &tabName);
        void addItem(const QByteArray &data, const QString &format, const QModelIndex &index);
        void onTimerSearch();

        void actionStarted(Action *action);
        void actionFinished(Action *action);
        void actionError(Action *action);

        void onChangeClipboardRequest(const ClipboardItem *item);

    private:
        /** Create menu bar and tray menu with items. Called once. */
        void createMenu();

        /** Delete finished action and its menu item. */
        void closeAction(Action *action);

        /** Update tray and window icon depending on current state. */
        void updateIcon();

        void updateWindowTransparency(bool mouseOver = false);

        /** Update name and icon of "disable/enable monitoring" menu actions. */
        void updateMonitoringActions();

        /** Return browser containing item or NULL. */
        ClipboardBrowser *findBrowser(const QModelIndex &index);

        /** Return browser widget in given tab @a index (or current tab). */
        ClipboardBrowser *getBrowser(int index = -1) const;

        /** Update WId for paste and last focused window if needed. */
        void updateFocusWindows();

        /** Return true only if main window owns window/widget with given WId. */
        bool isForeignWindow(WId wid);

        Ui::MainWindow *ui;
        AboutDialog *aboutDialog;
        QMenu *itemCmdMenu;
        QMenu *cmdMenu;
        QMenu *itemMenu;
        TrayMenu *trayMenu;
        QSystemTrayIcon *tray;
        bool m_browsemode;
        bool m_confirmExit;
        bool m_trayCommands;
        bool m_trayCurrentTab;
        QString m_trayTabName;
        int m_trayItems;
        bool m_trayImages;
        int m_itemPopupInterval;
        int m_lastTab;
        QTimer *m_timerSearch;

        int m_transparency;
        int m_transparencyFocused;

        bool m_activateCloses;
        bool m_activateFocuses;
        bool m_activatePastes;

        bool m_monitoringDisabled;
        QPointer<QAction> m_actionToggleMonitoring;
        QPointer<QAction> m_actionMonitoringDisabled;

        QMap<Action*, QAction*> m_actions;

        QSharedPointer<ClipboardBrowserShared> m_sharedData;

        bool m_trayItemPaste;
        WId m_pasteWindow;
        WId m_lastWindow;
    };

#endif // MAINWINDOW_H
