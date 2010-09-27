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
#include "ui_aboutdialog.h"
#include "client_server.h"
#include <QtGui/QDesktopWidget>
#include <QDebug>
#include <QDataStream>
#include <QFile>
#include <QUrl>
#include <QDomDocument>
#include <QDomElement>
#include <QDomAttr>
#include <QSettings>
#include <QCloseEvent>
#include <QMenu>
#include <qtlocalpeer.h>
#include "clipboardmodel.h"

MainWindow::MainWindow(const QString &css, QWidget *parent)
: QMainWindow(parent), ui(new Ui::MainWindow), aboutDialog(NULL)
{
    // global stylesheet
    setStyleSheet(css);

    ui->setupUi(this);
    setFocusPolicy(Qt::StrongFocus);

    ClipboardBrowser *c = ui->clipboardBrowser;
    c->readSettings(css);
    c->startMonitoring();

    // main window: icon & title
    this->setWindowTitle("CopyQ");
    m_icon = QIcon(":images/icon.svg");
    setWindowIcon(m_icon);

    // tray
    tray = new QSystemTrayIcon(this);
    tray->setIcon(m_icon);
    tray->setToolTip(
            tr("left click to show or hide, middle click to quit") );

    // menu
    QMenu *menu = new QMenu(this);
    QAction *act;
    // - show/hide
    act = new QAction( tr("&Show/Hide"), this );
    act->setWhatsThis( tr("Show or hide main window") );
    connect( act, SIGNAL(triggered()), this, SLOT(toggleVisible()) );
    menu->addAction(act);
    // - action dialog
    act = new QAction( tr("&Action..."), this );
    act->setWhatsThis( tr("Open action dialog") );
    connect( act, SIGNAL(triggered()), c, SLOT(openActionDialog()) );
    menu->addAction(act);
    // - exit
    act = new QAction( tr("E&xit..."), this );
    connect( act, SIGNAL(triggered()), this, SLOT(exit()) );
    menu->addAction(act);

    tray->setContextMenu(menu);

    // signals & slots
    connect( c, SIGNAL(requestSearch(QEvent*)),
            this, SLOT(enterSearchMode(QEvent*)) );
    connect( c, SIGNAL(hideSearch()),
            this, SLOT(enterBrowseMode()) );
    connect( c, SIGNAL(error(QString)),
            this, SLOT(showError(QString)) );
    connect( c, SIGNAL(message(QString,QString)),
            this, SLOT(showMessage(QString,QString)) );
    connect( c, SIGNAL(addMenuItem(QAction*)),
            this, SLOT(addMenuItem(QAction*)) );
    connect( c, SIGNAL(removeMenuItem(QAction*)),
            this, SLOT(removeMenuItem(QAction*)) );
    connect( tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayActivated(QSystemTrayIcon::ActivationReason)) );

    // settings
    readSettings();

    // browse mode by default
    m_browsemode = false;
    enterBrowseMode();

    tray->show();
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
    tray->contextMenu()->addAction(menuItem);
}

void MainWindow::removeMenuItem(QAction *menuItem)
{
    tray->contextMenu()->removeAction(menuItem);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    QFile *f;

    if ( event->modifiers() == Qt::ControlModifier )
        if ( event->key() == Qt::Key_Q )
            exit();

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

        // show about dialog
        case Qt::Key_F1:
            if ( !aboutDialog ) {
                aboutDialog = new QDialog(this);
                aboutDialog_ui = new Ui::AboutDialog;
                aboutDialog_ui->setupUi(aboutDialog);

                f = new QFile(":/aboutdialog.html");
                if ( f->open(QIODevice::ReadOnly) ) {
                    aboutDialog_ui->textEdit->setText( QString::fromUtf8(f->readAll()) );
                }
                delete f;
            }
            aboutDialog->show();
            break;

        case Qt::Key_F3:
            // focus search bar
            enterBrowseMode(false);
            break;

        case Qt::Key_Escape:
            close();
            resetStatus();
            enterBrowseMode();
            break;

        default:
            QMainWindow::keyPressEvent(event);
            break;
    }
}

void MainWindow::resetStatus()
{
    ui->searchBar->clear();
    ui->clipboardBrowser->clearFilter();
    ui->clipboardBrowser->setCurrentIndex( QModelIndex() );
    ui->clipboardBrowser->scrollToTop();
}

void MainWindow::writeSettings()
{
    QSettings settings;

    settings.beginGroup("MainWindow");
    settings.setValue("size", size());
    settings.setValue("pos", pos());
    settings.endGroup();

    ui->clipboardBrowser->writeSettings();
    ui->clipboardBrowser->saveItems();
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

    // force check clipboard (update clipboard browser)
    c->checkClipboard();

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
            c->openActionDialog(0);
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
                c->action(row, cmd, sep);
                goto messageEnd;
            }

            actionError:
            SHOWERROR("Bad \"action\" command syntax!\n"
                  "action [row=0] cmd [sep=\"\\n\"]\n");
        }
    }

    // add new item
    else if ( cmd == "add" )
        c->add( args.join(QString(' ')) );

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
        c->add( args.join(QString(' ')), false );
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
            QString arg;

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
    if ( isVisible() )
        close();
    else {
        // TODO: bypass focus prevention
        showNormal();
        raise();
        activateWindow();
        QApplication::setActiveWindow(this);
    }
}

void MainWindow::trayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if ( reason == QSystemTrayIcon::MiddleClick )
        exit();
    else if ( reason == QSystemTrayIcon::Trigger )
        toggleVisible();
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
    QLineEdit *l = ui->searchBar;

    if (m_browsemode == browsemode) return;
    m_browsemode = browsemode;

    if(m_browsemode){
        // browse mode
        if ( l->text().isEmpty() )
            l->hide();
        ui->clipboardBrowser->setFocus();
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
    writeSettings();
    delete ui;
}

void MainWindow::on_searchBar_textEdited(const QString &)
{
    timer_search.start(100,this);
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
