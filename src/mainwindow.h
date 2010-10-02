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
#include <QTextDocument>
#include <QSystemTrayIcon>
#include <QBasicTimer>

class ClipboardModel;
class AboutDialog;
class ActionDialog;

namespace Ui
{
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

    public:
        MainWindow(const QString &css = QString(), QWidget *parent = 0);
        ~MainWindow();
        void center();
        bool browseMode() const { return m_browsemode; }
        void writeSettings();
        void readSettings();
        void closeEvent(QCloseEvent *event);
        void createActionDialog();

    private:
        Ui::MainWindow *ui;
        AboutDialog *aboutDialog;
        ActionDialog *actionDialog;
        //ItemDelegate delegate;
        QIcon m_icon;
        QSystemTrayIcon *tray;
        bool m_browsemode;
        QBasicTimer timer_search;

    protected:
        void keyPressEvent(QKeyEvent *event);
        void timerEvent(QTimerEvent *event);

    public slots:
       void handleMessage(const QString& message);
       void enterBrowseMode(bool browsemode = true);
       // show tray popup
       void showMessage(const QString &title, const QString &msg,
                        QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information,
                        int msec = 8000);
       void showError(const QString &msg);
       // show/hide window
       void toggleVisible();
       // clear search & unselect items
       void resetStatus();
       void addMenuItem(QAction *menuItem);
       void removeMenuItem(QAction *menuItem);
       void exit();

       void openAboutDialog();

       /**
         Open action dialog.
         \param row Row in clipboard browser. Text in the row can be used as
                input in actiog dialog. Default is -1 which means all selected
                row(s).
         \param modal action dialog modality
         */
       void openActionDialog(int row = -1, bool modal = true);

       /**
         Execute command on an item.
         \param row Row number or -1 for selected items.
         \param cmd Command string (all occurrencies of '%s' will be replaced
                with text of item on given row or concatenated text of
                selected items).
         \param input Text of item on given row or concatenated text of
                selected items is send on standard input of the command.
         \param output If true the command output will be saved into new
                items.
         \param sep String using which the output is separated to new items
                (ignored if output parameter is false).
         \param wait If true the action dialog with the command is shown,
                otherwise the command is executed immediately.
         */
       void action(int row, const QString &cmd,
                   const QString &sep = QString("\\n"),
                   bool input = false, bool output = false,
                   bool wait = false);

    private slots:
        void on_searchBar_textEdited(const QString &);
        void trayActivated(QSystemTrayIcon::ActivationReason reason);
        void enterSearchMode(QEvent *event);
};

#endif // MAINWINDOW_H
