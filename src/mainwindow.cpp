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
#include "clipboarddialog.h"
#include <QCloseEvent>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QToolTip>
#include "clipboardbrowser.h"
#include "clipboardmodel.h"
#include "tabdialog.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),
    aboutDialog(NULL), actionDialog(NULL)
{
    ClipboardBrowser *c;

    ui->setupUi(this);

    // settings
    loadSettings();

    // store clipboard in first tab
    c = m_browser = ui->tabWidget->count() ?
                browser(0) : addTab( tr("&clipboard") );

    ui->tabWidget->setCurrentIndex(0);

    // tray
    tray = new QSystemTrayIcon(this);
    tray->setIcon( windowIcon() );
    tray->setToolTip(
            tr("left click to show or hide, middle click to quit") );

    // create menubar & context menu
    createMenu();

    // signals & slots
    connect( tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
             this, SLOT(trayActivated(QSystemTrayIcon::ActivationReason)) );
    connect( ui->tabWidget, SIGNAL(currentChanged(int)),
             this, SLOT(tabChanged()) );

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
                    tr("Do you want to <strong>exit</strong> CopyQ?"),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::Yes);
    }

    if ( !m_confirmExit || answer == QMessageBox::Yes) {
        close();
        QApplication::exit();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    ClipboardBrowser *c = browser();

    hide();

    if ( actionDialog && !actionDialog->isHidden() ) {
        actionDialog->close();
    }
    if ( aboutDialog && !aboutDialog->isHidden() ) {
        aboutDialog->close();
    }

    // deselect all
    c->clearSelection();
    c->setCurrentIndex( QModelIndex() );

    event->ignore();
}

