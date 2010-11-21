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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "aboutdialog.h"
#include "actiondialog.h"
#include <QDebug>
#include <QCloseEvent>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include "clipboardmodel.h"

MainWindow::MainWindow(QWidget *parent)
: QMainWindow(parent), ui(new Ui::MainWindow),
    aboutDialog(NULL), actionDialog(NULL)
{
    ui->setupUi(this);

    // main window: icon & title
    setWindowTitle("CopyQ");
    m_icon = QIcon(":/images/icon.svg");
    setWindowIcon(m_icon);

    // tray
    tray = new QSystemTrayIcon(this);
    tray->setIcon(m_icon);
    tray->setToolTip(
            tr("left click to show or hide, middle click to quit") );

    // create menubar & context menu
    createMenu();

    // clipboard browser widget
    ClipboardBrowser *c = browser();

    // signals & slots
    connect( c, SIGNAL(requestSearch(QString)),
            this, SLOT(enterSearchMode(QString)) );
    connect( c, SIGNAL(requestActionDialog(int,const ConfigurationManager::Command*)),
            this, SLOT(action(int,const ConfigurationManager::Command*)) );
    connect( c, SIGNAL(hideSearch()),
            this, SLOT(enterBrowseMode()) );
    connect( tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayActivated(QSystemTrayIcon::ActivationReason)) );

    // settings
    loadSettings();

    ConfigurationManager *cm = ConfigurationManager::instance(this);
    restoreGeometry( cm->windowGeometry(objectName()) );

    // notify window if configuration changes
    connect( cm, SIGNAL(configurationChanged()),
             this, SLOT(loadSettings()) );

    // browse mode by default
    m_browsemode = false;
    enterBrowseMode();

    tray->show();

    // restore clipboard history items
    c->loadItems();
}

