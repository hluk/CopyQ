/*
    Copyright (c) 2009, Lukas Holecek <hluk@email.cz>

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

#include <QtGui/QMainWindow>
#include <QSystemTrayIcon>
#include <QBasicTimer>
#include <QMap>
#include "client_server.h"
#include "configurationmanager.h"

class ClipboardModel;
class AboutDialog;
class ActionDialog;
class ClipboardBrowser;
class ClipboardItem;

namespace Ui
{
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

    public:
        MainWindow(QWidget *parent = 0);
        ~MainWindow();
        bool browseMode() const { return m_browsemode; }
        void saveSettings();
        void closeEvent(QCloseEvent *event);
        void createActionDialog();
        ClipboardBrowser *browser(int index = -1);

    private:
        Ui::MainWindow *ui;
        AboutDialog *aboutDialog;
        ActionDialog *actionDialog;
        QMenu *cmdMenu;
        QMenu *itemMenu;
        QMenu *tabMenu;
        QIcon m_icon;
        QSystemTrayIcon *tray;
        bool m_browsemode;
        QBasicTimer timer_search;
        bool m_confirmExit;
        int m_trayitems;
        ClipboardBrowser *m_browser;

        void createMenu();
        ClipboardBrowser *createTab(const QString &name);

    protected:
        void keyPressEvent(QKeyEvent *event);
        void timerEvent(QTimerEvent *event);

    public slots:
        void showMenu();

        void enterBrowseMode(bool browsemode = true);
        // show tray popup
        void showMessage(const QString &title, const QString &msg,
                         QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information,
                         int msec = 8000);
        void showError(const QString &msg);
        // show/hide window
        void showWindow();
        void toggleVisible();
        // clear search & unselect items
        void resetStatus();
        void addMenuItem(QAction *menuItem);
        void removeMenuItem(QAction *menuItem);
        void exit();
        void changeTrayIcon(const QIcon &icon);
        void updateTrayMenuItems();
        void loadSettings();

        void openAboutDialog();

        void showClipboardContent();

        /**
         Open action dialog.
         \param row Row in clipboard browser. Text in the row can be used as
                input in actiog dialog. Default is -1 which means all selected
                rows if window is opened, otherwise only the first item.
         */
        void openActionDialog(int row = -1);
        void openActionDialog(const QString &text);

        void openPreferences();

        /**
         Execute command on input text.
         */
        void action(const QString &text, const Command *cmd = NULL);

        void newTab();
        void removeTab();
        ClipboardBrowser *addTab(const QString &name);
        void newItem();
        void pasteItems();
        void copyItems();

    private slots:
        void on_searchBar_textChanged(const QString &text);
        void trayActivated(QSystemTrayIcon::ActivationReason reason);
        void trayMenuAction(QAction *act);
        void enterSearchMode(const QString &txt);
        void tabChanged();
        void addToTab(QMimeData *data, const QString &tabName);

    signals:
        void changeClipboard(const ClipboardItem *item);
    };

#endif // MAINWINDOW_H