void MainWindow::createMenu()
{
    QMenuBar *menubar = menuBar();
    QMenu *traymenu = new QMenu(this);
    QMenu *menu;
    QAction *act;

    // items before separator in tray
    traymenu->addSeparator();

    // File
    menu = menubar->addMenu( tr("&File") );

    // - show/hide
    act = menu->addAction( QIcon(":/images/icon.svg"), tr("&Show/Hide"),
                           this, SLOT(toggleVisible()) );
    act = traymenu->addAction( act->icon(), act->text(),
                               this, SLOT(toggleVisible()) );
    // bold font for default item in tray
    QFont font( act->font() );
    font.setBold(true);
    act->setFont(font);

    // - new
    act = traymenu->addAction( QIcon(":/images/new.svg"), tr("&New Item"),
                               this, SLOT(newItem()) );
    menu->addAction( act->icon(), act->text(),
                     this, SLOT(newItem()),
                     QKeySequence("Ctrl+N") );

    // - paste
    act = menu->addAction( QIcon(":/images/paste.svg"), tr("&Paste Item"),
                           this, SLOT(pasteItem()) );
    act->setShortcuts(QKeySequence::Paste);

    // - show clipboard content
    act = traymenu->addAction( QIcon(":/images/clipboard.svg"),
                               tr("Show &Clipboard Content"),
                               this, SLOT(showClipboardContent()) );
    menu->addAction( act->icon(), act->text(), this, SLOT(showClipboardContent()),
                     QKeySequence("Ctrl+S") );

    // - action dialog
    act = traymenu->addAction( QIcon(":/images/action.svg"), tr("&Action..."),
                               this, SLOT(openActionDialog()) );
    act->setWhatsThis( tr("Open action dialog") );

    // - preferences
    act = traymenu->addAction( QIcon(":/images/preferences.svg"),
                               tr("&Preferences"),
                               this, SLOT(openPreferences()) );
    menu->addAction( act->icon(), act->text(), this, SLOT(openPreferences()),
                     QKeySequence("Ctrl+P") );

    // Items
    ClipboardBrowser *c = browser();
    itemMenu = c->contextMenu();
    itemMenu->setTitle( tr("&Item") );
    menubar->addMenu(itemMenu);

    // Tabs
    tabMenu = menubar->addMenu(tr("&Tabs"));

    // add tab
    tabMenu->addAction( QIcon(":/images/tab_new.svg"), tr("&New tab"),
                        this, SLOT(newTab()),
                        QKeySequence("Ctrl+T") );
    tabMenu->addAction( QIcon(":/images/tab_remove.svg"), tr("&Remove tab"),
                        this, SLOT(removeTab()),
                        QKeySequence("Ctrl+W") );

    // Commands
    cmdMenu = menubar->addMenu(tr("&Commands"));
    cmdMenu->setEnabled(false);
    traymenu->addMenu(cmdMenu);

    // File/exit
    act = traymenu->addAction( QIcon(":/images/exit.svg"), tr("E&xit"),
                               this, SLOT(exit()) );
    menu->addAction( act->icon(), act->text(), this, SLOT(exit()),
                     QKeySequence("Ctrl+Q") );

    // Help
    menu = menubar->addMenu( tr("&Help") );
    act = menu->addAction( QIcon(":/images/help.svg"), tr("&Help"),
                           this, SLOT(openAboutDialog()),
                           QKeySequence("F1") );
    menu->addAction(act);

    // update tray menu before opening
    connect( traymenu, SIGNAL(aboutToShow()),
             this, SLOT(updateTrayMenuItems()) );

    connect( traymenu, SIGNAL(triggered(QAction*)),
             this, SLOT(trayMenuAction(QAction*)) );

    tray->setContextMenu(traymenu);
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
    QTabWidget *w = ui->tabWidget;

    if ( event->modifiers() == Qt::ControlModifier ) {
        switch( event->key() ) {
            case Qt::Key_F:
                enterBrowseMode(false);
                break;
            case Qt::Key_V:
                browser()->add( browser(0)->at(0) );
                break;
            case Qt::Key_Tab:
                w->setCurrentIndex( (w->currentIndex() + 1) % w->count() );
                break;
            case Qt::Key_Backtab:
                w->setCurrentIndex( (w->currentIndex() - 1) % w->count() );
                break;
            default:
                QMainWindow::keyPressEvent(event);
                break;
        }
        return;
    }

    switch( event->key() ) {
        case Qt::Key_Down:
        case Qt::Key_Up:
        case Qt::Key_PageDown:
        case Qt::Key_PageUp:
            c->keyEvent(event);
            break;
        
        case Qt::Key_Return:
        case Qt::Key_Enter:
            // move current item to clipboard and hide window
            if ( c->selectionModel()->selectedIndexes().count() > 1 )
                c->add( c->selectedText() );
            else
                c->moveToClipboard( c->currentIndex() );
            close();
            break;

        case Qt::Key_F3:
            // focus search bar
            enterBrowseMode(false);
            break;

        case Qt::Key_Escape:
            if ( ui->searchBar->isHidden() )
                close();
            else
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
            QMainWindow::keyPressEvent(event);
            if ( !event->isAccepted() ) {
                txt = event->text();
                if ( !txt.isEmpty() )
                    enterSearchMode(txt);
            }
            break;
    }
}

void MainWindow::resetStatus()
{
    if ( !ui->searchBar->text().isEmpty() ) {
        ui->searchBar->clear();
        browser()->clearFilter();
    }
    enterBrowseMode();
}

void MainWindow::saveSettings()
{
    ConfigurationManager *cm = ConfigurationManager::instance(this);
    QTabWidget *w = ui->tabWidget;
    QStringList tabs;

    cm->windowGeometry( objectName(), saveGeometry() );

    for( int i = 0; i < w->count(); ++i ) {
        browser(i)->saveItems();
        tabs << w->tabText(i);
    }
    cm->setValue("tabs", tabs);

    cm->saveSettings();
}