void MainWindow::exit()
{
    int answer;
    if ( m_confirmExit ) {
        answer = QMessageBox::question(
                this,
                tr("Exit?"),
                tr("Do you want to <b>exit</b> CopyQ?"),
                QMessageBox::Yes | QMessageBox::No);
    }

    if ( !m_confirmExit || answer == QMessageBox::Yes) {
        close();
        QApplication::exit();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    //showMinimized();
    hide();
    event->ignore();
}

void MainWindow::createMenu()
{
    ClipboardBrowser *c = browser();
    QMenuBar *menubar = menuBar();
    QMenu *m;
    QMenu *menu = new QMenu(this);
    QAction *act;

    // &File
    m = menubar->addMenu( tr("&File") );

    // - show/hide
    act = new QAction( QIcon(":/images/icon.svg"),
                       tr("&Show/Hide"), this );
    QFont font(act->font());
    // bold
    font.setBold(true);
    act->setFont(font);
    act->setWhatsThis( tr("Show or hide main window") );
    connect( act, SIGNAL(triggered()), this, SLOT(toggleVisible()) );
    menu->addAction(act);
    m->addAction(act);

    // - new
    act = new QAction( QIcon(":/images/new.svg"),
                       tr("&New Item"), this );
    act->setShortcut( QString("Ctrl+N") );
    connect( act, SIGNAL(triggered()), c, SLOT(newItem()) );
    menu->addAction(act);
    m->addAction(act);

    // - preferences
    act = new QAction( QIcon(":/images/preferences.svg"),
                       tr("&Preferences"), this );
    act->setShortcut( QString("Ctrl+P") );
    connect( act, SIGNAL(triggered()), this, SLOT(openPreferences()) );
    menu->addAction(act);
    m->addAction(act);

    // - exit
    act = new QAction( QIcon(":/images/exit.svg"),
                       tr("E&xit"), this );
    act->setShortcut( QString("Ctrl+Q") );
    connect( act, SIGNAL(triggered()), this, SLOT(exit()) );
    menu->addAction(act);
    m->addAction(act);

    // &Items
    itemMenu = new QMenu(tr("&Item"), this);
    menubar->addMenu(itemMenu);
    c->setMenu(itemMenu);

    // - action dialog
    act = new QAction( QIcon(":/images/action.svg"),
                       tr("&Action..."), this );
    act->setShortcut( QString("F5") );
    act->setWhatsThis( tr("Open action dialog") );
    connect( act, SIGNAL(triggered()), this, SLOT(openActionDialog()) );
    menu->addAction(act);

    // - custom commands
    cmdMenu = menubar->addMenu(tr("&Commands"));
    cmdMenu->setEnabled(false);
    menu->addMenu(cmdMenu);

    // - about dialog
    m = menubar->addMenu( tr("&Help") );
    act = new QAction( QIcon(":/images/help.svg"),
                       tr("&Help"), this );
    act->setShortcut( QString("F1") );
    act->setWhatsThis( tr("Open help dialog") );
    connect( act, SIGNAL(triggered()), this, SLOT(openAboutDialog()) );
    menu->addAction(act);
    m->addAction(act);

    tray->setContextMenu(menu);
}

void MainWindow::showMessage(const QString &title, const QString &msg,
                             QSystemTrayIcon::MessageIcon icon, int msec)
{
    tray->showMessage(title, msg, icon, msec);
}

void MainWindow::showError(const QString &msg)
{
    tray->showMessage(QString("Error"), msg, QSystemTrayIcon::Critical);
}

void MainWindow::addMenuItem(QAction *menuItem)
{
    cmdMenu->addAction(menuItem);
    cmdMenu->setEnabled(true);
}

void MainWindow::removeMenuItem(QAction *menuItem)
{
    cmdMenu->removeAction(menuItem);
    if ( cmdMenu->isEmpty() )
        cmdMenu->setEnabled(false);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    QString txt;
    ClipboardBrowser *c = browser();

    switch( event->key() ) {
        case Qt::Key_Down:
        case Qt::Key_Up:
        case Qt::Key_PageDown:
        case Qt::Key_PageUp:
            c->keyEvent(event);
            break;
        
        case Qt::Key_Return:
        case Qt::Key_Enter:
            close();
            // move current item to clipboard and hide window
            c->moveToClipboard(
                    c->currentIndex() );
            resetStatus();
            break;

        case Qt::Key_F3:
            // focus search bar
            enterBrowseMode(false);
            break;

        case Qt::Key_Escape:
            if ( ui->searchBar->isHidden() )
                close();
            resetStatus();
            break;

        case Qt::Key_Tab:
            QMainWindow::keyPressEvent(event);
            break;

        case Qt::Key_Backspace:
            resetStatus();
            c->setCurrent(0);
            break;

        default:
            txt = event->text();
            if ( txt.isEmpty() )
                QMainWindow::keyPressEvent(event);
            else
                enterSearchMode(txt);
            break;
    }
}

void MainWindow::resetStatus()
{
    ClipboardBrowser *c = browser();

    ui->searchBar->clear();
    c->clearFilter();
    c->setCurrentIndex( QModelIndex() );
    enterBrowseMode();
}

void MainWindow::saveSettings()
{
    ConfigurationManager::instance(this)->windowGeometry( objectName(), saveGeometry() );
    browser()->saveItems();
}

void MainWindow::loadSettings()
{
    qDebug( tr("Loading configuration").toLocal8Bit() );
    ConfigurationManager *cm = ConfigurationManager::instance(this);
    m_confirmExit = cm->value( ConfigurationManager::ConfirmExit ).toBool();
    setStyleSheet( cm->styleSheet() );
    browser()->loadSettings();
    qDebug( tr("Configuration loaded").toLocal8Bit() );
}

void MainWindow::toggleVisible()
{
    ClipboardBrowser *c = browser();

    if ( isVisible() ) {
        if ( actionDialog && !actionDialog->isHidden() ) {
            actionDialog->close();
        }
        if ( aboutDialog && !aboutDialog->isHidden() ) {
            aboutDialog->close();
        }
        // if the first item is selected then select none
        // (next time the window is shown the first item will be selected)
        if ( c->currentIndex().row() == 0 )
            c->setCurrentIndex( QModelIndex() );
        close();
    } else {
        // FIXME: bypass focus prevention
        showNormal();
        raise();
        activateWindow();
        QApplication::setActiveWindow(this);

        // if no item is selected then select first
        if( !c->currentIndex().isValid() || c->currentIndex().row() == 0 ) {
            c->setCurrent(0);
        }
    }
}

void MainWindow::trayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if ( reason == QSystemTrayIcon::MiddleClick )
        exit();
    else if ( reason == QSystemTrayIcon::Trigger )
        toggleVisible();
}

