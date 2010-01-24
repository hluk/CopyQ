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
//#include "itemdelegate.h"

class ClipboardModel;

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
        inline bool browseMode() const { return m_browsemode; };
        void writeSettings();
        void readSettings();

    private:
        Ui::MainWindow *ui;
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
       void showError(const QString);

    private slots:
        void on_searchBar_textEdited(const QString &);
        void trayActivated(QSystemTrayIcon::ActivationReason reason);
        void enterSearchMode(QEvent *event);
};

#endif // MAINWINDOW_H