void MainWindow::loadSettings()
{
    log( tr("Loading configuration") );

    ConfigurationManager *cm = ConfigurationManager::instance(this);
    m_confirmExit = cm->value("confirm_exit").toBool();
    setStyleSheet( cm->styleSheet() );
    m_trayitems = cm->value("tray_items").toInt();

    /* are tabs already loaded? */
    bool loaded = ui->tabWidget->count() > 0;
    QStringList tabs = cm->value("tabs").toStringList();
    foreach(const QString &name, tabs) {
        if (loaded)
            addTab(name)->loadSettings();
        else
            addTab(name);
    }

    log( tr("Configuration loaded") );
}

void MainWindow::showWindow()
{
    if ( isVisible() ) {
        if ( QApplication::focusWidget() )
            return;

        /* close the main window first so it can popup on current workspace */
        close();
        /* process pending events to ensure that the window will be opened at
           correct position */
        QApplication::processEvents();
    }

    showNormal();
    raise();
    activateWindow();

    // if no item is selected then select first
    ClipboardBrowser *c = browser();
    if( c->selectionModel()->selectedIndexes().size() <= 1 &&
            c->currentIndex().row() <= 0 ) {
        c->setCurrent(0);
    }
    c->scrollTo( c->currentIndex() );

    QApplication::processEvents();
    raiseWindow(winId());
}

void MainWindow::toggleVisible()
{
    // FIXME: focus window if not focused
    if ( isVisible() ) {
        close();
    } else {
        showWindow();
    }
}

void MainWindow::trayMenuAction(QAction *act)
{
    QVariant data = act->data();

    if ( data.isValid() ) {
        int row = data.toInt();
        ClipboardBrowser *c = browser();
        if ( row < c->length() ) {
            c->moveToClipboard(row);
        }
    }
}

void MainWindow::trayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if ( reason == QSystemTrayIcon::MiddleClick ) {
        exit();
    } else if ( reason == QSystemTrayIcon::Trigger ) {
        toggleVisible();
    }
}

void MainWindow::showMenu()
{
    updateTrayMenuItems();
    tray->contextMenu()->show();
}

void MainWindow::tabChanged()
{
    // update item menu (necessary for keyboard shortcuts to work)
    QMenu *m = itemMenu;
    itemMenu = browser()->contextMenu();
    itemMenu->setTitle( tr("&Item") );
    menuBar()->insertMenu( m->menuAction(), itemMenu );
    menuBar()->removeAction( m->menuAction() );
    browser()->filterItems( ui->searchBar->text() );
}