void MainWindow::showMenu()
{
    tray->contextMenu()->show();
}

void MainWindow::enterSearchMode(const QString &txt)
{
    enterBrowseMode(false);
    ui->searchBar->setText(txt);
    browser()->filterItems(txt);
}

void MainWindow::enterBrowseMode(bool browsemode)
{
    m_browsemode = browsemode;

    QLineEdit *l = ui->searchBar;
    QLabel *b = ui->findLabel;

    if(m_browsemode){
        // browse mode
        browser()->setFocus();
        if ( l->text().isEmpty() ) {
            l->hide();
            b->hide();
            l->setEnabled(false);
        }
    }
    else {
        // search mode
        l->show();
        b->show();
        l->setEnabled(true);
        l->setFocus(Qt::ShortcutFocusReason);
        l->selectAll();
    }
}

void MainWindow::changeTrayIcon(const QIcon &icon)
{
    tray->setIcon(icon);
}

void MainWindow::openAboutDialog()
{
    if ( !aboutDialog ) {
        aboutDialog = new AboutDialog(this);
    }
    aboutDialog->exec();
}

void MainWindow::createActionDialog()
{
    if (!actionDialog) {
        actionDialog = new ActionDialog(this);

        connect( actionDialog, SIGNAL(addItems(QStringList)),
                 browser(), SLOT(addItems(QStringList)) );
        connect( actionDialog, SIGNAL(error(QString)),
                 this, SLOT(showError(QString)) );
        connect( actionDialog, SIGNAL(message(QString,QString)),
                 this, SLOT(showMessage(QString,QString)) );
        connect( actionDialog, SIGNAL(addMenuItem(QAction*)),
                 this, SLOT(addMenuItem(QAction*)) );
        connect( actionDialog, SIGNAL(removeMenuItem(QAction*)),
                 this, SLOT(removeMenuItem(QAction*)) );
        connect( actionDialog, SIGNAL(changeTrayIcon(QIcon)),
                 this, SLOT(changeTrayIcon(QIcon)));
    }
}

void MainWindow::openActionDialog(int row, bool modal)
{
    ClipboardBrowser *c = browser();

    createActionDialog();
    actionDialog->setInput( row >= 0 ? c->itemText(row) : c->selectedText() );
    if (modal)
        actionDialog->exec();
    else
        actionDialog->show();
}

void MainWindow::openPreferences()
{
    ConfigurationManager::instance(this)->exec();
}

ClipboardBrowser *MainWindow::browser()
{
    return ui->clipboardBrowser;
}

void MainWindow::action(int row, const ConfigurationManager::Command *cmd)
{
    ClipboardBrowser *c = browser();

    createActionDialog();

    actionDialog->setInput( row >= 0 ? c->itemText(row) : c->selectedText() );
    if (cmd) {
        actionDialog->setCommand(cmd->cmd);
        actionDialog->setSeparator(cmd->sep);
        actionDialog->setInput(cmd->input);
        actionDialog->setOutput(cmd->output);
    }
    if (!cmd || cmd->wait)
        actionDialog->exec();
    else
        actionDialog->runCommand();
}

MainWindow::~MainWindow()
{
    saveSettings();
    delete ui;
}

void MainWindow::timerEvent(QTimerEvent *event)
{
    if ( event->timerId() == timer_search.timerId() ) {
        browser()->filterItems( ui->searchBar->text() );
        timer_search.stop();
    }
    else
        QMainWindow::timerEvent(event);
}

void MainWindow::on_searchBar_textChanged(QString )
{
    timer_search.start(100,this);
}
