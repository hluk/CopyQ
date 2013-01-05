/*
    Copyright (c) 2012, Lukas Holecek <hluk@email.cz>

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
#include "tabwidget.h"
#include "client_server.h"
#include "action.h"

#include <QCloseEvent>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QTimer>
#include <QInputDialog>
#include <QFileDialog>
#include <QDesktopWidget>
#include <QDebug> // TODO: REMOVE!

#ifdef Q_WS_X11
#   include <X11/Xlib.h>
#   include <X11/Xatom.h>
#elif defined Q_OS_WIN
#   include "qt_windows.h"
#endif

namespace {

const QIcon &iconAction() { ICON("action"); }
const QIcon &iconClipboard() { ICON("clipboard"); }
const QIcon &iconCopy() { ICON2("edit-copy", "copy"); }
const QIcon &iconExit() { ICON2("application-exit", "exit"); }
const QIcon &iconHelp() { ICON2("help-about", "help"); }
const QIcon &iconNew() { ICON2("document-new", "new"); }
const QIcon &iconOpen() { ICON2("document-open", "open"); }
const QIcon &iconPaste() { ICON2("edit-paste", "paste"); }
const QIcon &iconPreferences() { ICON2("preferences-other", "preferences"); }
const QIcon &iconReverse() { ICON("reverse"); }
const QIcon &iconSave() { ICON2("document-save", "save"); }
const QIcon &iconSort() { ICON2("view-sort-ascending", "sort"); }
const QIcon &iconTabNew() { ICON("tab_new"); }
const QIcon &iconTabRemove() { ICON("tab_remove"); }
const QIcon &iconTabRename() { ICON("tab_rename"); }
const QIcon &iconTray() { ICON("icon"); }
const QIcon &iconTrayRunning() { ICON("icon-running"); }

QString currentWindowTitle()
{
#ifdef Q_WS_X11
    struct X11Display {
        X11Display() { d = XOpenDisplay(NULL); }
        ~X11Display() { if (d != NULL) XCloseDisplay(d); }
        operator Display*() { return d; }
        bool isOk() { return d != NULL; }
        Display *d;
    } display;

    if (display.isOk()) {
        static Atom atomWindow = XInternAtom(display, "_NET_ACTIVE_WINDOW", true);
        static Atom atomName = XInternAtom(display, "_NET_WM_NAME", false);
        static Atom atomUTF8 = XInternAtom(display, "UTF8_STRING", false);

        Atom type;
        int format;
        unsigned long len;
        unsigned long remain;
        unsigned char *data;
        Window focusedWindow = 0L;

        // current window
        if ( XGetWindowProperty(display, DefaultRootWindow(display.d), atomWindow, 0l, 1l, false,
                                XA_WINDOW, &type, &format, &len, &remain, &data) == Success ) {
            if (type == XA_WINDOW && format == 32 && len == 1)
                focusedWindow = *reinterpret_cast<Window *>(data);
            XFree(data);
        }

        // window title
        if (focusedWindow != 0L) {
            if ( XGetWindowProperty(display, focusedWindow, atomName, 0, (~0L), false, atomUTF8,
                                    &type, &format, &len, &remain, &data) == Success ) {
                QByteArray result(reinterpret_cast<const char *>(data), len);
                XFree(data);
                return QString::fromUtf8(result);
            }
        }
    }
#elif defined Q_OS_WIN
    TCHAR buf[1024];
    GetWindowText(GetForegroundWindow(), buf, 1024);
    return QString::fromUtf16(reinterpret_cast<ushort *>(buf));
#endif // TODO: current window on Mac

    return QString();
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , aboutDialog(NULL)
    , cmdMenu(NULL)
    , itemMenu(NULL)
    , tray(NULL)
    , m_browsemode(false)
    , m_confirmExit(true)
    , m_trayItems(5)
    , m_lastTab(0)
    , m_timerSearch( new QTimer(this) )
    , m_actions()
{
    ui->setupUi(this);

    setWindowIcon(iconTray());

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
             this, SLOT(tabChanged(int)) );
    connect( ui->tabWidget, SIGNAL(tabMoved(int, int)),
             this, SLOT(tabMoved(int, int)) );
    connect( ui->tabWidget, SIGNAL(tabMenuRequested(QPoint, int)),
             this, SLOT(tabMenuRequested(QPoint, int)) );
    connect( ui->tabWidget, SIGNAL(tabCloseRequested(int)),
             this, SLOT(tabCloseRequested(int)) );
    connect( m_timerSearch, SIGNAL(timeout()),
             this, SLOT(onTimerSearch()) );
    connect( ui->searchBar, SIGNAL(textChanged(QString)),
             m_timerSearch, SLOT(start()) );
    connect( this, SIGNAL(editingActive(bool)),
             ui->tabWidget, SLOT(setTabBarDisabled(bool)) );

    ConfigurationManager *cm = ConfigurationManager::instance();
    cm->loadGeometry(this);

    // notify window if configuration changes
    connect( cm, SIGNAL(configurationChanged()),
             this, SLOT(loadSettings()) );

    // browse mode by default
    enterBrowseMode();

    tray->show();

    setAcceptDrops(true);
}

void MainWindow::exit()
{
    int answer = QMessageBox::Yes;
    if ( m_confirmExit ) {
        answer = QMessageBox::question(
                    this,
                    tr("Exit?"),
                    tr("Do you want to <strong>exit</strong> CopyQ?"),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::Yes);
    }

    if (answer == QMessageBox::Yes) {
        close();
        QApplication::exit();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    ConfigurationManager::instance()->saveGeometry(this);

    hide();

    if ( aboutDialog && !aboutDialog->isHidden() ) {
        aboutDialog->close();
    }

    // deselect all
    ClipboardBrowser *c = browser();
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

    connect( this, SIGNAL(editingActive(bool)),
             menubar, SLOT(setDisabled(bool)) );

    // items before separator in tray
    traymenu->addSeparator();

    // File
    menu = menubar->addMenu( tr("&File") );

    // - show/hide
    act = traymenu->addAction( iconTray(), tr("&Show/Hide"),
                           this, SLOT(toggleVisible()) );
    traymenu->setDefaultAction(act);

    // - separator
    menu->addSeparator();

    // - new
    act = traymenu->addAction( iconNew(), tr("&New Item"),
                               this, SLOT(newItem()) );
    menu->addAction( act->icon(), act->text(),
                     this, SLOT(newItem()),
                     QKeySequence::New );

    // - import tab
    menu->addAction( iconOpen(), tr("&Import Tab..."),
                     this, SLOT(loadTab()),
                     QKeySequence(tr("Ctrl+I")) );

    // - export tab
    menu->addAction( iconSave(), tr("&Export Tab..."),
                     this, SLOT(saveTab()),
                     QKeySequence::Save );

    // - action dialog
    act = traymenu->addAction( iconAction(), tr("&Action..."),
                               this, SLOT(openActionDialog()) );
    act->setWhatsThis( tr("Open action dialog") );

    // - separator
    menu->addSeparator();

    // - preferences
    act = traymenu->addAction( iconPreferences(),
                               tr("&Preferences"),
                               this, SLOT(openPreferences()) );
    menu->addAction( act->icon(), act->text(), this, SLOT(openPreferences()),
                     QKeySequence(tr("Ctrl+P")) );

    // - show clipboard content
    act = traymenu->addAction( iconClipboard(),
                               tr("Show &Clipboard Content"),
                               this, SLOT(showClipboardContent()) );
    menu->addAction( act->icon(), act->text(), this, SLOT(showClipboardContent()),
                     QKeySequence(tr("Ctrl+Shift+C")) );

    // - separator
    menu->addSeparator();

    // - exit
    act = menu->addAction( iconExit(), tr("E&xit"),
                           this, SLOT(exit()),
                           QKeySequence(tr("Ctrl+Q")) );

    // Edit
    menu = menubar->addMenu( tr("&Edit") );

    // - sort
    act = menu->addAction( iconSort(),
                           tr("&Sort Selected Items"),
                           this, SLOT(sortSelectedItems()),
                           QKeySequence(tr("Ctrl+Shift+S")) );

    // - reverse order
    act = menu->addAction( iconReverse(),
                           tr("&Reverse Selected Items"),
                           this, SLOT(reverseSelectedItems()),
                           QKeySequence(tr("Ctrl+Shift+R")) );

    // - separator
    menu->addSeparator();

    // - paste items
    act = menu->addAction( iconPaste(), tr("&Paste Items"),
                           this, SLOT(pasteItems()) );
    act->setShortcuts(QKeySequence::Paste);

    // - copy items
    act = menu->addAction( iconCopy(),
                           tr("&Copy Selected Items"),
                           this, SLOT(copyItems()) );
    act->setShortcuts(QKeySequence::Copy);

    // Items
    itemMenu = browser()->contextMenu();
    itemMenu->setTitle( tr("&Item") );
    menubar->addMenu(itemMenu);

    // Tabs
    menu = menubar->addMenu(tr("&Tabs"));

    // add tab
    menu->addAction( iconTabNew(), tr("&New tab"),
                     this, SLOT(newTab()),
                     QKeySequence(tr("Ctrl+T")) );
    menu->addAction( iconTabRename(), tr("&Rename tab"),
                     this, SLOT(renameTab()),
                     QKeySequence(tr("Ctrl+F2")) );
    menu->addAction( iconTabRemove(), tr("&Remove tab"),
                     this, SLOT(removeTab()),
                     QKeySequence(tr("Ctrl+W")) );

    // Commands
    cmdMenu = menubar->addMenu(tr("Co&mmands"));
    cmdMenu->setEnabled(false);
    traymenu->addMenu(cmdMenu);

    // Exit in tray menu
    traymenu->addSeparator();
    traymenu->addAction( iconExit(), tr("E&xit"),
                         this, SLOT(exit()) );

    // Help
    menu = menubar->addMenu( tr("&Help") );
    act = menu->addAction( iconHelp(), tr("&Help"),
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

void MainWindow::elideText(QAction *act)
{
    QFont font = act->font();
    act->setFont(font);

    QFontMetrics fm(font);
    QString text = act->text();
    text = fm.elidedText( text.left(512).simplified(), Qt::ElideRight, 240 );
    act->setText(text);
}

void MainWindow::closeAction(Action *action)
{
    QString msg;

    if ( action->actionFailed() || action->exitStatus() != QProcess::NormalExit )
        msg += tr("Error: %1\n").arg(action->errorString()) + action->errorOutput();
    else if ( action->exitCode() != 0 )
        msg += tr("Exit code: %1\n").arg(action->exitCode()) + action->errorOutput();

    if ( !msg.isEmpty() )
        showMessage( tr("Command \"%1\"").arg(action->command()), msg );

    delete m_actions.take(action);
    action->deleteLater();

    if ( m_actions.isEmpty() ) {
        cmdMenu->setEnabled(false);
        changeTrayIcon( iconTray() );
    }
}

ClipboardBrowser *MainWindow::createTab(const QString &name, bool save)
{
    TabWidget *w = ui->tabWidget;

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
    connect( c, SIGNAL(editingActive(bool)),
             this, SIGNAL(editingActive(bool)) );
    connect( c, SIGNAL(requestActionDialog(const QString&, const Command&)),
             this, SLOT(action(const QString&, const Command&)) );
    connect( c, SIGNAL(requestActionDialog(const QString&)),
             this, SLOT(openActionDialog(const QString&)) );
    connect( c, SIGNAL(requestShow(const ClipboardBrowser*)),
             this, SLOT(showBrowser(const ClipboardBrowser*)) );
    connect( c, SIGNAL(addToTab(const QMimeData*,const QString&)),
             this, SLOT(addToTab(const QMimeData*,const QString&)),
             Qt::DirectConnection );

    w->addTab(c, name);

    if (save)
        saveSettings();

    return c;
}

QStringList MainWindow::tabs() const
{
    QStringList tabs;
    TabWidget *w = ui->tabWidget;

    for( int i = 0; i < w->count(); ++i )
        tabs << browser(i)->getID();

    return tabs;
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

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    QString txt;
    ClipboardBrowser *c = browser();

    if (m_browsemode && ConfigurationManager::instance()->value("vi").toBool()) {
        if (c->handleViKey(event))
            return;
        switch( event->key() ) {
        case Qt::Key_Slash:
            enterBrowseMode(false);
            event->accept();
            return;
        case Qt::Key_H:
            previousTab();
            event->accept();
            return;
        case Qt::Key_L:
            nextTab();
            event->accept();
            return;
        }
    }

    if ( event->modifiers() == Qt::ControlModifier ) {
        switch( event->key() ) {
            case Qt::Key_F:
                enterBrowseMode(false);
                break;
            case Qt::Key_Tab:
                nextTab();
                break;
            case Qt::Key_Backtab:
                previousTab();
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

        case Qt::Key_Left:
            if ( browser()->hasFocus() )
                previousTab();
            break;
        case Qt::Key_Right:
            if ( browser()->hasFocus() )
                nextTab();
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

        case Qt::Key_Tab:
            QMainWindow::keyPressEvent(event);
            break;

        case Qt::Key_Backspace:
            resetStatus();
            c->setCurrent(0);
            break;

        case Qt::Key_Escape:
            if ( ui->searchBar->isHidden() )
                close();
            else
                resetStatus();
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

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    ClipboardBrowser *c = browser();
    addToTab(event->mimeData(), c->getID());
    c->updateClipboard();
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
    ConfigurationManager *cm = ConfigurationManager::instance();

    cm->saveGeometry(this);
    cm->setTabs(tabs());
    cm->saveSettings();
}

void MainWindow::loadSettings()
{
    log( tr("Loading configuration") );

    ConfigurationManager *cm = ConfigurationManager::instance();
    m_confirmExit = cm->value("confirm_exit").toBool();
    m_trayItems = cm->value("tray_items").toInt();

    // always on top window hint
    if (cm->value("always_on_top").toBool()) {
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    } else {
        setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
    }

    // tab bar position
    int tabPosition = cm->value("tab_position").toInt();
    ui->tabWidget->setTabPosition(
          tabPosition == 1 ? QTabWidget::South
        : tabPosition == 2 ? QTabWidget::West
        : tabPosition == 3 ? QTabWidget::East
                           : QTabWidget::North);

    /* are tabs already loaded? */
    bool loaded = ui->tabWidget->count() > 0;
    QStringList tabs = cm->value("tabs").toStringList();
    foreach (const QString &name, tabs) {
        ClipboardBrowser *c;
        c = createTab(name, false);
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

    ConfigurationManager::instance()->loadGeometry(this);

    // if no item is selected then select first
    ClipboardBrowser *c = browser();
    if( c->selectionModel()->selectedIndexes().size() <= 1 &&
            c->currentIndex().row() <= 0 ) {
        c->setCurrent(0);
    }
    c->scrollTo( c->currentIndex() );
    c->setFocus();

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

void MainWindow::showBrowser(const ClipboardBrowser *browser)
{
    TabWidget *tabs = ui->tabWidget;
    int i = 0;
    for( ; i < tabs->count() && this->browser(i) != browser; ++i );
    if ( i < tabs->count() ) {
        showWindow();
        tabs->setCurrentIndex(i);
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

WId MainWindow::showMenu()
{
    QMenu *menu = tray->contextMenu();
    updateTrayMenuItems();

    // open menu unser cursor
    QPoint pos = QCursor::pos();
    QRect screen = QApplication::desktop()->screenGeometry();
    pos.setX(qMax(0, qMin(screen.width() - menu->width(), pos.x())));
    pos.setY(qMax(0, qMin(screen.height() - menu->height(), pos.y())));
    menu->popup(pos);

    // steal focus
    WId wid = menu->winId();
    raiseWindow(wid);
    return wid;
}

void MainWindow::tabChanged(int current)
{
    // update item menu (necessary for keyboard shortcuts to work)
    QMenu *m = itemMenu;
    itemMenu = browser()->contextMenu();
    itemMenu->setTitle( tr("&Item") );
    menuBar()->insertMenu( m->menuAction(), itemMenu );
    menuBar()->removeAction( m->menuAction() );
    browser()->filterItems( ui->searchBar->text() );

    /* Deselect first item in previously selected tab if it is current item and
     * is the only one selected. Next time the tab is selected first item is
     * selected as current.
     */
    if ( m_lastTab >= 0 && m_lastTab < ui->tabWidget->count() ) {
        ClipboardBrowser *c = browser(m_lastTab);
        QModelIndex current = c->currentIndex();
        if ( current.row() == 0 ) {
            QModelIndexList indexes = c->selectionModel()->selectedIndexes();
            if ( indexes.isEmpty() ||
                    (indexes.size() == 1 && indexes.first() == current) ) {
                c->selectionModel()->clear();
                c->setCurrentIndex( QModelIndex() );
            }
        }
    }

    if ( current >= 0 ) {
        ClipboardBrowser *c = browser();
        if( !c->currentIndex().isValid() ) {
            c->setCurrent(0);
        }
    }

    m_lastTab = current;
}

void MainWindow::tabMoved(int, int)
{
    ConfigurationManager *cm = ConfigurationManager::instance();
    cm->setTabs(tabs());
    cm->saveSettings();
}

void MainWindow::tabMenuRequested(const QPoint &pos, int tab)
{
    QMenu *menu = new QMenu(this);

    menu->addAction( iconTabNew(), tr("&New tab"),
                     this, SLOT(newTab()) );

    if (tab < 0) {
        menu->exec(pos);
        return;
    }

    QString name = ui->tabWidget->tabText(tab);
    QAction *renameAct = menu->addAction( iconTabRename(),
                                          tr("&Rename tab \"%1\"").arg(name) );
    QAction *removeAct = menu->addAction( iconTabRemove(),
                                          tr("&Remove tab \"%1\"").arg(name) );

    QAction *act = menu->exec(pos);
    if (act == renameAct)
        renameTab(tab);
    else if (act == removeAct)
        removeTab(true, tab);
}

void MainWindow::tabCloseRequested(int tab)
{
    if (tab > 0)
        removeTab(true, tab);
    else
        newTab();
}

void MainWindow::addToTab(const QMimeData *data, const QString &tabName)
{
    ClipboardBrowser *c;
    TabWidget *tabs = ui->tabWidget;
    int i = 0;

    if ( !tabName.isEmpty() )
        for ( ; i < tabs->count() && tabs->tabText(i) != tabName; ++i );

    if ( i < tabs->count() ) {
        c = browser(i);
    } else if ( !tabName.isEmpty() ) {
        c = createTab(tabName, true);
    } else {
        return;
    }

    ClipboardBrowser::Lock lock(c);
    if ( !c->select(hash(*data, data->formats())) ) {
        QMimeData *data2 = cloneData(*data);
        // force adding item if tab name is specified
        bool force = !tabName.isEmpty();
        // merge data with first item if it is same
        if ( !force && c->length() > 0 ) {
            ClipboardItem *first = c->at(0);
            if ( data2->hasText() && data2->text() == first->text() ) {
                QStringList formats = data2->formats();
                QStringList firstFormats = first->formats();
                foreach (const QString &format, firstFormats) {
                    if ( !formats.contains(format) )
                        data2->setData( format, first->data()->data(format) );
                }
                // remove merged item (if it's not edited)
                if (!c->editing() || c->currentIndex().row() != 0)
                    c->model()->removeRow(0);
            }
        }
        c->add(data2, force, force ? QString() : currentWindowTitle());
    }
}

void MainWindow::nextTab()
{
    ui->tabWidget->nextTab();
}

void MainWindow::previousTab()
{
    ui->tabWidget->previousTab();
}

void MainWindow::addItems(const QStringList &items, const QString &tabName)
{
    ClipboardBrowser *c = tabName.isEmpty() ? browser() :
                                              createTab(tabName, true);
    foreach (const QString &item, items)
        c->add(item, true);
}

void MainWindow::onTimerSearch()
{
    QString txt = ui->searchBar->text();
    enterBrowseMode( txt.isEmpty() );
    browser()->filterItems(txt);
}

void MainWindow::actionStarted(Action *action)
{
    // menu item
    QString text = tr("KILL") + " " + action->command() + " " +
                   action->commandArguments().join(" ");
    QString tooltip = tr("<b>COMMAND:</b>") + '\n' + escapeHtml(text) + '\n' +
                      tr("<b>INPUT:</b>") + '\n' +
                      escapeHtml( QString::fromLocal8Bit(action->input()) );

    QAction *act = m_actions[action] = new QAction(text, this);
    act->setToolTip(tooltip);

    connect( act, SIGNAL(triggered()),
             action, SLOT(terminate()) );

    cmdMenu->addAction(act);
    cmdMenu->setEnabled(true);

    changeTrayIcon( iconTrayRunning() );

    elideText(act);
}

void MainWindow::actionFinished(Action *action)
{
    closeAction(action);
}

void MainWindow::actionError(Action *action)
{
    closeAction(action);
}

void MainWindow::enterSearchMode(const QString &txt)
{
    enterBrowseMode( txt.isEmpty() );
    ui->searchBar->setText( ui->searchBar->text()+txt );
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
    } else {
        // search mode
        l->show();
        b->show();
        l->setEnabled(true);
        l->setFocus(Qt::ShortcutFocusReason);
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

    len = qMin( m_trayItems, c->length() );
    unsigned char hint('0');
    for( i = 0; i < len; ++i ) {
        QString text = c->itemText(i);

        if (hint <= '9') {
            /* FIXME: keypad numbers don't work */
            act = menu->addAction(QString('&') + hint + ". " + text);
            ++hint;
        } else {
            act = menu->addAction(text);
        }

        act->setData( QVariant(i) );

        menu->insertAction(sep, act);

        // activate first clipboard item
        if (i == 0)
            menu->setActiveAction(act);

        elideText(act);
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
    ClipboardDialog *d = new ClipboardDialog(this);
    connect( d, SIGNAL(finished(int)), d, SLOT(deleteLater()) );
    d->show();
}

ActionDialog *MainWindow::createActionDialog()
{
    ActionDialog *actionDialog = new ActionDialog(this);
    actionDialog->setOutputTabs(tabs(), QString());

    connect( actionDialog, SIGNAL(accepted(Action*)),
             this, SLOT(action(Action*)) );
    connect( actionDialog, SIGNAL(finished(int)),
             actionDialog, SLOT(deleteLater()) );

    return actionDialog;
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

WId MainWindow::openActionDialog(const QString &text)
{
    ActionDialog *actionDialog = createActionDialog();
    actionDialog->setInputText(text);
    actionDialog->show();

    // steal focus
    WId wid = actionDialog->winId();
    raiseWindow(wid);
    return wid;
}

void MainWindow::openPreferences()
{
    ConfigurationManager::instance()->exec();
}

ClipboardBrowser *MainWindow::browser(int index) const
{
    QWidget *w = ui->tabWidget->widget(
                index < 0 ? ui->tabWidget->currentIndex() : index );
    return dynamic_cast<ClipboardBrowser*>(w);
}

int MainWindow::tabIndex(const ClipboardBrowser *c) const
{
    /* Iterate because QTabWidget::indexOf takes non-const argument. */
    int i = 0;
    const QWidget *w = dynamic_cast<const QWidget*>(c);
    const QWidget *w2 = ui->tabWidget->widget(i);
    for ( ; w2 != NULL && w != w2; w2 = ui->tabWidget->widget(++i) );
    return w2 == NULL ? -1 : i;
}

ClipboardBrowser *MainWindow::addTab(const QString &name)
{
    TabWidget *w = ui->tabWidget;
    ClipboardBrowser *c = createTab(name, true);
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

void MainWindow::pasteItems()
{
    const QMimeData *data = clipboardData();
    if (data == NULL)
        return;

    ClipboardBrowser *c = browser();
    ClipboardBrowser::Lock lock(c);
    int count = 0;

    // Insert items from clipboard or just clipboard content.
    if ( data->hasFormat("application/x-copyq-item") ) {
        const QByteArray bytes = data->data("application/x-copyq-item");
        QDataStream in(bytes);
        ClipboardItem item;

        while( !in.atEnd() ) {
            in >> item;
            c->add(item, true);
            ++count;
        }
    } else {
        c->add( cloneData(*data), true );
        count = 1;
    }

    // Select new items.
    if (count > 0) {
        QItemSelection sel;
        QModelIndex first = c->index(0);
        QModelIndex last = count > 1 ? c->index(count-1) : first;
        sel.select(first, last);
        c->setCurrentIndex(first);
        c->selectionModel()->select(sel, QItemSelectionModel::ClearAndSelect);
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

    /* Copy items in reverse (items will be pasted correctly). */
    for ( int i = indexes.size()-1; i >= 0; --i ) {
        out << *c->at( indexes.at(i).row() );
    }

    ClipboardItem item;
    QMimeData data;
    if ( indexes.size() == 1 ) {
        int row = indexes.at(0).row();
        item.setData( cloneData(*c->at(row)->data()) );
    } else {
        data.setText( c->selectedText() );
        data.setData("application/x-copyq-item", bytes);
        item.setData( cloneData(data) );
    }

    emit changeClipboard(&item);
}

bool MainWindow::saveTab(const QString &fileName, int tab_index)
{
    QFile file(fileName);
    if ( !file.open(QIODevice::WriteOnly | QIODevice::Truncate) )
        return false;

    QDataStream out(&file);

    int i = tab_index >= 0 ? tab_index : ui->tabWidget->currentIndex();
    ClipboardBrowser *c = browser(i);
    ClipboardModel *model = static_cast<ClipboardModel *>( c->model() );

    out << QByteArray("CopyQ v1") << c->getID() << *model;

    file.close();

    return true;
}

bool MainWindow::saveTab(int tab_index)
{
    QString fileName = QFileDialog::getSaveFileName( this, QString(), QString(),
                                                     tr("CopyQ Items (*.cpq)") );
    if ( fileName.isNull() )
        return false;

    if ( !saveTab(fileName, tab_index) ) {
        QMessageBox::critical( this, tr("CopyQ Error Saving File"),
                               tr("Cannot save file \"%1\"!").arg(fileName) );
        return false;
    }

    return true;
}

bool MainWindow::loadTab(const QString &fileName)
{
    QFile file(fileName);
    if ( !file.open(QIODevice::ReadOnly) )
        return false;

    QDataStream in(&file);

    QByteArray header;
    QString tabName;
    in >> header >> tabName;
    if ( !header.startsWith("CopyQ v1") || tabName.isEmpty() ) {
        file.close();
        return false;
    }

    // Find unique tab name.
    QString baseTabName = tabName;
    QStringList existingTabs = tabs();
    int i = 0;
    while ( existingTabs.contains(tabName) ) {
        log(tabName);
        tabName = baseTabName + " (" + QString::number(++i) + ')';
    }

    ClipboardBrowser *c = createTab(tabName, true);
    ClipboardModel *model = static_cast<ClipboardModel *>( c->model() );

    in >> *model;

    file.close();

    return true;
}

bool MainWindow::loadTab()
{
    QString fileName = QFileDialog::getOpenFileName( this, QString(), QString(),
                                                     tr("CopyQ Items (*.cpq)") );
    if ( fileName.isNull() )
        return false;

    if ( !loadTab(fileName) ) {
        QMessageBox::critical( this, tr("CopyQ Error Opening File"),
                               tr("Cannot open file \"%1\"!").arg(fileName) );
        return false;
    }

    QTabWidget *w = ui->tabWidget;
    w->setCurrentIndex( w->count()-1 );

    return true;
}

void MainWindow::sortSelectedItems()
{
    ClipboardBrowser *c = browser();
    c->sortItems( c->selectionModel()->selectedRows() );
}

void MainWindow::reverseSelectedItems()
{
    ClipboardBrowser *c = browser();
    c->reverseItems( c->selectionModel()->selectedRows() );
}

void MainWindow::action(Action *action)
{
    connect( action, SIGNAL(newItems(QStringList, QString)),
             this, SLOT(addItems(QStringList, QString)) );
    connect( action, SIGNAL(actionStarted(Action*)),
             this, SLOT(actionStarted(Action*)) );
    connect( action, SIGNAL(actionFinished(Action*)),
             this, SLOT(actionFinished(Action*)) );
    connect( action, SIGNAL(actionError(Action*)),
             this, SLOT(actionError(Action*)) );

    log( tr("Executing: %1").arg(action->command()) );
    log( tr("Arguments: %1").arg(action->commandArguments().join(", ")) );
    action->start();
}

void MainWindow::action(const QString &text, const Command &cmd)
{
    ActionDialog *actionDialog = createActionDialog();
    QString outputTab;

    actionDialog->setInputText(text);
    actionDialog->setCommand(cmd.cmd);
    actionDialog->setSeparator(cmd.sep);
    actionDialog->setInput(cmd.input);
    actionDialog->setOutput(cmd.output);
    actionDialog->setRegExp(cmd.re);
    outputTab = cmd.outputTab;

    if (cmd.wait) {
        // Insert tab labels to action dialog's combo box.
        QStringList tabs;
        TabWidget *w = ui->tabWidget;
        for( int i = 0; i < w->count(); ++i )
            tabs << w->tabText(i);
        if ( outputTab.isEmpty() && w->currentIndex() > 0 )
            outputTab = w->tabText( w->currentIndex() );
        actionDialog->setOutputTabs(tabs, outputTab);

        // Show action dialog.
        actionDialog->show();
    } else {
        // Create action without showing action dialog.
        actionDialog->setOutputTabs(QStringList(), outputTab);
        actionDialog->createAction();
        actionDialog->deleteLater();
    }
}

void MainWindow::newTab()
{
    TabDialog *d = new TabDialog(TabDialog::TabNew, this);
    d->setTabs(tabs());

    connect( d, SIGNAL(accepted(QString, int)),
             this, SLOT(addTab(QString)) );
    connect( d, SIGNAL(finished(int)),
             d, SLOT(deleteLater()) );

    d->open();
}

void MainWindow::renameTab(int tab)
{
    TabDialog *d = new TabDialog(TabDialog::TabRename, this);
    int i = tab >= 0 ? tab : ui->tabWidget->currentIndex();
    d->setTabIndex(i);
    d->setTabs(tabs());
    d->setTabName(browser(i)->getID());

    connect( d, SIGNAL(accepted(QString, int)),
             this, SLOT(renameTab(QString, int)) );
    connect( d, SIGNAL(finished(int)),
             d, SLOT(deleteLater()) );

    d->open();
}

void MainWindow::renameTab(const QString &name, int tabIndex)
{
    if ( name.isEmpty() || tabs().contains(name) )
        return;

    TabWidget *w = ui->tabWidget;
    ClipboardBrowser *c = browser(tabIndex);
    QString oldName = c->getID();

    c->setID(name);
    c->saveItems();
    w->setTabText(tabIndex, name);

    ConfigurationManager *cm = ConfigurationManager::instance();
    cm->removeItems(oldName);
    cm->setTabs(tabs());
}

void MainWindow::removeTab(bool ask, int tab_index)
{
    TabWidget *w = ui->tabWidget;
    int i = tab_index >= 0 ? tab_index : w->currentIndex();
    ClipboardBrowser *c = browser(i);

    if ( c != NULL && w->count() > 1 ) {
        int answer = QMessageBox::Yes;
        if (ask) {
            answer = QMessageBox::question(
                        this,
                        tr("Remove Tab?"),
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
    ConfigurationManager::instance()->disconnect();
    saveSettings();
    tray->hide();
    delete ui;
}