void MainWindow::enterSearchMode(const QString &txt)
{
    enterBrowseMode(false);
    ui->searchBar->setText(txt);
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

void MainWindow::updateTrayMenuItems()
{
    QAction *act;
    QAction *sep;
    int i, len;
    QMenu *menu = tray->contextMenu();
    ClipboardBrowser *c = browser();

    QList<QAction *> actions = menu->actions();
    for( i = 0, len = actions.size(); i<len && !actions[i]->isSeparator(); ++i )
        menu->removeAction(actions[i]);
    sep  = actions[i];

    len = qMin( m_trayitems, c->length() );
    unsigned char hint('0');
    for( i = 0; i < len; ++i ) {
        QFont font = menu->font();
        font.setItalic(true);

        QFontMetrics fm(font);
        QString text = fm.elidedText( c->itemText(i).left(512).simplified(),
                                      Qt::ElideRight, 240 );

        /* FIXME: keypad numbers don't work */
        if (hint <= '9') {
            QChar h(hint);
            text.prepend( QString("&%1. ").arg(h) );
            ++hint;
        }

        act = menu->addAction(text);
        act->setFont(font);
        act->setData( QVariant(i) );

        menu->insertAction(sep, act);
    }
}

void MainWindow::openAboutDialog()
{
    if ( !aboutDialog ) {
        aboutDialog = new AboutDialog(this);
    }
    aboutDialog->exec();
}

void MainWindow::showClipboardContent()
{
    ClipboardDialog *d = new ClipboardDialog;
    connect( d, SIGNAL(finished(int)), d, SLOT(deleteLater()) );
    d->exec();
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

void MainWindow::openActionDialog(int row)
{
    ClipboardBrowser *c = browser();
    QString text;
    if (row >= 0) {
        text = c->itemText(row);
    } else if ( isVisible() ) {
        text = c->selectedText();
    } else {
        text = c->itemText(0);
    }
    openActionDialog(text);
}

void MainWindow::openActionDialog(const QString &text)
{
    createActionDialog();
    actionDialog->setInputText(text);
    actionDialog->exec();
}

void MainWindow::openPreferences()
{
    ConfigurationManager::instance(this)->exec();
}

ClipboardBrowser *MainWindow::browser(int index)
{
    QWidget *w = ui->tabWidget->widget(
                index < 0 ? ui->tabWidget->currentIndex() : index );
    return dynamic_cast<ClipboardBrowser*>(w);
}

ClipboardBrowser *MainWindow::addTab(const QString name)
{
    QTabWidget *w = ui->tabWidget;

    /* check name */
    for( int i = 0; i < w->count(); ++i )
        if ( name == w->tabText(i) )
            return browser(i);

    ClipboardBrowser *c = new ClipboardBrowser(name, this);

    c->loadSettings();
    c->loadItems();
    c->setAutoUpdate(true);

    connect( c, SIGNAL(changeClipboard(const ClipboardItem*)),
             this, SIGNAL(changeClipboard(const ClipboardItem*)) );
    connect( c, SIGNAL(requestSearch(QString)),
             this, SLOT(enterSearchMode(QString)) );
    connect( c, SIGNAL(requestActionDialog(QString,const Command*)),
             this, SLOT(action(QString,const Command*)) );
    connect( c, SIGNAL(hideSearch()),
             this, SLOT(enterBrowseMode()) );
    connect( c, SIGNAL(requestShow()),
             this, SLOT(show()) );

    w->addTab(c, name);
    w->setCurrentIndex( w->count()-1 );

    return c;
}

void MainWindow::newItem()
{
    ClipboardBrowser *c = browser( ui->tabWidget->currentIndex() );
    if (c) {
        showWindow();
        c->setFocus();
        c->newItem();
    }
}

void MainWindow::pasteItem()
{
    ClipboardBrowser *c = browser( ui->tabWidget->currentIndex() );
    const QMimeData *data = clipboardData();
    if (data)
        c->add( cloneData(*data) );
}

void MainWindow::action(const QString &text, const Command *cmd)
{
    createActionDialog();

    actionDialog->setInputText(text);
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

void MainWindow::newTab()
{
    TabDialog *d = new TabDialog(this);

    connect( d, SIGNAL(addTab(QString)),
            this, SLOT(addTab(QString)) );
    connect( d, SIGNAL(finished(int)),
            d, SLOT(deleteLater()) );

    d->open();
}

void MainWindow::removeTab()
{
    QTabWidget *w = ui->tabWidget;
    int i = w->currentIndex();

    if ( w->count() > 1 ) {
        int answer = QMessageBox::question(
                    this,
                    tr("Exit?"),
                    tr("Do you want to remove tab <strong>%1</strong>?"
                       ).arg( w->tabText(i).remove('&')),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::Yes);
        if (answer == QMessageBox::Yes) {
            ConfigurationManager::instance()->removeItems( w->tabText(i) );
            browser(i)->deleteLater();
            w->removeTab(i);
        }
    }
}

MainWindow::~MainWindow()
{
    saveSettings();
    tray->hide();
    delete ui;
}

void MainWindow::timerEvent(QTimerEvent *event)
{
    if ( event->timerId() == timer_search.timerId() ) {
        browser()->filterItems( ui->searchBar->text() );
        timer_search.stop();
    } else {
        QMainWindow::timerEvent(event);
    }
}

void MainWindow::on_searchBar_textChanged(QString )
{
    timer_search.start(300, this);
}
