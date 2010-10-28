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
#include "client_server.h"
#include <QDebug>
#include <QCloseEvent>
#include <QMenu>
#include <QMenuBar>
#include <qtlocalpeer.h>
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
    ClipboardBrowser *c = ui->clipboardBrowser;

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

    c->loadItems();
}

void MainWindow::exit()
{
    close();
    QApplication::exit();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    showMinimized();
    hide();
    event->ignore();
}

void MainWindow::createMenu()
{
    ClipboardBrowser *c = ui->clipboardBrowser;
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

    c->startMonitoring();
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

    switch( event->key() ) {
        case Qt::Key_Down:
        case Qt::Key_Up:
        case Qt::Key_PageDown:
        case Qt::Key_PageUp:
            ui->clipboardBrowser->keyEvent(event);
            break;
        
        case Qt::Key_Return:
        case Qt::Key_Enter:
            close();
            // move current item to clipboard and hide window
            ui->clipboardBrowser->moveToClipboard(
                    ui->clipboardBrowser->currentIndex() );
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
            ui->clipboardBrowser->setCurrent(0);
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
    ClipboardBrowser *c = ui->clipboardBrowser;

    ui->searchBar->clear();
    c->clearFilter();
    c->setCurrentIndex( QModelIndex() );
    enterBrowseMode();
}

void MainWindow::saveSettings()
{
    ConfigurationManager::instance(this)->windowGeometry( objectName(), saveGeometry() );
    ui->clipboardBrowser->saveItems();
}

void MainWindow::loadSettings()
{
    ConfigurationManager *cm = ConfigurationManager::instance(this);
    setStyleSheet( cm->styleSheet() );
    ui->clipboardBrowser->loadSettings();
}

void MainWindow::handleMessage(const QString& message)
{
    // deserialize list of arguments from QString
    QStringList args;
    deserialize_args(message,args);

    const QString &cmd = args.isEmpty() ? QString() : args.takeFirst();

    // client
    QStringList client_args;
    QtLocalPeer peer( NULL, QString("CopyQclient") );
    client_args.append( QString() ); // client output
    #define SHOWERROR(x) do {client_args[0] = (x); client_args.append( QString("2") ); } while(0)

    ClipboardBrowser *c = ui->clipboardBrowser;

    // show/hide main window
    if ( cmd == "toggle" )
        toggleVisible();

    // exit server
    else if ( cmd == "exit") {
        // close client and exit
        client_args[0] = QString("Exiting server.");
        this->exit();
    }

    // show menu
    else if ( cmd == "menu" )
        tray->contextMenu()->show();

    // show action dialog or run action on item
    // action [row] "cmd" "[sep]"
    else if ( cmd == "action" ) {
        if ( args.isEmpty() )
            openActionDialog(0);
        else {
            QString cmd, sep;

            // get row
            int row;
            parse(args,NULL,&row);

            // get command
            if ( !parse(args,&cmd) )
                goto actionError;

            // get separator
            if ( !parse(args,&sep) )
                sep = QString('\n');

            // no other arguments to parse
            if ( args.isEmpty() ) {
                ConfigurationManager::Command command;
                command.cmd = cmd;
                command.sep = sep;
                command.wait = false;
                action(row, &command);
                goto messageEnd;
            }

            actionError:
            SHOWERROR("Bad \"action\" command syntax!\n"
                  "action [row=0] cmd [sep=\"\\n\"]\n");
        }
    }

    // add new items
    else if ( cmd == "add" ) {
        c->addItems(args);
    }

    // edit clipboard item
    else if ( cmd == "edit" ) {
        int row;
        parse(args,NULL,&row);
        if ( !args.isEmpty() )
            SHOWERROR("Bad \"edit\" command syntax!\n"
                      "edit [row=0]\n");
        c->setCurrent(row);
        c->openEditor();
    }

    // create new item and edit it
    else if ( cmd == "new" ) {
        c->add( args.join(QString(' ')) );
        c->setCurrent(0);
        c->openEditor();
    }

    // set current item
    // select [row=1]
    else if ( cmd == "select" ) {
        int row;
        parse(args,NULL,&row);
        c->moveToClipboard(row);
    }

    // remove item from clipboard
    // remove [row=0] ...
    else if ( cmd == "remove" ) {
        if ( args.isEmpty() ) {
            c->setCurrent(0);
            c->remove();
        }
        else {
            int row;
            while( parse(args,NULL,&row) ) {
                c->setCurrent(row);
                c->remove();
            }
        }
    }

    else if ( cmd == "length" || cmd == "count" || cmd == "size" )
        client_args[0] = QString("%1\n").arg(c->length());

    // print items in given rows, format can have two arguments %1:item %2:row
    // list [format="%1\n"|row=0]
    else if ( cmd == "list" ) {
        QString fmt("%1\n");
        if ( args.isEmpty() )
            client_args[0] = fmt.arg( c->itemText(0) );
        else {
            int row;

            if ( args.isEmpty() )
                client_args[0] = fmt.arg( c->itemText(0) );
            else {
                do {
                    if ( parse(args,NULL,&row) )
                        // number
                        client_args[0] += fmt.arg( c->itemText(row) ).arg(row);
                    else {
                        // format
                        parse(args,&fmt);
                        fmt.replace(QString("\\n"),QString('\n'));
                    }
                } while( !args.isEmpty() );
            }
        }
    }

    // TODO: move item

    else
        SHOWERROR("Unknown command.\n");

    messageEnd:
    QString client_msg;
    if ( client_args.length() == 1 )
        client_args.append("0"); // default exit code
    serialize_args(client_args,client_msg);
    // empty message tells client to quit
    peer.sendMessage(client_msg,1000);
}

void MainWindow::toggleVisible()
{
    ClipboardBrowser *c = ui->clipboardBrowser;

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

void MainWindow::enterSearchMode(const QString &txt)
{
    enterBrowseMode(false);
    ui->searchBar->setText(txt);
    ui->clipboardBrowser->filterItems(txt);
}

void MainWindow::enterBrowseMode(bool browsemode)
{
    m_browsemode = browsemode;

    QLineEdit *l = ui->searchBar;
    QLabel *b = ui->findLabel;

    if(m_browsemode){
        // browse mode
        ui->clipboardBrowser->setFocus();
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
                 ui->clipboardBrowser, SLOT(addItems(QStringList)) );
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
    ClipboardBrowser *c = ui->clipboardBrowser;

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

void MainWindow::action(int row, const ConfigurationManager::Command *cmd)
{
    ClipboardBrowser *c = ui->clipboardBrowser;

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
        ui->clipboardBrowser->filterItems( ui->searchBar->text() );
        timer_search.stop();
    }
    else
        QMainWindow::timerEvent(event);
}

void MainWindow::on_searchBar_textChanged(QString )
{
    timer_search.start(100,this);
}
