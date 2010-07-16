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

namespace Ui
{
    class MainWindow;
    class AboutDialog;
}

class Client : public QObject
{
    Q_OBJECT
    public slots:
        void handleMessage(const QString& message);
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

    public:
        MainWindow(const QString &css = QString(), QWidget *parent = 0);
        ~MainWindow();
        void center();
        inline bool browseMode() const { return m_browsemode; };
        void writeSettings();
        void readSettings();
        void closeEvent(QCloseEvent *event);

    private:
        Ui::MainWindow *ui;
        QDialog *aboutDialog;
        Ui::AboutDialog *aboutDialog_ui;
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

    private slots:
        void on_searchBar_textEdited(const QString &);
        void trayActivated(QSystemTrayIcon::ActivationReason reason);
        void enterSearchMode(QEvent *event);
};

#endif // MAINWINDOW_H
