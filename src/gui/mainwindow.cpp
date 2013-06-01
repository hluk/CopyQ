/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#include "common/action.h"
#include "common/client_server.h"
#include "common/command.h"
#include "common/contenttype.h"
#include "gui/aboutdialog.h"
#include "gui/actiondialog.h"
#include "gui/clipboardbrowser.h"
#include "gui/clipboarddialog.h"
#include "gui/configurationmanager.h"
#include "gui/iconfactory.h"
#include "gui/tabdialog.h"
#include "gui/tabwidget.h"
#include "gui/traymenu.h"
#include "item/clipboarditem.h"
#include "item/clipboardmodel.h"
#include "platform/platformnativeinterface.h"

#include <QAction>
#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QTimer>

#ifdef COPYQ_ICON_PREFIX
#   define RETURN_ICON_FROM_PREFIX(suffix, fallback) do { \
        const QString fileName(COPYQ_ICON_PREFIX suffix); \
        return QFile::exists(fileName) ? QIcon(fileName) : fallback; \
    } while(false)
#else
#   define RETURN_ICON_FROM_PREFIX(suffix, fallback) \
        return fallback
#endif

namespace {

const QIcon iconAction() { return getIcon("action", IconCog); }
const QIcon iconClipboard() { return getIcon("clipboard", IconPaste); }
const QIcon iconCopy() { return getIcon("edit-copy", IconCopy); }
const QIcon iconExit() { return getIcon("application-exit", IconOff); }
const QIcon iconHelp() { return getIcon("help-about", IconQuestionSign); }
const QIcon iconNew() { return getIcon("document-new", IconFile); }
const QIcon iconOpen() { return getIcon("document-open", IconFolderOpen); }
const QIcon iconPaste() { return getIcon("edit-paste", IconPaste); }
const QIcon iconPreferences() { return getIcon("preferences-other", IconWrench); }
const QIcon iconReverse() { return getIcon("view-sort-descending", IconSortUp); }
const QIcon iconSave() { return getIcon("document-save", IconSave); }
const QIcon iconSort() { return getIcon("view-sort-ascending", IconSortDown); }
const QIcon &iconTabNew() { return getIconFromResources("tab_new"); }
const QIcon &iconTabRemove() { return getIconFromResources("tab_remove"); }
const QIcon &iconTabRename() { return getIconFromResources("tab_rename"); }
const QIcon iconTray(bool disabled) {
    if (disabled)
        RETURN_ICON_FROM_PREFIX( "-disabled.svg", getIconFromResources("icon-disabled") );
    else
        RETURN_ICON_FROM_PREFIX( "-normal.svg", getIconFromResources("icon") );
}
const QIcon iconTrayRunning(bool disabled) {
    if (disabled)
        RETURN_ICON_FROM_PREFIX( "-disabled-busy.svg", getIconFromResources("icon-disabled-running") );
    else
        RETURN_ICON_FROM_PREFIX( "-busy.svg", getIconFromResources("icon-running") );
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , aboutDialog(NULL)
    , cmdMenu(NULL)
    , itemMenu(NULL)
    , trayMenu( new TrayMenu(this) )
    , tray( new QSystemTrayIcon(this) )
    , m_browsemode(false)
    , m_confirmExit(true)
    , m_trayCommands(false)
    , m_trayCurrentTab(false)
    , m_trayItems(5)
    , m_trayImages(true)
    , m_itemPopupInterval(0)
    , m_lastTab(0)
    , m_timerSearch( new QTimer(this) )
    , m_transparency(0)
    , m_transparencyFocused(0)
    , m_hideTabs(false)
    , m_hideMenuBar(false)
    , m_activateCloses(true)
    , m_activateFocuses(false)
    , m_activatePastes(false)
    , m_monitoringDisabled(false)
    , m_actionToggleMonitoring()
    , m_actionMonitoringDisabled()
    , m_actions()
    , m_sharedData(new ClipboardBrowserShared)
    , m_trayItemPaste(true)
    , m_pasteWindow()
    , m_lastWindow()
    , m_timerUpdateFocusWindows( new QTimer(this) )
{
    ui->setupUi(this);

    tray->setContextMenu(trayMenu);

    updateIcon();

    // signals & slots
    connect( tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
             this, SLOT(trayActivated(QSystemTrayIcon::ActivationReason)) );
    connect( trayMenu, SIGNAL(aboutToShow()),
             this, SLOT(updateTrayMenuItems()) );
    connect( trayMenu, SIGNAL(clipboardItemActionTriggered(uint)),
             this, SLOT(onTrayActionTriggered(uint)) );
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
    connect( m_timerUpdateFocusWindows, SIGNAL(timeout()),
             this, SLOT(updateFocusWindows()) );
    connect( this, SIGNAL(editingActive(bool)),
             ui->tabWidget, SLOT(setTabBarDisabled(bool)) );
    connect( this, SIGNAL(changeClipboard(const ClipboardItem*)),
             this, SLOT(clipboardChanged(const ClipboardItem*)) );

    // settings
    loadSettings();

    if ( ui->tabWidget->count() == 0 )
        addTab( tr("&clipboard") );

    ui->tabWidget->setCurrentIndex(0);

    // search timer
    m_timerSearch->setSingleShot(true);
    m_timerSearch->setInterval(200);

    m_timerUpdateFocusWindows->setSingleShot(true);
    m_timerUpdateFocusWindows->setInterval(50);

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

    event->ignore();
}

void MainWindow::createMenu()
{
    QMenuBar *menubar = menuBar();
    QMenu *menu;
    QAction *act;

    itemMenu = NULL;
    menubar->clear();
    trayMenu->clear();

    connect( this, SIGNAL(editingActive(bool)),
             menubar, SLOT(setDisabled(bool)), Qt::UniqueConnection );

    // File
    menu = menubar->addMenu( tr("&File") );

    // - show/hide
    act = trayMenu->addAction( iconTray(false), tr("&Show/Hide"),
                           this, SLOT(toggleVisible()) );
    trayMenu->setDefaultAction(act);

    // - separator
    menu->addSeparator();

    // - new
    act = trayMenu->addAction( iconNew(), tr("&New Item"),
                               this, SLOT(editNewItem()) );
    menu->addAction( act->icon(), act->text(),
                     this, SLOT(editNewItem()),
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
    act = trayMenu->addAction( iconAction(), tr("&Action..."),
                               this, SLOT(openActionDialog()) );
    act->setWhatsThis( tr("Open action dialog") );

    // - separator
    menu->addSeparator();

    // - preferences
    act = trayMenu->addAction( iconPreferences(),
                               tr("&Preferences"),
                               this, SLOT(openPreferences()) );
    menu->addAction( act->icon(), act->text(), this, SLOT(openPreferences()),
                     QKeySequence(tr("Ctrl+P")) );

    // - show clipboard content
    act = trayMenu->addAction( iconClipboard(),
                               tr("Show &Clipboard Content"),
                               this, SLOT(showClipboardContent()) );
    menu->addAction( act->icon(), act->text(), this, SLOT(showClipboardContent()),
                     QKeySequence(tr("Ctrl+Shift+C")) );

    // - enable/disable
    m_actionToggleMonitoring = trayMenu->addAction( "", this, SLOT(toggleMonitoring()) );
    m_actionMonitoringDisabled = menu->addAction( QIcon(), "", this, SLOT(toggleMonitoring()),
                                                  QKeySequence(tr("Ctrl+Shift+X")) );
    updateMonitoringActions();

    // - separator
    menu->addSeparator();

    // - exit
    menu->addAction( iconExit(), tr("E&xit"),
                           this, SLOT(exit()),
                           QKeySequence(tr("Ctrl+Q")) );

    // Edit
    menu = menubar->addMenu( tr("&Edit") );

    // - sort
    menu->addAction( iconSort(),
                     tr("&Sort Selected Items"),
                     this, SLOT(sortSelectedItems()),
                     QKeySequence(tr("Ctrl+Shift+S")) );

    // - reverse order
    menu->addAction( iconReverse(),
                     tr("&Reverse Selected Items"),
                     this, SLOT(reverseSelectedItems()),
                     QKeySequence(tr("Ctrl+Shift+R")) );

    // - separator
    menu->addSeparator();

    // - paste items
    menu->addAction( iconPaste(), tr("&Paste Items"),
                     this, SLOT(pasteItems()),
                     QKeySequence::Paste );

    // - copy items
    menu->addAction( iconCopy(),
                     tr("&Copy Selected Items"),
                     this, SLOT(copyItems()),
                     QKeySequence::Copy );

    // Items
    ClipboardBrowser *c = browser();
    if (c != NULL) {
        itemMenu = c->contextMenu();
        menubar->addMenu(itemMenu);
    } else {
        itemMenu = menubar->addMenu( QString() );
    }
    itemMenu->setTitle( tr("&Item") );

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
    trayMenu->addMenu(cmdMenu);

    // Exit in tray menu
    trayMenu->addSeparator();
    trayMenu->addAction( iconExit(), tr("E&xit"),
                         this, SLOT(exit()) );

    // Help
    menu = menubar->addMenu( tr("&Help") );
    menu->addAction( iconHelp(), tr("&Help"),
                     this, SLOT(openAboutDialog()),
                     QKeySequence::HelpContents );
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
        updateIcon();
    }
}

void MainWindow::updateIcon()
{
    QIcon icon = iconTray(m_monitoringDisabled);
    setWindowIcon(icon);

    if ( !m_actions.isEmpty() )
        icon = iconTrayRunning(m_monitoringDisabled);
    tray->setIcon(icon);
}

void MainWindow::updateWindowTransparency(bool mouseOver)
{
    int opacity = 100 - (mouseOver || isActiveWindow() ? m_transparencyFocused : m_transparency);
    setWindowOpacity(opacity / 100.0);
}

void MainWindow::updateMonitoringActions()
{
    const QString text = m_monitoringDisabled ? tr("&Enable Clipboard Storing")
                                              : tr("&Disable Clipboard Storing");

    QIcon icon = iconTray(!m_monitoringDisabled);

    if (!m_actionToggleMonitoring.isNull()) {
        m_actionToggleMonitoring->setText(text);
        m_actionToggleMonitoring->setIcon(icon);
    }

    if (!m_actionMonitoringDisabled.isNull()) {
        m_actionMonitoringDisabled->setText(text);
        m_actionMonitoringDisabled->setIcon(icon);
    }
}

ClipboardBrowser *MainWindow::findBrowser(const QModelIndex &index)
{
    if (!index.isValid())
        return NULL;

    // Find browser.
    TabWidget *tabs = ui->tabWidget;
    int i = 0;
    for ( ; i < tabs->count(); ++i ) {
        ClipboardBrowser *c = browser(i);
        if (c->model() == index.model())
            return c;
    }

    return NULL;
}

ClipboardBrowser *MainWindow::getBrowser(int index) const
{
    QWidget *w = ui->tabWidget->widget(
                index < 0 ? ui->tabWidget->currentIndex() : index );
    return qobject_cast<ClipboardBrowser*>(w);
}

bool MainWindow::isForeignWindow(WId wid)
{
    return wid != WId() && winId() != wid && find(wid) == NULL;
}

void MainWindow::delayedUpdateFocusWindows()
{
    if (m_activateFocuses || m_activatePastes)
        m_timerUpdateFocusWindows->start();
}

void MainWindow::setHideTabs(bool hide)
{
    ui->tabWidget->setTabBarHidden(hide);
}

void MainWindow::setHideMenuBar(bool hide)
{
    if (m_hideMenuBar) {
        const QColor color = palette().color(QPalette::Highlight);
        ui->widgetShowMenuBar->setStyleSheet( QString("*{background-color:%1}").arg(color.name()) );
        ui->widgetShowMenuBar->installEventFilter(this);
        ui->widgetShowMenuBar->show();
    } else {
        ui->widgetShowMenuBar->removeEventFilter(this);
        ui->widgetShowMenuBar->hide();
    }

    // Hiding menu bar completely disables shortcuts for child QAction.
    menuBar()->setStyleSheet(hide ? "QMenuBar{height:0}" : "");
}

ClipboardBrowser *MainWindow::findTab(const QString &name)
{
    int i = findTabIndex(name);
    return i != -1 ? browser(i) : NULL;
}

int MainWindow::findTabIndex(const QString &name)
{
    TabWidget *w = ui->tabWidget;

    for( int i = 0; i < w->count(); ++i )
        if ( name == w->tabText(i) )
            return i;

    return -1;
}

ClipboardBrowser *MainWindow::createTab(const QString &name, bool save)
{
    /* check name */
    int i = findTabIndex(name);
    if (i != -1)
        return getBrowser(i);

    ClipboardBrowser *c = new ClipboardBrowser(this, m_sharedData);
    c->setID(name);
    c->loadSettings();
    c->setAutoUpdate(true);

    // Preload items only in the first tab.
    if ( ui->tabWidget->count() == 0 )
        c->loadItems();

    connect( c, SIGNAL(changeClipboard(const ClipboardItem*)),
             this, SLOT(onChangeClipboardRequest(const ClipboardItem*)) );
    connect( c, SIGNAL(editingActive(bool)),
             this, SIGNAL(editingActive(bool)) );
    connect( c, SIGNAL(requestActionDialog(const QMimeData&, const Command&)),
             this, SLOT(action(const QMimeData&, const Command&)) );
    connect( c, SIGNAL(requestActionDialog(const QMimeData&, const Command&, const QModelIndex&)),
             this, SLOT(action(const QMimeData&, const Command&, const QModelIndex&)) );
    connect( c, SIGNAL(requestActionDialog(const QMimeData&)),
             this, SLOT(openActionDialog(const QMimeData&)) );
    connect( c, SIGNAL(requestShow(const ClipboardBrowser*)),
             this, SLOT(showBrowser(const ClipboardBrowser*)) );
    connect( c, SIGNAL(requestHide()),
             this, SLOT(close()) );
    connect( c, SIGNAL(doubleClicked(QModelIndex)),
             this, SLOT(activateCurrentItem()) );
    connect( c, SIGNAL(addToTab(const QMimeData*,const QString&)),
             this, SLOT(addToTab(const QMimeData*,const QString&)),
             Qt::DirectConnection );

    ui->tabWidget->addTab(c, name);

    if (save)
        saveSettings();

    return c;
}

QStringList MainWindow::tabs() const
{
    QStringList tabs;
    TabWidget *w = ui->tabWidget;

    for( int i = 0; i < w->count(); ++i )
        tabs << getBrowser(i)->getID();

    return tabs;
}

bool MainWindow::isTrayMenuVisible() const
{
    return trayMenu->isVisible();
}

WId MainWindow::mainWinId() const
{
    return winId();
}

WId MainWindow::trayMenuWinId() const
{
    return trayMenu->winId();
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

    if (event->key() == Qt::Key_Alt) {
        if (m_hideTabs)
            setHideTabs(false);
        if (m_hideMenuBar)
            setHideMenuBar(false);
    }

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
            activateCurrentItem();
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
                if ( !txt.isEmpty() && txt[0].isPrint() )
                    enterSearchMode(txt);
            }
            break;
    }
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Alt) {
        if (m_hideTabs)
            setHideTabs(true);
        if (m_hideMenuBar) {
            setHideMenuBar(true);
            enterBrowseMode(m_browsemode);
        }
    }
    QMainWindow::keyReleaseEvent(event);
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

