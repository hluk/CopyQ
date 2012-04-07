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
#include "clipboardbrowser.h"
#include "clipboardmodel.h"
#include "clipboarditem.h"
#include "tabdialog.h"
#include "client_server.h"

#include <QCloseEvent>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , aboutDialog(NULL)
    , actionDialog(NULL)
    , cmdMenu(NULL)
    , itemMenu(NULL)
    , tabMenu(NULL)
    , tray(NULL)
    , m_browsemode(false)
    , m_confirmExit(true)
    , m_trayitems(5)
    , m_timerSearch( new QTimer(this) )
{
    ui->setupUi(this);

    // settings
    loadSettings();

    if ( ui->tabWidget->count() == 0 )
        addTab( tr("&clipboard") );

    ui->tabWidget->setCurrentIndex(0);

    // tray
    tray = new QSystemTrayIcon(this);
    tray->setIcon( windowIcon() );
    tray->setToolTip(
            tr("left click to show or hide, middle click to quit") );

    // search timer
    m_timerSearch->setSingleShot(true);
    m_timerSearch->setInterval(200);

    // create menubar & context menu
    createMenu();

    // signals & slots
    connect( tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
             this, SLOT(trayActivated(QSystemTrayIcon::ActivationReason)) );
    connect( ui->tabWidget, SIGNAL(currentChanged(int)),
             this, SLOT(tabChanged()) );
    connect( m_timerSearch, SIGNAL(timeout()),
             this, SLOT(onTimerSearch()) );
    connect( ui->searchBar, SIGNAL(textChanged(QString)),
             m_timerSearch, SLOT(start()) );

    ConfigurationManager *cm = ConfigurationManager::instance(this);
    restoreGeometry( cm->windowGeometry(objectName()) );

    // notify window if configuration changes
    connect( cm, SIGNAL(configurationChanged()),
             this, SLOT(loadSettings()) );

    // browse mode by default
    enterBrowseMode();

    tray->show();
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
    act = traymenu->addAction( QIcon(":/images/icon.svg"), tr("&Show/Hide"),
                           this, SLOT(toggleVisible()) );
    // bold font for default item in tray
    QFont font( act->font() );
    font.setBold(true);
    act->setFont(font);

    // - separator
    menu->addSeparator();

    // - new
    act = traymenu->addAction( QIcon(":/images/new.svg"), tr("&New Item"),
                               this, SLOT(newItem()) );
    menu->addAction( act->icon(), act->text(),
                     this, SLOT(newItem()),
                     QKeySequence::New );

    // - paste items
    act = menu->addAction( QIcon(":/images/paste.svg"), tr("&Paste Items"),
                           this, SLOT(pasteItems()) );
    act->setShortcuts(QKeySequence::Paste);

    // - copy items
    act = menu->addAction( QIcon(":/images/copy.svg"),
                           tr("&Copy Selected Items"),
                           this, SLOT(copyItems()) );
    act->setShortcuts(QKeySequence::Copy);

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

    // - separator
    menu->addSeparator();

    // - preferences
    act = traymenu->addAction( QIcon(":/images/preferences.svg"),
                               tr("&Preferences"),
                               this, SLOT(openPreferences()) );
    menu->addAction( act->icon(), act->text(), this, SLOT(openPreferences()),
                     QKeySequence("Ctrl+P") );

    // - separator
    menu->addSeparator();

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
                        QKeySequence::AddTab );
    tabMenu->addAction( QIcon(":/images/tab_remove.svg"), tr("&Remove tab"),
                        this, SLOT(removeTab()),
                        QKeySequence::Close );

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
                           QKeySequence::HelpContents );
    menu->addAction(act);

    // update tray menu before opening
    connect( traymenu, SIGNAL(aboutToShow()),
             this, SLOT(updateTrayMenuItems()) );

    connect( traymenu, SIGNAL(triggered(QAction*)),
             this, SLOT(trayMenuAction(QAction*)) );

    tray->setContextMenu(traymenu);
}

ClipboardBrowser *MainWindow::createTab(const QString &name)
{
    QTabWidget *w = ui->tabWidget;

    /* check name */
    for( int i = 0; i < w->count(); ++i )
        if ( name == w->tabText(i) )
            return browser(i);

    ClipboardBrowser *c = new ClipboardBrowser(this);

    c->setID(name);
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
    connect( c, SIGNAL(addToTab(QMimeData*,QString)),
             this, SLOT(addToTab(QMimeData*,QString)),
             Qt::DirectConnection );

    w->addTab(c, name);

    return c;
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
            if ( !c->hasFocus() )
                c->setFocus();
            else
                c->keyEvent(event);
            break;
        
        case Qt::Key_Return:
        case Qt::Key_Enter:
            // move current item to clipboard and hide window
            if ( c->selectionModel()->selectedIndexes().count() > 1 )
                c->add( c->selectedText() );
            else
                c->moveToClipboard( c->currentIndex() );
            resetStatus();
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
    cm->setTabs(tabs);

    cm->saveSettings();
}

