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

    // commands send from client to server
    m_commands["toggle"] = Cmd_Toggle;
    m_commands["exit"]   = Cmd_Exit;
    m_commands["menu"]   = Cmd_Menu;
    m_commands["action"] = Cmd_Action;
    m_commands["add"]    = Cmd_Add;
    m_commands["write"]  = Cmd_Write;
    m_commands["_write"] = Cmd_WriteNoUpdate;
    m_commands["edit"]   = Cmd_Edit;
    m_commands["select"] = Cmd_Select;
    m_commands["remove"] = Cmd_Remove;
    m_commands["length"] = Cmd_Length;
    m_commands["size"]   = Cmd_Length;
    m_commands["count"]  = Cmd_Length;
    m_commands["list"]   = Cmd_List;
    m_commands["read"]   = Cmd_Read;

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

    // run clipboard monitor
    c->startMonitoring();
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
    qDebug( tr("Loading configuration").toLocal8Bit() );
    ConfigurationManager *cm = ConfigurationManager::instance(this);
    m_confirmExit = cm->value( ConfigurationManager::ConfirmExit ).toBool();
    setStyleSheet( cm->styleSheet() );
    ui->clipboardBrowser->loadSettings();
    qDebug( tr("Configuration loaded").toLocal8Bit() );
}

bool MainWindow::doCommand(const QString &cmd, DataList &args)
{
    ClipboardBrowser *c = ui->clipboardBrowser;
    bool noupdate = false;
    QByteArray bytes;
    QString mime;
    QMimeData *data;
    int row;

    // default "list" format
    QString fmt("%1\n");

    switch( m_commands.value(cmd, Cmd_Unknown) ) {

    // show/hide main window
    case Cmd_Toggle:
        toggleVisible();
        break;

    // exit server
    case Cmd_Exit:
        // close client and exit
        sendMessage( tr("Terminating server.\n") );
        QApplication::exit();
        break;

    // show menu
    case Cmd_Menu:
        tray->contextMenu()->show();
        break;

    // show action dialog or run action on item
    // action [row] "cmd" "[sep]"
    case Cmd_Action:
        if ( args.isEmpty() ) {
            openActionDialog(0);
        } else {
            QString cmd, sep;

            // get row
            int row;
            parse(args, NULL, &row);

            // get command
            if ( !parse(args,&cmd) ) {
                return false;
            }

            // get separator
            if ( !parse(args, &sep) )
                sep = QString('\n');

            // no other arguments to parse
            if ( args.isEmpty() ) {
                ConfigurationManager::Command command;
                command.cmd = cmd;
                command.output = true;
                command.input = true;
                command.sep = sep;
                command.wait = false;
                action(row, &command);
            } else {
                return false;
            }
        }
        break;

    // add new items
    case Cmd_Add:
        if ( args.isEmpty() ) {
            return false;
        }

        c->setAutoUpdate(false);
        for(int i=args.length()-1; i>=0; --i) {
            c->add( QString(args[i]) );
        }
        c->setAutoUpdate(true);

        c->updateClipboard();
        break;

    // add new items
    case Cmd_WriteNoUpdate:
        // don't update clipboard if command is _write
        noupdate = cmd.startsWith('_');
    case Cmd_Write:
        data = new QMimeData;
        while ( !args.isEmpty() ) {
            // get mime type
            if ( !parse(args, &mime) ) {
                return false;
            }

            // get data
            if ( args.isEmpty() ) {
                delete data;
                data = NULL;
            } else {
                data->setData( mime, args.takeFirst() );
            }
        }
        if (data) {
            if (noupdate) c->setAutoUpdate(false);
            c->add(data);
            if (noupdate) c->setAutoUpdate(true);
        } else {
            return false;
        }
        break;

    // edit clipboard item
    case Cmd_Edit:
        parse(args, NULL, &row);
        if ( args.isEmpty() ) {
            c->setCurrent(row);
            c->openEditor();
        } else {
            return false;
        }
        break;

    // set current item
    // select [row=1]
    case Cmd_Select:
        parse(args, NULL, &row);
        c->moveToClipboard(row);
        break;

    // remove item from clipboard
    // remove [row=0] ...
    case Cmd_Remove:
        if ( args.isEmpty() ) {
            c->setCurrent(0);
            c->remove();
        }
        else {
            int row;
            while( parse(args, NULL, &row) ) {
                c->setCurrent(row);
                c->remove();
            }
        }
        break;

    case Cmd_Length:
        if ( args.isEmpty() ) {
            bytes = QString("%1\n").arg(c->length()).toLocal8Bit();
        } else {
            return false;
        }
        break;

    // print items in given rows, format can have two arguments %1:item %2:row
    // list [format="%1\n"|row=0] ...
    case Cmd_List:
        if ( args.isEmpty() ) {
            bytes = fmt.arg( c->itemText(0) ).toLocal8Bit();
        } else {
            do {
                if ( parse(args, NULL, &row) )
                    // number
                    bytes.append( fmt.arg( c->itemText(row) ).arg(row) );
                else {
                    // format
                    parse(args, &fmt);
                    fmt.replace(QString("\\n"),QString('\n'));
                }
            } while( !args.isEmpty() );
        }
        break;

    // print items in given rows, format can have two arguments %1:item %2:row
    // read [mime="text/plain"|row=0] ...
    case Cmd_Read:
        mime = QString("text/plain");

        if ( args.isEmpty() ) {
            bytes = c->itemData(0)->data(mime);
        } else {
            do {
                if ( parse(args, NULL, &row) )
                    // number
                    bytes.append( c->itemData(row)->data(mime) );
                else {
                    // format
                    parse(args, &mime);
                }
            } while( !args.isEmpty() );
        }
        break;

    default:
        return false;
    }

    sendMessage(bytes);

    return true;
}

void MainWindow::sendMessage(const QByteArray &message, int exit_code)
{
    DataList client_args;
    // client output
    client_args.append( message );
    // client exit code
    client_args.append( QString("%1").arg(exit_code).toLocal8Bit() );

    QString client_msg;
    serialize_args(client_args, client_msg);

    QtLocalPeer peer( NULL, QString("CopyQclient") );
    peer.sendMessage(client_msg, 1000);
}

void MainWindow::handleMessage(const QString& message)
{
    // deserialize list of arguments from QString
    DataList args;
    deserialize_args(message, args);

    const QString cmd = args.isEmpty() ? QString() : args.takeFirst();

    // try to handle command
    if ( !doCommand(cmd, args) ) {
        sendMessage( tr("Bad command syntax. Use -h for help.\n"), 2 );
    }
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