bool MainWindow::event(QEvent *event)
{
    if (event->type() == QEvent::Enter) {
        updateFocusWindows();
        updateWindowTransparency(true);
    } else if (event->type() == QEvent::Leave) {
        updateWindowTransparency(false);

        setHideTabs(m_hideTabs);
        setHideMenuBar(m_hideMenuBar);
    } else if (event->type() == QEvent::WindowActivate) {
        updateWindowTransparency();

        // Update highligh color of show/hide widget for menu bar.
        setHideMenuBar(m_hideMenuBar);
    } else if (event->type() == QEvent::WindowDeactivate) {
        m_lastWindow = WId();
        m_pasteWindow = WId();
        updateWindowTransparency();

        setHideTabs(m_hideTabs);
        setHideMenuBar(m_hideMenuBar);
    }
    return QMainWindow::event(event);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->widgetShowMenuBar && event->type() == QEvent::Enter)
        setHideMenuBar(false);

    return false;
}

#if QT_VERSION < 0x050000
#   ifdef COPYQ_WS_X11
bool MainWindow::x11Event(XEvent *event)
{
    delayedUpdateFocusWindows();
    return QMainWindow::x11Event(event);
}
#   elif defined(Q_OS_WIN)
bool MainWindow::winEvent(MSG *message, long *result)
{
    delayedUpdateFocusWindows();
    return QMainWindow::winEvent(message, result);
}
#   elif defined(Q_OS_MAC)
bool MainWindow::macEvent(EventHandlerCallRef caller, EventRef event)
{
    delayedUpdateFocusWindows();
    return QMainWindow::macEvent(caller, event);
}
#   endif
#else
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
    delayedUpdateFocusWindows();
    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif

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

    // update menu items and icons
    createMenu();

    // always on top window hint
    if (cm->value("always_on_top").toBool()) {
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    } else {
        setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
    }

    m_transparency = qMax( 0, qMin(100, cm->value("transparency").toInt()) );
    m_transparencyFocused = qMax( 0, qMin(100, cm->value("transparency_focused").toInt()) );
    updateWindowTransparency();

    m_hideTabs = cm->value("hide_tabs").toBool();
    setHideTabs(m_hideTabs);
    m_hideMenuBar = cm->value("hide_menu_bar").toBool();
    setHideMenuBar(m_hideMenuBar);

    // tab bar position
    int tabPosition = cm->value("tab_position").toInt();
    ui->tabWidget->setTabPosition(
          tabPosition == 1 ? QTabWidget::South
        : tabPosition == 2 ? QTabWidget::West
        : tabPosition == 3 ? QTabWidget::East
                           : QTabWidget::North);

    // shared data for browsers
    m_sharedData->loadFromConfiguration();

    /* are tabs already loaded? */
    QStringList tabs = cm->value("tabs").toStringList();
    foreach (const QString &name, tabs) {
        ClipboardBrowser *c;
        c = createTab(name, false);
        c->loadSettings();
    }
    cm->setTabs(tabs);

    m_activateCloses = cm->value("activate_closes").toBool();
    m_activateFocuses = cm->value("activate_focuses").toBool();
    m_activatePastes = cm->value("activate_pastes").toBool();

    m_trayItems = cm->value("tray_items").toInt();
    m_trayItemPaste = cm->value("tray_item_paste").toBool();
    m_trayCommands = cm->value("tray_commands").toBool();
    m_trayCurrentTab = cm->value("tray_tab_is_current").toBool();
    m_trayTabName = cm->value("tray_tab").toString();
    m_trayImages = cm->value("tray_images").toBool();
    m_itemPopupInterval = cm->value("item_popup_interval").toBool();

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

    updateFocusWindows();

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
    QApplication::setActiveWindow(this);

    createPlatformNativeInterface()->raiseWindow(winId());
}