void MainWindow::loadSettings()
{
    log( tr("Loading configuration") );

    ConfigurationManager *cm = ConfigurationManager::instance(this);
    m_confirmExit = cm->value("confirm_exit").toBool();
    m_trayitems = cm->value("tray_items").toInt();

    /* are tabs already loaded? */
    bool loaded = ui->tabWidget->count() > 0;
    QStringList tabs = cm->value("tabs").toStringList();
    foreach(const QString &name, tabs) {
        ClipboardBrowser *c;
        c = createTab(name);
        if (loaded)
            c->loadSettings();
    }
    cm->setTabs(tabs);

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

void MainWindow::addToTab(QMimeData *data, const QString &tabName)
{
    ClipboardBrowser *c;
    QTabWidget *tabs = ui->tabWidget;
    int i = 0;

    if ( !tabName.isEmpty() )
        for ( ; i < tabs->count() && tabs->tabText(i) != tabName; ++i );

    if ( i < tabs->count() ) {
        c = browser(i);
    } else if ( !tabName.isEmpty() ) {
        c = createTab(tabName);
        saveSettings();
    } else {
        return;
    }

    c->setAutoUpdate(false);
    if ( !c->select(hash(*data, data->formats())) ) {
        if ( c->length() > 0 ) {
            /* merge data with first item if it is same */
            ClipboardItem *first = c->at(0);
            if ( data->hasText() && data->text() == first->text() ) {
                QStringList formats = data->formats();
                QStringList first_formats = first->formats();
                foreach (const QString &format, first_formats) {
                    if ( !formats.contains(format) )
                        data->setData( format, first->data()->data(format) );
                }
                c->model()->removeRow(0);
            }
        }
        /* force adding item if tab name is specified */
        bool force = !tabName.isEmpty();
        c->add( cloneData(*data), force );
    }
    c->setAutoUpdate(true);
}

void MainWindow::addItems(const QStringList &items, const QString &tabName)
{
    ClipboardBrowser *c = tabName.isEmpty() ? browser(0) : addTab(tabName);
    c->addItems(items);
}

void MainWindow::onTimerSearch()
{
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

        if (hint <= '9') {
            /* FIXME: keypad numbers don't work */
            act = menu->addAction(QString('&') + hint + ". " + text);
            ++hint;
        } else {
            act = menu->addAction(text);
        }

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
    aboutDialog->show();
    aboutDialog->activateWindow();
}

void MainWindow::showClipboardContent()
{
    ClipboardDialog *d = new ClipboardDialog;
    connect( d, SIGNAL(finished(int)), d, SLOT(deleteLater()) );
    d->show();
}

void MainWindow::createActionDialog()
{
    if (!actionDialog) {
        actionDialog = new ActionDialog(this);

        connect( actionDialog, SIGNAL(addItems(QStringList, QString)),
                 this, SLOT(addItems(QStringList, QString)) );
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

ClipboardBrowser *MainWindow::addTab(const QString &name)
{
    QTabWidget *w = ui->tabWidget;
    ClipboardBrowser *c = createTab(name);
    w->setCurrentIndex( w->count()-1 );
    saveSettings();

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

void MainWindow::pasteItems()
{
    const QMimeData *data = clipboardData();
    if (data) {
        const QByteArray bytes = data->data("application/x-copyq-item");
        QDataStream in(bytes);
        ClipboardBrowser *c = browser();
        ClipboardItem item;

        c->setAutoUpdate(false);
        c->setUpdatesEnabled(false);
        while( !in.atEnd() ) {
            in >> item;
            c->add(item, true);
        }
        c->setUpdatesEnabled(true);
        c->setAutoUpdate(true);
        c->updateClipboard();
    }
}

void MainWindow::copyItems()
{
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);

    ClipboardBrowser *c = browser();
    QModelIndexList indexes = c->selectionModel()->selectedRows();

    if ( indexes.isEmpty() )
        return;

    ClipboardModel *model = static_cast<ClipboardModel *>( c->model() );

    /* Copy items in reverse (items will be pasted correctly). */
    for ( int i = indexes.size()-1; i >= 0; --i ) {
        out << *model->get( indexes.at(i).row() );
    }

    QMimeData data;
    data.setText( c->selectedText() );
    data.setData("application/x-copyq-item", bytes);

    ClipboardItem item;
    item.setData( cloneData(data) );
    emit changeClipboard(&item);
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

        QStringList tabs;
        QTabWidget *w = ui->tabWidget;
        for( int i = 0; i < w->count(); ++i )
            tabs << w->tabText(i);
        actionDialog->setOutputTabs(tabs, cmd->outputTab);
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

void MainWindow::removeTab(bool ask, int tab_index)
{
    QTabWidget *w = ui->tabWidget;
    int i = tab_index >= 0 ? tab_index : w->currentIndex();
    ClipboardBrowser *c = browser(i);

    if ( c != NULL && w->count() > 1 ) {
        int answer = QMessageBox::Yes;
        if (ask) {
            answer = QMessageBox::question(
                        this,
                        tr("Exit?"),
                        tr("Do you want to remove tab <strong>%1</strong>?"
                           ).arg( w->tabText(i).remove('&')),
                        QMessageBox::Yes | QMessageBox::No,
                        QMessageBox::Yes);
        }
        if (answer == QMessageBox::Yes) {
            ConfigurationManager::instance()->removeItems( w->tabText(i) );
            c->purgeItems();
            c->deleteLater();
            w->removeTab(i);
            saveSettings();
        }
    }
}

MainWindow::~MainWindow()
{
    ConfigurationManager::instance(this)->disconnect();
    saveSettings();
    tray->hide();
    delete ui;
}
