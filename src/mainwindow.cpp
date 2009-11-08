/*
    Copyright (c) 2009, Lukas Holecek <hluk@email.cz>

    This file is part of Copyq.

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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QtGui/QDesktopWidget>
#include <QDebug>
#include <QDataStream>
#include <QFile>
#include <QDomDocument>
#include <QDomElement>
#include <QDomAttr>
#include <QSettings>
#include <QCloseEvent>

MainWindow::MainWindow(const QString &css, QWidget *parent)
: QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    tray = new QSystemTrayIcon(this);
    tray->setIcon(QIcon(":/images/icon.png"));
    tray->setToolTip(tr("left click to show or hide, middle click to quit"));

    connect( ui->clipboardBrowser, SIGNAL(requestSearch(QEvent*)),
            this, SLOT(enterSearchMode(QEvent*)) );
    connect( ui->clipboardBrowser, SIGNAL(hideSearch()),
            this, SLOT(enterBrowseMode()) );
    connect( ui->searchBar, SIGNAL(textEdited(const QString&)),
            ui->clipboardBrowser, SLOT(filterItems(const QString&)) );
    connect( tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayActivated(QSystemTrayIcon::ActivationReason)) );

    // settings
    readSettings();

    // browse mode by default
    m_browsemode = false;
    enterBrowseMode();

    ui->clipboardBrowser->setStyleSheet(css);
    tray->show();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    switch(event->key()) {
        case Qt::Key_Down:
        case Qt::Key_Up:
        case Qt::Key_PageDown:
        case Qt::Key_PageUp:
            ui->clipboardBrowser->keyEvent(event);
            break;
        
        case Qt::Key_Return:
            ui->clipboardBrowser->moveToClipboard();
            hide();
            break;

        case Qt::Key_F3:
            // TODO: search (with shift -> backwards)
            break;

        case Qt::Key_Escape:
            if ( ui->searchBar->isVisible() )
                enterBrowseMode();
            else
                hide();
            break;

        default:
            QMainWindow::keyPressEvent(event);
            break;
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    writeSettings();
    event->accept();
}

void MainWindow::writeSettings()
{
    QSettings settings;

    settings.beginGroup("MainWindow");
    settings.setValue("size", size());
    settings.setValue("pos", pos());
    settings.endGroup();
}

void MainWindow::readSettings()
{
    QSettings settings;

    settings.beginGroup("MainWindow");
    resize(settings.value("size", QSize(400, 400)).toSize());
    move(settings.value("pos", QPoint(200, 200)).toPoint());
    settings.endGroup();
}

void MainWindow::handleMessage(const QString& message)
{
    if ( message == "toggle" ) {
        if ( this->isVisible() )
            hide();
        else
            show();
    }
}

void MainWindow::trayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if ( reason == QSystemTrayIcon::MiddleClick )
        close();
    else if (this->isVisible())
        hide();
    else
        show();
}

void MainWindow::enterSearchMode(QEvent *event)
{
    enterBrowseMode(false);
    ui->searchBar->event(event);
    if ( ui->searchBar->text().isEmpty() )
        enterBrowseMode(true);
}

void MainWindow::enterBrowseMode(bool browsemode)
{
    SearchBar *l = ui->searchBar;

    if (m_browsemode == browsemode) return;
    m_browsemode = browsemode;

    if(m_browsemode){
        // browse mode
        l->hide();
        ui->clipboardBrowser->setFocus(Qt::ShortcutFocusReason);
    }
    else {
        // search mode
        l->show();
        l->setFocus(Qt::ShortcutFocusReason);
        l->selectAll();
    }
}

void MainWindow::center() {
    int x, y;
    int screenWidth, screenHeight;
    int width, height;
    QSize windowSize;

    QDesktopWidget *desktop = QApplication::desktop();

    width = frameGeometry().width();
    height = frameGeometry().height();

    screenWidth = desktop->width();
    screenHeight = desktop->height();

    x = (screenWidth - width) / 2;
    y = (screenHeight - height) / 2;

    move( x, y );
}

MainWindow::~MainWindow()
{
    ui->clipboardBrowser->writeSettings();
    delete ui;
}

// vim: tags +=~/.vim/tags/qt4