bool MainWindow::toggleVisible()
{
    if ( isVisible() ) {
        close();
        return false;
    }

    showWindow();
    return true;
}

void MainWindow::showBrowser(const ClipboardBrowser *browser)
{
    int i = 0;
    for( ; i < ui->tabWidget->count() && this->browser(i) != browser; ++i ) {}
    showBrowser(i);
}

void MainWindow::showBrowser(int index)
{
    if ( index > 0 && index < ui->tabWidget->count() ) {
        showWindow();
        ui->tabWidget->setCurrentIndex(index);
    }
}

void MainWindow::onTrayActionTriggered(uint clipboardItemHash)
{
    ClipboardBrowser *c = getTabForTrayMenu();
    if (c->select(clipboardItemHash) && m_trayItemPaste && isForeignWindow(m_pasteWindow)) {
        QApplication::processEvents();
        createPlatformNativeInterface()->pasteToWindow(m_pasteWindow);
    }
}

void MainWindow::trayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if ( reason == QSystemTrayIcon::MiddleClick ) {
        toggleMenu();
    } else if ( reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick ) {
        toggleVisible();
    }
}

bool MainWindow::toggleMenu()
{
    trayMenu->toggle();
    return trayMenu->isVisible();
}

void MainWindow::tabChanged(int current)
{
    // update item menu (necessary for keyboard shortcuts to work)
    QMenu *m = browser()->contextMenu();
    m->setTitle( itemMenu->title() );
    menuBar()->insertMenu( itemMenu->menuAction(), m );
    menuBar()->removeAction( itemMenu->menuAction() );
    itemMenu = m;
    setHideMenuBar(m_hideMenuBar);

    browser()->filterItems( ui->searchBar->text() );

    if ( current >= 0 ) {
        ClipboardBrowser *c = browser();
        if( !c->currentIndex().isValid() && isVisible() ) {
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

void MainWindow::addToTab(const QMimeData *data, const QString &tabName, bool moveExistingToTop)
{
    if (m_monitoringDisabled)
        return;

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
    if ( !c->select(hash(*data, data->formats()), moveExistingToTop) ) {
        QMimeData *data2 = cloneData(*data);
        // force adding item if tab name is specified
        bool force = !tabName.isEmpty();
        // merge data with first item if it is same
        if ( !force && c->length() > 0 && data2->hasText() ) {
            ClipboardItem *first = c->at(0);
            const QString newText = data2->text();
            const QString firstItemText = first->text();
            if ( newText == firstItemText || (
                     data2->data(mimeWindowTitle) == first->data()->data(mimeWindowTitle)
                     && (newText.startsWith(firstItemText) || newText.endsWith(firstItemText))) )
            {
                force = true;
                QStringList formats = data2->formats();
                const QMimeData *firstData = first->data();
                foreach (const QString &format, firstData->formats()) {
                    if ( !formats.contains(format) )
                        data2->setData( format, firstData->data(format) );
                }
                // remove merged item (if it's not edited)
                if (!c->editing() || c->currentIndex().row() != 0)
                    c->model()->removeRow(0);
            }
        }
        c->add(data2, force);
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

void MainWindow::clipboardChanged(const ClipboardItem *item)
{
    QString text;
    const QStringList formats = item->data(contentType::formats).toStringList();
    bool hasText = formats.indexOf("text/plain") != -1;

    if (hasText) {
        text = item->text();
    } else if ( formats.indexOf(QRegExp("^image/.*")) != -1 ) {
        text = tr("<IMAGE>");
    } else if ( formats.indexOf(QString("text/uri-list")) != -1 ) {
        text = tr("<FILES>");
    } else {
        text = tr("<DATA>");
    }

    QString format(hasText ? "\"%1\"" : "%1");
    tray->setToolTip( tr("Clipboard:\n%1").arg( format.arg(elideText(text, 256))) );
    setWindowTitle( tr("%1 - CopyQ").arg( format.arg(elideText(text, 30))) );
}

void MainWindow::setClipboard(const ClipboardItem *item)
{
    if (m_itemPopupInterval > 0 && !isVisible()) {
        showMessage( tr("Clipboard"), elideText(item->text(), 256), QSystemTrayIcon::Information,
                     m_itemPopupInterval * 1000 );
    }
    emit changeClipboard(item);
}

void MainWindow::activateCurrentItem()
{
    // Copy current item or selection to clipboard.
    ClipboardBrowser *c = browser();
    if ( c->selectionModel()->selectedIndexes().count() > 1 ) {
        c->add( c->selectedText() );
        c->setCurrent(0);
    }
    c->moveToClipboard( c->currentIndex() );

    resetStatus();

    // Perform custom actions on item activation.
    WId lastWindow = m_lastWindow;
    WId pasteWindow = m_pasteWindow;
    if (m_activateCloses)
        close();
    if (m_activateFocuses || m_activatePastes) {
        PlatformPtr platform = createPlatformNativeInterface();
        if (m_activateFocuses && isForeignWindow(lastWindow))
            platform->raiseWindow(lastWindow);
        if (m_activatePastes && isForeignWindow(pasteWindow)) {
            QApplication::processEvents();
            platform->pasteToWindow(pasteWindow);
        }
    }
}

void MainWindow::disableMonitoring(bool disable)
{
    if (m_monitoringDisabled == disable)
        return;
    m_monitoringDisabled = disable;

    updateMonitoringActions();
    updateIcon();
}

void MainWindow::toggleMonitoring()
{
    disableMonitoring(!m_monitoringDisabled);
}

QByteArray MainWindow::getClipboardData(const QString &mime, QClipboard::Mode mode)
{
    const QMimeData *data = ::clipboardData(mode);
    if (data == NULL)
        return QByteArray();

    return mime == "?" ? data->formats().join("\n").toUtf8() + '\n' : data->data(mime);
}

ClipboardBrowser *MainWindow::getTabForTrayMenu()
{
    return m_trayCurrentTab ? browser()
                            : m_trayTabName.isEmpty() ? browser(0) : findTab(m_trayTabName);
}

void MainWindow::addItems(const QStringList &items, const QString &tabName)
{
    ClipboardBrowser *c = tabName.isEmpty() ? browser() : createTab(tabName, true);
    foreach (const QString &item, items)
        c->add(item, true);
}

void MainWindow::addItems(const QStringList &items, const QModelIndex &index)
{
    ClipboardBrowser *c = findBrowser(index);
    if (c == NULL)
        return;

    QMimeData *newData = new QMimeData();
    newData->setData(QString("text/plain"), items.join(QString()).toLocal8Bit());
    c->setItemData(index, newData);
}

void MainWindow::addItem(const QByteArray &data, const QString &format, const QString &tabName)
{
    ClipboardBrowser *c = tabName.isEmpty() ? browser() : createTab(tabName, true);
    QMimeData *newData = new QMimeData();
    newData->setData(format, data);
    c->add(newData, true);
}

void MainWindow::addItem(const QByteArray &data, const QString &format, const QModelIndex &index)
{
    ClipboardBrowser *c = findBrowser(index);
    if (c == NULL)
        return;

    QMimeData *newData = new QMimeData();
    newData->setData(format, data);
    c->setItemData(index, newData);
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
    QString text = tr("KILL") + " " + action->command();
    QString tooltip = tr("<b>COMMAND:</b>") + '\n' + escapeHtml(text) + '\n' +
                      tr("<b>INPUT:</b>") + '\n' +
                      escapeHtml( QString::fromLocal8Bit(action->input()) );

    QAction *act = m_actions[action] = new QAction(text, this);
    act->setToolTip(tooltip);

    connect( act, SIGNAL(triggered()),
             action, SLOT(terminate()) );

    cmdMenu->addAction(act);
    cmdMenu->setEnabled(true);

    updateIcon();

    elideText(act, true);
}

void MainWindow::actionFinished(Action *action)
{
    closeAction(action);
}

void MainWindow::actionError(Action *action)
{
    closeAction(action);
}

void MainWindow::onChangeClipboardRequest(const ClipboardItem *item)
{
    setClipboard(item);
}

void MainWindow::updateFocusWindows()
{
    if ( isActiveWindow() || (!m_activateFocuses && !m_activatePastes) )
        return;

    PlatformPtr platform = createPlatformNativeInterface();
    if (m_activatePastes) {
        WId pasteWindow = platform->getPasteWindow();
        if ( isForeignWindow(pasteWindow) )
            m_pasteWindow = pasteWindow;
    }
    if (m_activateFocuses) {
        WId lastWindow = platform->getCurrentWindow();
        if ( isForeignWindow(lastWindow) )
            m_lastWindow = lastWindow;
    }
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

void MainWindow::updateTrayMenuItems()
{
    PlatformPtr platform = createPlatformNativeInterface();
    m_pasteWindow = platform->getPasteWindow();

    ClipboardBrowser *c = getTabForTrayMenu();

    trayMenu->clearClipboardItemActions();
    trayMenu->clearCustomActions();

    // Add items.
    const int len = (c != NULL) ? qMin( m_trayItems, c->length() ) : 0;
    const int current = c->currentIndex().row();
    for ( int i = 0; i < len; ++i ) {
        const ClipboardItem *item = c->at(i);
        if (item != NULL)
            trayMenu->addClipboardItemAction(*item, m_trayImages, i == current);
    }

    // Add commands.
    if (m_trayCommands) {
        if (c == NULL)
            c = browser(0);
        const QMimeData *data = clipboardData();

        // Show clipboard content as disabled item.
        QString text = data != NULL ? data->text() : c->selectedText();
        QAction *act = trayMenu->addAction(text);
        act->setDisabled(true);
        trayMenu->addCustomAction(act);
        elideText(act, true);

        int i = trayMenu->actions().size();
        c->addCommandsToMenu(trayMenu, text, data);
        QList<QAction *> actions = trayMenu->actions();
        for ( ; i < actions.size(); ++i )
            trayMenu->addCustomAction(actions[i]);
    }

    if (trayMenu->activeAction() == NULL)
        trayMenu->setActiveFirstEnabledAction();
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
    ClipboardDialog *d = new ClipboardDialog(NULL, this);
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
    if (row >= 0) {
        const QMimeData *data = c->itemData(row);
        if (data != NULL)
            openActionDialog(*data);
    } else if ( hasFocus() ) {
        QModelIndexList selected = c->selectionModel()->selectedRows();
        if (selected.size() == 1) {
            const QMimeData *data = c->itemData(selected.first().row());
            if (data != NULL)
            openActionDialog(*data);
        } else {
            QMimeData data;
            data.setText(c->selectedText());
            openActionDialog(data);
        }
    } else {
        const QMimeData *data = clipboardData();
        if (data != NULL)
            openActionDialog(*data);
        else
            openActionDialog(0);
    }
}

WId MainWindow::openActionDialog(const QMimeData &data)
{
    ActionDialog *actionDialog = createActionDialog();
    actionDialog->setInputData(data);
    actionDialog->show();

    // steal focus
    WId wid = actionDialog->winId();
    createPlatformNativeInterface()->raiseWindow(wid);
    return wid;
}

void MainWindow::openPreferences()
{
    ConfigurationManager::instance()->exec();
}

ClipboardBrowser *MainWindow::browser(int index)
{
    ClipboardBrowser *c = getBrowser(index);
    if (c != NULL)
        c->loadItems();
    return c;
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

void MainWindow::editNewItem()
{
    ClipboardBrowser *c = browser( ui->tabWidget->currentIndex() );
    if (c) {
        showWindow();
        c->setFocus();
        c->editNew();
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
    QModelIndexList list = c->selectionModel()->selectedIndexes();
    qSort(list);
    const int row = list.isEmpty() ? 0 : list.first().row();

    // Insert items from clipboard or just clipboard content.
    if ( data->hasFormat("application/x-copyq-item") ) {
        const QByteArray bytes = data->data("application/x-copyq-item");
        QDataStream in(bytes);

        while ( !in.atEnd() ) {
            ClipboardItem item;
            in >> item;
            c->add(item, true, row);
            ++count;
        }
    } else {
        c->add( cloneData(*data), true, row );
        count = 1;
    }

    // Select new items.
    if (count > 0) {
        QItemSelection sel;
        QModelIndex first = c->index(row);
        QModelIndex last = c->index(row + count - 1);
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
    connect( action, SIGNAL(newItems(QStringList, QModelIndex)),
             this, SLOT(addItems(QStringList, QModelIndex)) );
    connect( action, SIGNAL(newItem(QByteArray, QString, QString)),
             this, SLOT(addItem(QByteArray, QString, QString)) );
    connect( action, SIGNAL(newItem(QByteArray, QString, QModelIndex)),
             this, SLOT(addItem(QByteArray, QString, QModelIndex)) );
    connect( action, SIGNAL(actionStarted(Action*)),
             this, SLOT(actionStarted(Action*)) );
    connect( action, SIGNAL(actionFinished(Action*)),
             this, SLOT(actionFinished(Action*)) );
    connect( action, SIGNAL(actionError(Action*)),
             this, SLOT(actionError(Action*)) );

    log( tr("Executing: %1").arg(action->command()) );
    action->start();
}

void MainWindow::action(const QMimeData &data, const Command &cmd, const QModelIndex &outputIndex)
{
    ActionDialog *actionDialog = createActionDialog();
    QString outputTab;

    actionDialog->setInputData(data);
    actionDialog->setCommand(cmd.cmd);
    actionDialog->setSeparator(cmd.sep);
    actionDialog->setInput(cmd.input);
    actionDialog->setOutput(cmd.output);
    actionDialog->setRegExp(cmd.re);
    actionDialog->setOutputIndex(outputIndex);
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
            ConfigurationManager::instance()->removeItems( c->getID() );
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
