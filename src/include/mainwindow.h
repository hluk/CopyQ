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
#include <QSystemTrayIcon>
#include <QMap>
#include <QSharedPointer>
#include "configurationmanager.h"

class ClipboardModel;
class AboutDialog;
class ClipboardBrowser;
class ClipboardBrowserShared;
class ClipboardItem;
class Action;
class ActionDialog;
class QMimeData;

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
        explicit MainWindow(QWidget *parent = 0);
        ~MainWindow();

        /** Return true if in browse mode. */
        bool browseMode() const { return m_browsemode; }

        /** Save settings, items in browsers and window geometry. */
        void saveSettings();

        /** Hide (minimize to tray) window on close. */
        void closeEvent(QCloseEvent *event);

        /** Create new action dialog. */
        ActionDialog *createActionDialog();

        /** Return browser widget in given tab @a index (or current tab). */
        ClipboardBrowser *browser(int index = -1) const;

        /** Return tab index for browser widget (-1 if not found). */
        int tabIndex(const ClipboardBrowser *c) const;

        /**
         * Find tab with given @a name.
         * @return found tab or NULL
         */
        ClipboardBrowser *findTab(const QString &name);

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

    signals:
        /** Request clipboard change. */
        void changeClipboard(const ClipboardItem *item);

        /** User begins or stops to edit an item in a tab. */
        void editingActive(bool active);

    protected:
        void keyPressEvent(QKeyEvent *event);
        void dragEnterEvent(QDragEnterEvent *event);
        void dropEvent(QDropEvent *event);
        bool eventFilter(QObject *object, QEvent *event);

    public slots:
        /**
         * Show tray menu.
         * @return Window ID for the tray menu.
         */
        WId showMenu();

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
        /** Change tray icon. */
        void changeTrayIcon(const QIcon &icon);
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
        void action(const QMimeData &data, const Command &cmd);

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
        void newItem();
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

    private slots:
        void updateTrayMenuItems();
        void trayActivated(QSystemTrayIcon::ActivationReason reason);
        void trayMenuAction();
        void enterSearchMode(const QString &txt);
        void tabChanged(int current);
        void tabMoved(int from, int to);
        void tabMenuRequested(const QPoint &pos, int tab);
        void tabCloseRequested(int tab);
        void addItems(const QStringList &items, const QString &tabName);
        void addItem(const QByteArray &data, const QString &format, const QString &tabName);
        void onTimerSearch();

        void actionStarted(Action *action);
        void actionFinished(Action *action);
        void actionError(Action *action);

    private:
        /** Create menu bar and tray menu with items. Called once. */
        void createMenu();

        /** Delete finished action and its menu item. */
        void closeAction(Action *action);

        Ui::MainWindow *ui;
        AboutDialog *aboutDialog;
        QMenu *itemCmdMenu;
        QMenu *cmdMenu;
        QMenu *itemMenu;
        QSystemTrayIcon *tray;
        bool m_browsemode;
        bool m_confirmExit;
        bool m_trayCommands;
        bool m_trayCurrentTab;
        QString m_trayTabName;
        int m_trayItems;
        bool m_trayImages;
        int m_lastTab;
        QTimer *m_timerSearch;

        QMap<Action*,QAction*> m_actions;

        QSharedPointer<ClipboardBrowserShared> m_sharedData;
    };

#endif // MAINWINDOW_H
