/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

#include "common/common.h"
#include "common/command.h"
#include "common/contenttype.h"
#include "gui/aboutdialog.h"
#include "gui/actiondialog.h"
#include "gui/actionhandler.h"
#include "gui/clipboardbrowser.h"
#include "gui/clipboarddialog.h"
#include "gui/configtabappearance.h"
#include "gui/configurationmanager.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "gui/notification.h"
#include "gui/notificationdaemon.h"
#include "gui/tabdialog.h"
#include "gui/tabwidget.h"
#include "gui/traymenu.h"
#include "item/clipboarditem.h"
#include "item/clipboardmodel.h"
#include "item/serialize.h"
#include "platform/platformnativeinterface.h"
#include "platform/platformwindow.h"

#ifdef Q_OS_MAC
#  include "platform/mac/foregroundbackgroundfilter.h"
#endif

#include <QAction>
#include <QBitmap>
#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QFlags>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QTimer>
#include <QToolBar>
#include <QPainter>

namespace {

const QIcon iconClipboard() { return getIcon("clipboard", IconPaste); }
const QIcon &iconTabNew() { return getIconFromResources("tab_new"); }
const QIcon &iconTabRemove() { return getIconFromResources("tab_remove"); }
const QIcon &iconTabRename() { return getIconFromResources("tab_rename"); }

QIcon appIcon(AppIconFlags flags = AppIconNormal)
{
    return ConfigurationManager::instance()->iconFactory()->appIcon(flags);
}

} // namespace

enum ItemActivationCommand {
    ActivateNoCommand = 0x0,
    ActivateCloses = 0x1,
    ActivateFocuses = 0x2,
    ActivatePastes = 0x4
};

struct MainWindowOptions {
    MainWindowOptions()
        : confirmExit(true)
        , viMode(false)
        , trayCommands(false)
        , trayCurrentTab(false)
        , trayTabName()
        , trayItems(5)
        , trayImages(true)
        , itemPopupInterval(0)
        , clipboardNotificationLines(0)
        , transparency(0)
        , transparencyFocused(0)
        , hideTabs(false)
        , itemActivationCommands(ActivateCloses)
        , clearFirstTab(false)
        , showTray(true)
        , trayItemPaste(true)
    {}

    bool activateCloses() const { return itemActivationCommands & ActivateCloses; }
    bool activateFocuses() const { return itemActivationCommands & ActivateFocuses; }
    bool activatePastes() const { return itemActivationCommands & ActivatePastes; }

    bool confirmExit;
    bool viMode;
    bool trayCommands;
    bool trayCurrentTab;
    QString trayTabName;
    int trayItems;
    bool trayImages;
    int itemPopupInterval;
    int clipboardNotificationLines;
    int transparency;
    int transparencyFocused;

    bool hideTabs;
    bool hideToolbar;

    int itemActivationCommands;

    bool clearFirstTab;

    bool showTray;
    bool trayItemPaste;
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_menuItem(NULL)
    , m_trayMenu( new TrayMenu(this) )
    , m_tray( new QSystemTrayIcon(this) )
    , m_options(new MainWindowOptions)
    , m_clipboardStoringDisabled(false)
    , m_actionToggleClipboardStoring()
    , m_sharedData(new ClipboardBrowserShared)
    , m_lastWindow()
    , m_timerUpdateFocusWindows( new QTimer(this) )
    , m_timerShowWindow( new QTimer(this) )
    , m_timerSaveGeometry( new QTimer(this) )
    , m_trayTimer(NULL)
    , m_notifications(NULL)
    , m_timerMiminizing(NULL)
    , m_minimizeUnsupported(false)
    , m_actionHandler(new ActionHandler(this))
{
    ui->setupUi(this);
    menuBar()->setObjectName("menu_bar");

    ui->tabWidget->addToolBars(this);
    addToolBar(Qt::RightToolBarArea, ui->toolBar);

    // create configuration manager
    ConfigurationManager::createInstance(this);
    ConfigurationManager *cm = ConfigurationManager::instance();

    loadGeometry();
    restoreState( cm->value(objectName() + "_state").toByteArray() );

    updateIcon();

    // signals & slots
    connect( m_tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
             this, SLOT(trayActivated(QSystemTrayIcon::ActivationReason)) );
    connect( m_trayMenu, SIGNAL(aboutToShow()),
             this, SLOT(updateTrayMenuItems()) );
    connect( m_trayMenu, SIGNAL(clipboardItemActionTriggered(uint)),
             this, SLOT(onTrayActionTriggered(uint)) );
    connect( ui->tabWidget, SIGNAL(currentChanged(int,int)),
             this, SLOT(tabChanged(int,int)) );
    connect( ui->tabWidget, SIGNAL(tabMoved(int, int)),
             this, SLOT(tabMoved(int, int)) );
    connect( ui->tabWidget, SIGNAL(tabMoved(QString,QString,QString)),
             this, SLOT(tabMoved(QString,QString,QString)) );
    connect( ui->tabWidget, SIGNAL(tabMenuRequested(QPoint,int)),
             this, SLOT(tabMenuRequested(QPoint,int)) );
    connect( ui->tabWidget, SIGNAL(tabMenuRequested(QPoint,QString)),
             this, SLOT(tabMenuRequested(QPoint,QString)) );
    connect( ui->tabWidget, SIGNAL(tabRenamed(QString,int)),
             this, SLOT(renameTab(QString,int)) );
    connect( ui->tabWidget, SIGNAL(tabCloseRequested(int)),
             this, SLOT(tabCloseRequested(int)) );
    connect( ui->searchBar, SIGNAL(filterChanged(QRegExp)),
             this, SLOT(onFilterChanged(QRegExp)) );
    connect( m_timerUpdateFocusWindows, SIGNAL(timeout()),
             this, SLOT(updateFocusWindows()) );
    connect( this, SIGNAL(changeClipboard(QVariantMap)),
             this, SLOT(clipboardChanged(QVariantMap)) );
    connect( m_actionHandler, SIGNAL(hasRunningActionChanged()),
             this, SLOT(updateIcon()) );
    connect( qApp, SIGNAL(aboutToQuit()),
             this, SLOT(onAboutToQuit()) );

    // settings
    loadSettings();

    ui->tabWidget->setCurrentIndex(0);

    m_timerUpdateFocusWindows->setSingleShot(true);
    m_timerUpdateFocusWindows->setInterval(50);

    m_timerShowWindow->setSingleShot(true);
    m_timerShowWindow->setInterval(250);

    m_timerSaveGeometry->setSingleShot(true);
    m_timerSaveGeometry->setInterval(500);
    connect(m_timerSaveGeometry, SIGNAL(timeout()), SLOT(saveGeometry()));

    // notify window if configuration changes
    connect( cm, SIGNAL(configurationChanged()),
             this, SLOT(loadSettings()) );

    // browse mode by default
    enterBrowseMode();

    m_tray->setContextMenu(m_trayMenu);
}

bool MainWindow::browseMode() const
{
    return !ui->searchBar->isVisible();
}

void MainWindow::exit()
{
    // Check if not editing in any tab (show and try to close editors).
    for ( int i = 0; i < ui->tabWidget->count(); ++i ) {
        ClipboardBrowser *c = getBrowser(i);
        if ( c->editing() ) {
            showBrowser(i);
            if ( !c->maybeCloseEditor() )
                return;
        }
    }

    int answer = QMessageBox::Yes;
    if (m_options->confirmExit) {
        showWindow();
        answer = QMessageBox::question(
                    this,
                    tr("Exit?"),
                    tr("Do you want to <strong>exit</strong> CopyQ?"),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::Yes);
    }

    if (answer == QMessageBox::Yes)
        emit requestExit();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if ( closeMinimizes() && isMinimized() ) {
        exit();
        return;
    }

    hideWindow();
    event->ignore();
    QMainWindow::closeEvent(event);
}

void MainWindow::showEvent(QShowEvent *event)
{
    m_timerShowWindow->start();
    QMainWindow::showEvent(event);
#ifdef COPYQ_WS_X11
    loadGeometry();
#endif
}

void MainWindow::createMenu()
{
    QMenuBar *menubar = menuBar();
    QMenu *menu;
    QAction *act;

    menubar->clear();
    m_trayMenu->clear();

    // File
    menu = menubar->addMenu( tr("&File") );

    // - separator
    menu->addSeparator();

    // - new
    act = createAction( Actions::File_New, SLOT(editNewItem()), menu );
    connect(this, SIGNAL(tabGroupSelected(bool)),
            act, SLOT(setDisabled(bool)) );

    // - import tab
    createAction( Actions::File_ImportTab, SLOT(loadTab()), menu );

    // - export tab
    act = createAction( Actions::File_ExportTab, SLOT(saveTab()), menu );
    connect(this, SIGNAL(tabGroupSelected(bool)),
            act, SLOT(setDisabled(bool)) );

    // - separator
    menu->addSeparator();

    // - preferences
    createAction( Actions::File_Preferences, SLOT(openPreferences()), menu );

    // - show clipboard content
    createAction( Actions::File_ShowClipboardContent, SLOT(showClipboardContent()), menu );

    // - enable/disable
    m_actionToggleClipboardStoring = createAction( Actions::File_ToggleClipboardStoring,
                                                   SLOT(toggleClipboardStoring()), menu );
    updateMonitoringActions();

    // - separator
    menu->addSeparator();

    // - exit
    createAction( Actions::File_Exit, SLOT(exit()), menu );

    // Edit
    menu = menubar->addMenu( tr("&Edit") );

    // - sort
    createAction( Actions::Edit_SortSelectedItems, SLOT(sortSelectedItems()), menu );

    // - reverse order
    createAction( Actions::Edit_ReverseSelectedItems, SLOT(reverseSelectedItems()), menu );

    // - separator
    menu->addSeparator();

    // - paste items
    createAction( Actions::Edit_PasteItems, SLOT(pasteItems()), menu );

    // - copy items
    createAction( Actions::Edit_CopySelectedItems, SLOT(copyItems()), menu );

    // Items
    m_menuItem = menubar->addMenu( tr("&Item") );
    connect(this, SIGNAL(tabGroupSelected(bool)),
            m_menuItem, SLOT(setDisabled(bool)) );

    // Tabs
    menu = menubar->addMenu(tr("&Tabs"));

    // - new tab
    createAction( Actions::Tabs_NewTab, SLOT(newTab()), menu );

    // - rename tab
    act = createAction( Actions::Tabs_RenameTab, SLOT(renameTab()), menu );
    connect(this, SIGNAL(tabGroupSelected(bool)),
            act, SLOT(setDisabled(bool)) );

    // - remove tab
    act = createAction( Actions::Tabs_RemoveTab, SLOT(removeTab()), menu );
    connect(this, SIGNAL(tabGroupSelected(bool)),
            act, SLOT(setDisabled(bool)) );


    // Commands
    menubar->addMenu(&m_actionHandler->commandMenu());

    // Help
    menu = menubar->addMenu(tr("&Help"));
    act = createAction( Actions::Help_Help, SLOT(openAboutDialog()), menu );

    // Tray menu
    act = m_trayMenu->addAction( appIcon(), tr("&Show/Hide"),
                                 this, SLOT(toggleVisible()) );
    m_trayMenu->setDefaultAction(act);
    addTrayAction(Actions::File_New);
    act = addTrayAction(Actions::Item_Action);
    act->setWhatsThis( tr("Open action dialog") );
    addTrayAction(Actions::File_Preferences);
    addTrayAction(Actions::File_ToggleClipboardStoring);
    addTrayAction(Actions::File_Exit);
    m_trayMenu->addMenu(&m_actionHandler->commandTrayMenu());
    m_trayMenu->addSeparator();
    addTrayAction(Actions::File_Exit);
}

void MainWindow::popupTabBarMenu(const QPoint &pos, const QString &tab)
{
    QMenu menu(this);

    const int tabIndex = findBrowserIndex(tab);
    bool hasTab = tabIndex != -1;
    bool isGroup = ui->tabWidget->isTabGroup(tab);

    const QString quotedTab = quoteString(tab);
    QAction *actNew = menu.addAction( iconTabNew(), tr("&New tab") );
    QAction *actRenameGroup =
            isGroup ? menu.addAction( iconTabRename(), tr("Rename &group %1").arg(quotedTab) ) : NULL;
    QAction *actRename =
            hasTab ? menu.addAction( iconTabRename(), tr("Re&name tab %1").arg(quotedTab) ) : NULL;
    QAction *actRemove =
            hasTab ? menu.addAction( iconTabRemove(), tr("Re&move tab %1").arg(quotedTab) ) : NULL;
    QAction *actRemoveGroup =
            isGroup ? menu.addAction( iconTabRename(), tr("Remove group %1").arg(quotedTab) ) : NULL;

    QAction *act = menu.exec(pos);
    if (act != NULL) {
        if (act == actNew)
            newTab(tab);
        else if (act == actRenameGroup)
            renameTabGroup(tab);
        else if (act == actRename)
            renameTab(tabIndex);
        else if (act == actRemove)
            removeTab(true, tabIndex);
        else if (act == actRemoveGroup)
            removeTabGroup(tab);
    }
}

void MainWindow::updateIcon()
{
    AppIconFlag flags = m_clipboardStoringDisabled ? AppIconDisabled : AppIconNormal;
    QIcon icon = appIcon(flags);

    if (m_options->showTray)
        m_tray->setIcon( hasRunningAction() ? appIcon(flags | AppIconRunning) : icon );

    setWindowIcon(icon);
}

void MainWindow::onBrowserError(const QString &errorString)
{
    log(errorString, LogError);
    showMessage( tr("Error"), errorString, QSystemTrayIcon::Critical );
}

void MainWindow::onAboutToQuit()
{
    ConfigurationManager *cm = ConfigurationManager::instance();
    cm->disconnect();
    cm->setValue( objectName() + "_state", saveState() );
    saveCollapsedTabs();
    hideWindow();
    m_tray->hide();
}

void MainWindow::updateNotifications()
{
    if (m_notifications == NULL)
        m_notifications = new NotificationDaemon(this);

    ConfigurationManager *cm = ConfigurationManager::instance();
    const ConfigTabAppearance *appearance = cm->tabAppearance();
    notificationDaemon()->setNotificationOpacity( appearance->themeColor("notification_bg").alphaF() );

    int id = cm->value("notification_position").toInt();
    NotificationDaemon::Position position;
    switch (id) {
    case 0: position = NotificationDaemon::Top; break;
    case 1: position = NotificationDaemon::Bottom; break;
    case 2: position = NotificationDaemon::TopRight; break;
    case 3: position = NotificationDaemon::BottomRight; break;
    case 4: position = NotificationDaemon::BottomLeft; break;
    default: position = NotificationDaemon::TopLeft; break;
    }
    m_notifications->setPosition(position);

    m_notifications->updateInterval(0, m_options->itemPopupInterval);

    m_notifications->updateAppearance();
}

void MainWindow::updateWindowTransparency(bool mouseOver)
{
    int opacity = 100 - (mouseOver || isActiveWindow() ? m_options->transparencyFocused : m_options->transparency);
    setWindowOpacity(opacity / 100.0);
}

void MainWindow::updateMonitoringActions()
{
    if ( !m_actionToggleClipboardStoring.isNull() ) {
        m_actionToggleClipboardStoring->setIcon(
                    appIcon(m_clipboardStoringDisabled ? AppIconNormal : AppIconDisabled) );
        m_actionToggleClipboardStoring->setText( m_clipboardStoringDisabled
                                                 ? tr("&Enable Clipboard Storing")
                                                 : tr("&Disable Clipboard Storing") );
    }
}

ClipboardBrowser *MainWindow::findBrowser(const QString &id)
{
    const int i = findBrowserIndex(id);
    return (i != -1) ? getBrowser(i) : NULL;
}

int MainWindow::findBrowserIndex(const QString &id)
{
    ClipboardBrowser *c = getBrowser(0);
    int i = 0;

    while (c != NULL) {
        if (c->tabName() == id)
            return i;
        c = getBrowser(++i);
    }

    return -1;
}

ClipboardBrowser *MainWindow::getBrowser(int index) const
{
    QWidget *w = ui->tabWidget->widget(
                index < 0 ? ui->tabWidget->currentIndex() : index );
    return qobject_cast<ClipboardBrowser*>(w);
}

void MainWindow::delayedUpdateFocusWindows()
{
    if ( m_options->activateFocuses() || m_options->activatePastes() )
        m_timerUpdateFocusWindows->start();
}

void MainWindow::setHideTabs(bool hide)
{
    ui->tabWidget->setTabBarHidden(hide);
}

void MainWindow::saveCollapsedTabs()
{
    TabWidget *tabs = ui->tabWidget;
    if ( tabs->isTreeModeEnabled() )
        ConfigurationManager::instance()->setValue( "collapsed_tabs", tabs->collapsedTabs() );
}

void MainWindow::loadCollapsedTabs()
{
    TabWidget *tabs = ui->tabWidget;
    if ( tabs->isTreeModeEnabled() )
        tabs->setCollapsedTabs( ConfigurationManager::instance()->value("collapsed_tabs").toStringList() );
}

bool MainWindow::closeMinimizes() const
{
    return !m_options->showTray && !m_minimizeUnsupported;
}

bool MainWindow::triggerActionForData(const QVariantMap &data, const QString &sourceTab)
{
    foreach (const Command &c, m_sharedData->commands) {
        if ( c.automatic && canExecuteCommand(c, data, sourceTab) ) {
            Command cmd = c;
            if ( cmd.outputTab.isEmpty() )
                cmd.outputTab = sourceTab;

            if ( cmd.input.isEmpty() || cmd.input == mimeItems || data.contains(cmd.input) )
                action(data, cmd);

            if (!c.tab.isEmpty())
                addToTab(data, c.tab);

            if (c.remove || c.transform)
                return false;
        }
    }

    return true;
}

bool MainWindow::canSaveGeometry() const
{
    return isVisible() && !isMinimized() && !m_timerShowWindow->isActive();
}

NotificationDaemon *MainWindow::notificationDaemon()
{
    if (m_notifications == NULL)
        updateNotifications();

    return m_notifications;
}

ClipboardBrowser *MainWindow::createTab(const QString &name, bool *needSave)
{
    int i = findTabIndex(name);

    if (needSave != NULL)
        *needSave = (i == -1);

    if (i != -1)
        return getBrowser(i);

    ClipboardBrowser *c = new ClipboardBrowser(this, m_sharedData);
    c->setTabName(name);
    c->loadSettings();
    c->setAutoUpdate(true);

    connect( c, SIGNAL(changeClipboard(QVariantMap)),
             this, SLOT(setClipboard(QVariantMap)) );
    connect( c, SIGNAL(requestActionDialog(const QVariantMap, const Command)),
             this, SLOT(action(const QVariantMap, const Command)) );
    connect( c, SIGNAL(requestActionDialog(const QVariantMap, const Command, const QModelIndex)),
             this, SLOT(action(const QVariantMap&, const Command, const QModelIndex)) );
    connect( c, SIGNAL(requestActionDialog(const QVariantMap)),
             this, SLOT(openActionDialog(const QVariantMap)) );
    connect( c, SIGNAL(requestShow(const ClipboardBrowser*)),
             this, SLOT(showBrowser(const ClipboardBrowser*)) );
    connect( c, SIGNAL(requestHide()),
             this, SLOT(closeAndReturnFocus()) );
    connect( c, SIGNAL(error(QString)),
             this, SLOT(onBrowserError(QString)) );
    connect( c, SIGNAL(doubleClicked(QModelIndex)),
             this, SLOT(activateCurrentItem()) );
    connect( c, SIGNAL(contextMenuUpdated()),
             this, SLOT(onItemMenuUpdated()) );
    connect( c, SIGNAL(addToTab(const QVariantMap,const QString)),
             this, SLOT(addToTab(const QVariantMap,const QString)),
             Qt::DirectConnection );

    ui->tabWidget->addTab(c, name);

    return c;
}

QAction *MainWindow::createAction(Actions::Id id, const char *slot, QMenu *menu)
{
    ConfigTabShortcuts *shortcuts = ConfigurationManager::instance()->tabShortcuts();
    QAction *act = shortcuts->action(id, this, Qt::WindowShortcut);
    connect(act, SIGNAL(triggered()),
            this, slot, Qt::UniqueConnection);
    menu->addAction(act);
    return act;
}

QAction *MainWindow::addTrayAction(Actions::Id id)
{
    ConfigTabShortcuts *shortcuts = ConfigurationManager::instance()->tabShortcuts();
    QAction *act = shortcuts->action(id, NULL, Qt::WindowShortcut);
    m_trayMenu->addAction(act);
    return act;
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

ClipboardBrowser *MainWindow::createTab(const QString &name)
{
    bool needSave;
    ClipboardBrowser *c = createTab(name, &needSave);
    if (needSave)
        ConfigurationManager::instance()->setTabs( ui->tabWidget->tabs() );
    c->loadItems();
    return c;
}

bool MainWindow::hasRunningAction() const
{
    return m_actionHandler->hasRunningAction();
}

QWidget *MainWindow::trayMenu()
{
    return m_trayMenu;
}

void MainWindow::showMessage(const QString &title, const QString &msg,
                             QSystemTrayIcon::MessageIcon icon, int msec, int notificationId)
{
    static const QColor color = NotificationDaemon::getNotificationIconColor(this);
    IconFactory *ifact = ConfigurationManager::instance()->iconFactory();
    QPixmap icon2;

    switch (icon) {
    case QSystemTrayIcon::Information:
        icon2 = ifact->createPixmap(IconInfoSign, color, 32);
        break;
    case QSystemTrayIcon::Warning:
        icon2 = ifact->createPixmap(IconWarningSign, color, 32);
        break;
    case QSystemTrayIcon::Critical:
        icon2 = ifact->createPixmap(IconRemoveSign, color, 32);
        break;
    default:
        break;
    }

    showMessage(title, msg, icon2, msec, notificationId);
}

void MainWindow::showMessage(const QString &title, const QString &msg, const QPixmap &icon,
                             int msec, int notificationId)
{
    notificationDaemon()->create(title, msg, icon, msec, this, notificationId);
}

void MainWindow::showClipboardMessage(const QVariantMap &data)
{
    if ( m_options->itemPopupInterval != 0 && m_options->clipboardNotificationLines > 0) {
        static const QColor color = NotificationDaemon::getNotificationIconColor(this);
        const QPixmap icon =
                ConfigurationManager::instance()->iconFactory()->createPixmap(IconPaste, color, 16);
        notificationDaemon()->create( data, m_options->clipboardNotificationLines, icon,
                                      m_options->itemPopupInterval * 1000, this, 0 );
    }
}

void MainWindow::showError(const QString &msg)
{
    showMessage( tr("CopyQ Error", "Notification error message title"),
                 msg, QSystemTrayIcon::Critical );
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    QString txt;
    ClipboardBrowser *c = getBrowser();

    if (c->editing())
        return;

    if (m_options->hideTabs && event->key() == Qt::Key_Alt)
        setHideTabs(false);

    if (browseMode() && m_options->viMode) {
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

    if ( event->matches(QKeySequence::NextChild) ) {
         nextTab();
         return;
    }

    if ( event->matches(QKeySequence::PreviousChild) ) {
         previousTab();
         return;
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
            if ( c->hasFocus() )
                previousTab();
            break;
        case Qt::Key_Right:
            if ( c->hasFocus() )
                nextTab();
            break;

        case Qt::Key_Return:
        case Qt::Key_Enter:
            if ( c->isLoaded() )
                activateCurrentItem();
            else
                c->loadItemsAgain();
            break;

        case Qt::Key_F3:
            // focus search bar
            enterBrowseMode(false);
            break;

        case Qt::Key_Tab:
            QMainWindow::keyPressEvent(event);
            break;

#ifndef Q_OS_MAC
        case Qt::Key_Backspace:
            resetStatus();
            c->setCurrent(0);
            break;
#endif // Q_OS_MAC

        case Qt::Key_Escape:
            if ( browseMode() ) {
                if (m_lastWindow) {
                    m_lastWindow->raise();
                }
                closeAndReturnFocus();
                getBrowser()->setCurrent(0);
            } else {
                resetStatus();
            }
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
    if (m_options->hideTabs && event->key() == Qt::Key_Alt)
        setHideTabs(true);

    QMainWindow::keyReleaseEvent(event);
}

bool MainWindow::event(QEvent *event)
{
    QEvent::Type type = event->type();

    if (type == QEvent::Enter) {
        updateFocusWindows();
        updateWindowTransparency(true);
    } else if (type == QEvent::Leave) {
        updateWindowTransparency(false);
        setHideTabs(m_options->hideTabs);
    } else if (type == QEvent::WindowActivate) {
        if ( m_timerMiminizing != NULL && m_timerMiminizing->isActive() ) {
            // Window manager ignores window minimizing -- hide it instead.
            m_minimizeUnsupported = true;
            hide();
            return true;
        }

        updateWindowTransparency();
    } else if (type == QEvent::WindowDeactivate) {
        m_timerShowWindow->start();
        m_lastWindow.clear();
        updateWindowTransparency();
        setHideTabs(m_options->hideTabs);
    } else if (type == QEvent::Resize || type == QEvent::Move) {
        if ( canSaveGeometry() )
            m_timerSaveGeometry->start();
    }
    return QMainWindow::event(event);
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
        getBrowser()->clearFilter();
    }
    enterBrowseMode();
}

void MainWindow::loadSettings()
{
    log( tr("Loading configuration") );

    ConfigurationManager *cm = ConfigurationManager::instance();

    ConfigTabAppearance *appearance = cm->tabAppearance();
    appearance->decorateToolBar(ui->toolBar);
    appearance->decorateMainWindow(this);

    // Try to get menu color more precisely by rendering current menu bar and getting color of pixel
    // that is presumably menu background.
    QImage img(1, 1, QImage::Format_RGB32);

    menuBar()->clear();
    QMenu &menu = *menuBar()->addMenu(QString());

    QAction *act = menu.addAction(QString());

    menu.render(&img, QPoint(-8, -8));
    const QColor color = getDefaultIconColor( img.pixel(0, 0) );

    menu.setActiveAction(act);
    menu.render(&img, QPoint(-8, -8));
    menu.removeAction(act);
    const QColor colorActive = getDefaultIconColor( img.pixel(0, 0) );

    cm->iconFactory()->setDefaultColors(color, colorActive);
    cm->updateIcons();

    // update menu items and icons
    createMenu();

    m_options->confirmExit = cm->value("confirm_exit").toBool();

    // always on top window hint
    bool alwaysOnTop = cm->value("always_on_top").toBool();
    bool hasAlwaysOnTop = windowFlags().testFlag(Qt::WindowStaysOnTopHint);
    if (alwaysOnTop != hasAlwaysOnTop) {
        setWindowFlags(windowFlags() ^ Qt::WindowStaysOnTopHint);
        // Workaround for QTBUG-28601.
        setAcceptDrops(true);
    }

    // Vi mode
    m_options->viMode = ConfigurationManager::instance()->value("vi").toBool();

    m_options->transparency = qMax( 0, qMin(100, cm->value("transparency").toInt()) );
    m_options->transparencyFocused = qMax( 0, qMin(100, cm->value("transparency_focused").toInt()) );
    updateWindowTransparency();

    // tab bar position
    ui->tabWidget->setTreeModeEnabled(cm->value("tab_tree").toBool());

    m_options->hideTabs = cm->value("hide_tabs").toBool();
    setHideTabs(m_options->hideTabs);

    m_options->hideToolbar = cm->value("hide_toolbar").toBool();
    ui->toolBar->clear();
    ui->toolBar->setHidden(m_options->hideToolbar);

    saveCollapsedTabs();

    // shared data for browsers
    m_sharedData->loadFromConfiguration();

    // create tabs
    QStringList tabs = cm->savedTabs();
    foreach (const QString &name, tabs) {
        bool settingsLoaded;
        ClipboardBrowser *c = createTab(name, &settingsLoaded);
        if (!settingsLoaded)
            c->loadSettings();
    }

    if ( ui->tabWidget->count() == 0 )
        addTab( tr("&clipboard") );

    browser()->setContextMenu(m_menuItem);

    loadCollapsedTabs();

    m_options->itemActivationCommands = ActivateNoCommand;
    if ( cm->value("activate_closes").toBool() )
        m_options->itemActivationCommands |= ActivateCloses;
    if ( cm->value("activate_focuses").toBool() )
        m_options->itemActivationCommands |= ActivateFocuses;
    if ( cm->value("activate_pastes").toBool() )
        m_options->itemActivationCommands |= ActivatePastes;

    m_options->trayItems = cm->value("tray_items").toInt();
    m_options->trayItemPaste = cm->value("tray_item_paste").toBool();
    m_options->trayCommands = cm->value("tray_commands").toBool();
    m_options->trayCurrentTab = cm->value("tray_tab_is_current").toBool();
    m_options->trayTabName = cm->value("tray_tab").toString();
    m_options->trayImages = cm->value("tray_images").toBool();
    m_options->itemPopupInterval = cm->value("item_popup_interval").toInt();
    m_options->clipboardNotificationLines = cm->value("clipboard_notification_lines").toInt();

    m_trayMenu->setStyleSheet( cm->tabAppearance()->getToolTipStyleSheet() );

    m_options->showTray = !cm->value("disable_tray").toBool();
    delete m_trayTimer;
    if ( m_options->showTray && !QSystemTrayIcon::isSystemTrayAvailable() ) {
        m_options->showTray = false;
        m_trayTimer = new QTimer(this);
        connect( m_trayTimer, SIGNAL(timeout()),
                 this, SLOT(onTrayTimer()) );
        m_trayTimer->setSingleShot(true);
        m_trayTimer->setInterval(1000);
        m_trayTimer->start();
    }

    m_tray->setVisible(m_options->showTray);
    if (!m_options->showTray) {
        if (m_timerMiminizing == NULL) {
            // Check if window manager can minimize window properly.
            // If window is activated while minimizing, assume that minimizing is not supported.
            m_timerMiminizing = new QTimer(this);
            m_timerMiminizing->setSingleShot(true);
            m_timerMiminizing->start(1000);
            showMinimized();
        } else if (isHidden() && !isMinimized()) {
            showMinimized();
        }
    }

    if (m_notifications != NULL)
        updateNotifications();

    updateIcon();

    ui->searchBar->loadSettings();

    m_lastWindow.clear();

    log( tr("Configuration loaded") );
}

void MainWindow::closeAndReturnFocus()
{
    if (m_lastWindow)
        m_lastWindow->raise();

    hideWindow();
}

void MainWindow::showWindow()
{
    if ( m_timerMiminizing != NULL && m_timerMiminizing->isActive() )
        return;

    if ( isActiveWindow() )
        return;

#ifdef COPYQ_WS_X11
    /* Re-initialize window in window manager so it can popup on current workspace. */
    Qt::WindowFlags flags = windowFlags();
    setWindowFlags(flags & Qt::X11BypassWindowManagerHint);
    setWindowFlags(flags);
    loadGeometry();
#endif

    updateFocusWindows();

    showNormal();
    raise();
    activateWindow();

    // if no item is selected then select first
    ClipboardBrowser *c = browser();
    if( c->selectionModel()->selectedIndexes().size() <= 1 &&
            c->currentIndex().row() <= 0 ) {
        c->setCurrent(0);
    }

    if ( !c->editing() ) {
        c->scrollTo( c->currentIndex() );
        c->setFocus();
    }

    QApplication::setActiveWindow(this);

    PlatformWindowPtr window = createPlatformNativeInterface()->getWindow(winId());
    if (window)
        window->raise();
}

void MainWindow::hideWindow()
{
    if (m_timerSaveGeometry->isActive()) {
        m_timerSaveGeometry->stop();
        saveGeometry();
    }

    if ( closeMinimizes() )
        showMinimized();
    else
        hide();
}

bool MainWindow::toggleVisible()
{
    // Showing/hiding window in quick succession doesn't work well on X11.
    if ( m_timerShowWindow->isActive() || isActiveWindow() ) {
        closeAndReturnFocus();
        return false;
    }

    showWindow();
    return true;
}

void MainWindow::showBrowser(const ClipboardBrowser *browser)
{
    int i = 0;
    for( ; i < ui->tabWidget->count() && getBrowser(i) != browser; ++i ) {}
    showBrowser(i);
}

void MainWindow::showBrowser(int index)
{
    if ( index >= 0 && index < ui->tabWidget->count() ) {
        showWindow();
        ui->tabWidget->setCurrentIndex(index);
    }
}

void MainWindow::onTrayActionTriggered(uint clipboardItemHash)
{
    ClipboardBrowser *c = getTabForTrayMenu();
    if ( c->select(clipboardItemHash) && m_lastWindow ) {
        QApplication::processEvents();
        m_lastWindow->pasteClipboard();
    }
}

void MainWindow::trayActivated(QSystemTrayIcon::ActivationReason reason)
{
#ifdef Q_OS_MAC
    // Don't do this on OS X, we only ever get "Trigger"
    Q_UNUSED(reason);
#else
    if ( reason == QSystemTrayIcon::MiddleClick ) {
        toggleMenu();
    } else if ( reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick ) {
        toggleVisible();
    }
#endif // Q_OS_MAC
}

bool MainWindow::toggleMenu()
{
    m_trayMenu->toggle();
    return m_trayMenu->isVisible();
}

void MainWindow::tabChanged(int current, int previous)
{
    if (previous != -1) {
        ClipboardBrowser *before = getBrowser(previous);
        if (before != NULL)
            before->setContextMenu(NULL);
    }

    bool currentIsTabGroup = current == -1;

    m_menuItem->clear();
    onItemMenuUpdated();

    emit tabGroupSelected(currentIsTabGroup);

    if (currentIsTabGroup)
        return;

    // update item menu (necessary for keyboard shortcuts to work)
    ClipboardBrowser *c = browser();
    c->setContextMenu(m_menuItem);

    c->filterItems( ui->searchBar->filter() );

    if ( current >= 0 ) {
        if( !c->currentIndex().isValid() && isVisible() ) {
            c->setCurrent(0);
        }
    }

    setTabOrder(ui->searchBar, c);
}

void MainWindow::tabMoved(int, int)
{
    ConfigurationManager *cm = ConfigurationManager::instance();
    cm->setTabs(ui->tabWidget->tabs());
}

void MainWindow::tabMoved(const QString &oldPrefix, const QString &newPrefix, const QString &afterPrefix)
{
    QStringList tabs;
    TabWidget *w = ui->tabWidget;
    for( int i = 0; i < w->count(); ++i )
        tabs << getBrowser(i)->tabName();

    QString prefix = afterPrefix + '/';

    int afterIndex = -1;
    if ( !afterPrefix.isEmpty() ) {
        afterIndex = tabs.indexOf(afterPrefix);
        if (afterIndex == -1) {
            for (afterIndex = 0; afterIndex < tabs.size() && !tabs[afterIndex].startsWith(prefix); ++afterIndex) {}
            --afterIndex;
        }
    }

    prefix = oldPrefix + '/';

    for (int i = 0, d = 0; i < tabs.size(); ++i) {
        const QString &tab = tabs[i];
        bool down = (i < afterIndex);

        if (i == afterIndex)
            d = 0;
        if ( tab == oldPrefix || tab.startsWith(prefix) ) {
            const int from = down ? i - d : i;

            // Rename tab if needed.
            if (newPrefix != oldPrefix) {
                QString newName = newPrefix + tab.mid(oldPrefix.size());
                if ( newName.isEmpty() || tabs.contains(newName) ) {
                    const QString oldName = newName;
                    int num = 0;
                    do {
                        newName = tr("%1 (%2)", "Format for automatic tab renaming (%1 is name, %2 is number)")
                                .arg(oldName)
                                .arg(++num);
                    } while ( tabs.contains(newName) );

                    w->setTabText(from, newName);
                }

                ClipboardBrowser *c = getBrowser(from);
                c->setTabName(newName);
            }

            // Move tab.
            const int to = afterIndex + (down ? 0 : d + 1);
            w->moveTab(from, to);
            ++d;
        }
    }

    ConfigurationManager::instance()->setTabs( w->tabs() );
}

void MainWindow::tabMenuRequested(const QPoint &pos, int tab)
{
    ClipboardBrowser *c = getBrowser(tab);
    if (c == NULL)
        return;
    const QString tabName = c->tabName();
    popupTabBarMenu(pos, tabName);
}

void MainWindow::tabMenuRequested(const QPoint &pos, const QString &groupPath)
{
    popupTabBarMenu(pos, groupPath);
}

void MainWindow::tabCloseRequested(int tab)
{
    if (tab > 0)
        removeTab(true, tab);
    else
        newTab();
}

void MainWindow::addToTab(const QVariantMap &data, const QString &tabName, bool moveExistingToTop)
{
    ClipboardBrowser *c = tabName.isEmpty() ? browser(0) : findBrowser(tabName);

    if ( c == NULL && !tabName.isEmpty() )
        c = createTab(tabName);

    if (c == NULL)
        return;

    c->loadItems();

    const uint itemHash = hash(data);

    // force adding item if tab name is specified
    bool force = !tabName.isEmpty();

    bool reselectFirst = false;

    if ( c->select(itemHash, moveExistingToTop, false) ) {
        if (!force)
            triggerActionForData(data, tabName);
    } else {
        QVariantMap data2 = data;

        // merge data with first item if it is same
        if ( !force && c->length() > 0 && data2.contains(mimeText) ) {
            const ClipboardItemPtr &first = c->at(0);
            const QByteArray newText = data2[mimeText].toByteArray();
            const QByteArray firstItemText = first->data(mimeText);

            // When selecting text under X11, we can get "new" clipboard data whenever the mouse moves,
            // so we keep updating the same clipboard item instead of adding them all!
            if (
                    // Check that the first item has plain text too
                    first->data().contains(mimeText)
                    && (
                        // If the text is exactly the same, merge them
                        newText == firstItemText
                        || (
                            // If they come from the same window, and the new text extends the previous text, merge them
                            data2.value(mimeWindowTitle) == first->data().value(mimeWindowTitle)
                            && (newText.startsWith(firstItemText) || newText.endsWith(firstItemText))
                            )
                        )
                    )
            {
                force = true;
                const QVariantMap &firstData = first->data();
                foreach (const QString &format, firstData.keys()) {
                    if ( !data2.contains(format) )
                        data2.insert(format, firstData[format]);
                }
                // remove merged item (if it's not edited)
                if (!c->editing() || c->currentIndex().row() != 0) {
                    reselectFirst = c->currentIndex().row() == 0;
                    c->model()->removeRow(0);
                }
            }
        }

        if ( force || triggerActionForData(data2, tabName) )
            c->add(data2);

        if (reselectFirst)
            c->setCurrent(0);
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

void MainWindow::clipboardChanged(const QVariantMap &data)
{
    QString text = textLabelForData(data);
    m_tray->setToolTip( tr("Clipboard:\n%1", "Tray tooltip format").arg(text) );

    showClipboardMessage(data);

    const QString clipboardContent = textLabelForData(data);
    const QString sessionName = qApp->property("CopyQ_session_name").toString();
    if ( sessionName.isEmpty() ) {
        setWindowTitle( tr("%1 - CopyQ", "Main window title format (%1 is clipboard content label)")
                        .arg(clipboardContent) );
    } else {
        setWindowTitle( tr("%1 - %2 - CopyQ",
                           "Main window title format (%1 is clipboard content label, %2 is session name)")
                        .arg(clipboardContent)
                        .arg(sessionName) );
    }
}

void MainWindow::setClipboard(const QVariantMap &data)
{
    if ( !isVisible() || isMinimized() )
        showClipboardMessage(data);
    emit changeClipboard(data);
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
    PlatformWindowPtr lastWindow = m_lastWindow;

    if ( m_options->activateCloses() )
        hideWindow();

    if (lastWindow) {
        if (m_options->activateFocuses())
            lastWindow->raise();

        if (m_options->activatePastes()) {
            QApplication::processEvents();
            lastWindow->pasteClipboard();
        }
    }
}

void MainWindow::disableClipboardStoring(bool disable)
{
    if (m_clipboardStoringDisabled == disable)
        return;

    m_clipboardStoringDisabled = disable;

    updateMonitoringActions();
    updateIcon();
}

void MainWindow::toggleClipboardStoring()
{
    disableClipboardStoring(!m_clipboardStoringDisabled);
}

QByteArray MainWindow::getClipboardData(const QString &mime, QClipboard::Mode mode)
{
    const QMimeData *data = ::clipboardData(mode);
    if (data == NULL)
        return QByteArray();

    return mime == "?" ? data->formats().join("\n").toUtf8() + '\n' : data->data(mime);
}

void MainWindow::pasteToCurrentWindow()
{
    PlatformWindowPtr window = createPlatformNativeInterface()->getCurrentWindow();
    if (window)
        window->pasteClipboard();
}

QStringList MainWindow::tabs() const
{
    return ui->tabWidget->tabs();
}

QVariant MainWindow::config(const QString &name, const QString &value)
{
    ConfigurationManager *cm = ConfigurationManager::instance();

    if ( name.isNull() ) {
        // print options
        QStringList options = cm->options();
        options.sort();
        QString opts;
        foreach (const QString &option, options)
            opts.append( option + "\n  " +
                         cm->optionToolTip(option).replace('\n', "\n  ").toLocal8Bit() + '\n' );
        return opts;
    }

    if ( cm->options().contains(name) ) {
        if ( value.isNull() )
            return cm->value(name).toString(); // return option value

        // set option
        cm->setValue(name, value);

        return QString();
    }

    return QVariant();
}

QString MainWindow::selectedTab() const
{
    return getBrowser()->tabName();
}

QString MainWindow::selectedItems() const
{
    ClipboardBrowser *browser = getBrowser();
    QString result = QString::number(browser->currentIndex().row());

    foreach (const QModelIndex &index, browser->selectionModel()->selectedRows())
        result.append("\n" + QString::number(index.row()));

    return result;
}

ClipboardBrowser *MainWindow::getTabForTrayMenu()
{
    return m_options->trayCurrentTab ? browser()
                            : m_options->trayTabName.isEmpty() ? browser(0) : findTab(m_options->trayTabName);
}

void MainWindow::onFilterChanged(const QRegExp &re)
{
    enterBrowseMode( re.isEmpty() );
    browser()->filterItems(re);
}

void MainWindow::onTrayTimer()
{
    if ( QSystemTrayIcon::isSystemTrayAvailable() ) {
        delete m_trayTimer;
        m_trayTimer = NULL;

        m_options->showTray = true;
        m_tray->setVisible(true);
        if ( isMinimized() )
            hideWindow();

        updateIcon();
    } else {
        m_trayTimer->start();
    }
}

void MainWindow::saveGeometry()
{
    if (canSaveGeometry())
        ConfigurationManager::instance()->saveGeometry(this);
}

void MainWindow::loadGeometry()
{
    ConfigurationManager::instance()->loadGeometry(this);
    m_timerSaveGeometry->stop();
}

void MainWindow::updateFocusWindows()
{
    if ( isActiveWindow() || (!m_options->activateFocuses() && !m_options->activatePastes()) )
        return;

    PlatformPtr platform = createPlatformNativeInterface();

    if ( m_options->activatePastes() || m_options->activateFocuses() ) {
        PlatformWindowPtr lastWindow = platform->getCurrentWindow();
        if (lastWindow)
            m_lastWindow = lastWindow;
    }
}

void MainWindow::onItemMenuUpdated()
{
    if ( !ui->toolBar->isVisible() )
        return;

    ui->toolBar->clear();
    const QColor color = getDefaultIconColor(*ui->toolBar, QPalette::Window);
    foreach ( QAction *action, m_menuItem->actions() ) {
        if ( action->isSeparator() ) {
            ui->toolBar->addSeparator();
        } else if ( !action->icon().isNull() ) {
            QIcon icon = action->icon();
            bool hasIconId;
            const int iconId = action->property("CopyQ_icon_id").toInt(&hasIconId);
            const QString iconTheme = action->property("CopyQ_icon_theme").toString();
            if (hasIconId)
                icon = getIcon(iconTheme, iconId, color, color);
            ui->toolBar->addAction( icon, action->text(), action, SIGNAL(triggered()) );
        }
    }
}

void MainWindow::enterSearchMode(const QString &txt)
{
    enterBrowseMode( txt.isEmpty() );
    ui->searchBar->setText( ui->searchBar->text()+txt );
}

void MainWindow::enterBrowseMode(bool browsemode)
{
    if(browsemode){
        // browse mode
        getBrowser()->setFocus();
        if ( ui->searchBar->text().isEmpty() )
            ui->searchBar->hide();
    } else {
        // search mode
        ui->searchBar->show();
        ui->searchBar->setFocus(Qt::ShortcutFocusReason);
    }
}

void MainWindow::updateTrayMenuItems()
{
    if (m_options->trayItemPaste)
        m_lastWindow = createPlatformNativeInterface()->getCurrentWindow();

    ClipboardBrowser *c = getTabForTrayMenu();

    m_trayMenu->clearClipboardItemActions();
    m_trayMenu->clearCustomActions();

    // Add items.
    const int len = (c != NULL) ? qMin( m_options->trayItems, c->length() ) : 0;
    const int current = c->currentIndex().row();
    for ( int i = 0; i < len; ++i ) {
        const ClipboardItemPtr &item = c->at(i);
        if ( !item.isNull() )
            m_trayMenu->addClipboardItemAction(*item, m_options->trayImages, i == current);
    }

    // Add commands.
    if (m_options->trayCommands) {
        if (c == NULL)
            c = browser(0);
        const QMimeData *data = clipboardData();

        if (data != NULL) {
            const QVariantMap dataMap = cloneData(*data);

            // Show clipboard content as disabled item.
            const QString format = tr("&Clipboard: %1", "Tray menu clipboard item format");
            QAction *act = m_trayMenu->addAction( iconClipboard(),
                                                QString(), this, SLOT(showClipboardContent()) );
            act->setText( textLabelForData(dataMap, act->font(), format, true) );
            m_trayMenu->addCustomAction(act);

            int i = m_trayMenu->actions().size();
            c->addCommandsToMenu(m_trayMenu, dataMap);
            QList<QAction *> actions = m_trayMenu->actions();
            for ( ; i < actions.size(); ++i )
                m_trayMenu->addCustomAction(actions[i]);
        }
    }

    if (m_trayMenu->activeAction() == NULL)
        m_trayMenu->setActiveFirstEnabledAction();
}

void MainWindow::openAboutDialog()
{
    QScopedPointer<AboutDialog> aboutDialog( new AboutDialog(this) );
    aboutDialog->setAttribute(Qt::WA_DeleteOnClose, true);
    aboutDialog->setWindowIcon(appIcon());
    aboutDialog->activateWindow();
    aboutDialog->show();
    aboutDialog.take();
}

void MainWindow::showClipboardContent()
{
    QScopedPointer<ClipboardDialog> clipboardDialog(new ClipboardDialog(ClipboardItemPtr(), this));
    connect( clipboardDialog.data(), SIGNAL(changeClipboard(QVariantMap)),
             this, SLOT(setClipboard(QVariantMap)) );
    clipboardDialog->setAttribute(Qt::WA_DeleteOnClose, true);
    clipboardDialog->show();
    clipboardDialog.take();
}

void MainWindow::openActionDialog(int row)
{
    ClipboardBrowser *c = browser();
    QVariantMap dataMap;
    if (row >= 0) {
        dataMap = c->itemData(row);
    } else if ( hasFocus() ) {
        QModelIndexList selected = c->selectionModel()->selectedRows();
        if (selected.size() == 1)
            dataMap = c->itemData(selected.first().row());
        else
            setTextData( &dataMap, c->selectedText() );
    } else {
        const QMimeData *data = clipboardData();
        if (data != NULL)
            dataMap = cloneData(*data);
    }

    if ( !dataMap.isEmpty() )
        openActionDialog(dataMap);
}

WId MainWindow::openActionDialog(const QVariantMap &data)
{
    QScopedPointer<ActionDialog> actionDialog( m_actionHandler->createActionDialog(ui->tabWidget->tabs()) );
    actionDialog->setWindowIcon(appIcon(AppIconRunning));
    actionDialog->setInputData(data);
    actionDialog->show();

    // steal focus
    WId wid = actionDialog->winId();
    PlatformWindowPtr window = createPlatformNativeInterface()->getWindow(wid);
    if (window)
        window->raise();

    actionDialog.take();

    return wid;
}

void MainWindow::openPreferences()
{
    if ( !isEnabled() )
        return;

    ConfigurationManager::instance()->exec();
}

ClipboardBrowser *MainWindow::browser(int index)
{
    ClipboardBrowser *c = getBrowser(index);
    if (c != NULL)
        c->loadItems();
    return c;
}

ClipboardBrowser *MainWindow::browserForItem(const QModelIndex &index)
{
    if (!index.isValid())
        return NULL;

    // Find browser.
    TabWidget *tabs = ui->tabWidget;
    int i = 0;
    for ( ; i < tabs->count(); ++i ) {
        ClipboardBrowser *c = getBrowser(i);
        if (c->model() == index.model()) {
            c->loadItems();
            return c;
        }
    }

    return NULL;
}

int MainWindow::tabIndex(const ClipboardBrowser *c) const
{
    int i = 0;
    const QWidget *w = dynamic_cast<const QWidget*>(c);
    const QWidget *w2 = ui->tabWidget->widget(i);
    for ( ; w2 != NULL && w != w2; w2 = ui->tabWidget->widget(++i) );
    return w2 == NULL ? -1 : i;
}

ClipboardBrowser *MainWindow::addTab(const QString &name)
{
    TabWidget *w = ui->tabWidget;
    ClipboardBrowser *c = createTab(name);
    w->setCurrentIndex( w->count()-1 );

    return c;
}

void MainWindow::editNewItem()
{
    ClipboardBrowser *c = browser( ui->tabWidget->currentIndex() );
    if (c) {
        showWindow();
        if ( !c->editing() ) {
            c->setFocus();
            c->editNew();
        }
    }
}

void MainWindow::pasteItems()
{
    const QMimeData *data = clipboardData();
    if (data == NULL)
        return;

    ClipboardBrowser *c = browser();
    QModelIndexList list = c->selectionModel()->selectedIndexes();
    qSort(list);
    const int row = list.isEmpty() ? 0 : list.first().row();
    c->paste( cloneData(*data), row );
}

void MainWindow::copyItems()
{
    ClipboardBrowser *c = browser();
    QModelIndexList indexes = c->selectionModel()->selectedRows();

    if ( indexes.isEmpty() )
        return;

    ClipboardItem item;
    QVariantMap data = c->copyIndexes(indexes);
    if ( indexes.size() == 1 ) {
        QVariantMap data2 = c->itemData(indexes.first().row());
        data2.remove(mimeItems);
        data.unite(data2);
    }
    item.setData(data);

    emit changeClipboard(data);
}

bool MainWindow::saveTab(const QString &fileName, int tab_index)
{
    QFile file(fileName);
    if ( !file.open(QIODevice::WriteOnly | QIODevice::Truncate) )
        return false;

    QDataStream out(&file);
    out.setVersion(QDataStream::Qt_4_7);

    int i = tab_index >= 0 ? tab_index : ui->tabWidget->currentIndex();
    ClipboardBrowser *c = browser(i);
    ClipboardModel *model = static_cast<ClipboardModel *>( c->model() );

    out << QByteArray("CopyQ v2") << c->tabName();
    serializeData(*model, &out);

    file.close();

    return true;
}

bool MainWindow::saveTab(int tab_index)
{
    QString fileName = QFileDialog::getSaveFileName( this, QString(), QString(),
                                                     tr("CopyQ Items (*.cpq)") );
    if ( fileName.isNull() )
        return false;

    if ( !fileName.endsWith(".cpq") )
        fileName.append(".cpq");

    if ( !saveTab(fileName, tab_index) ) {
        QMessageBox::critical( this, tr("CopyQ Error Saving File"),
                               tr("Cannot save file %1!")
                               .arg(quoteString(fileName)) );
        return false;
    }

    return true;
}

void MainWindow::saveTabs()
{
    for( int i = 0; i < ui->tabWidget->count(); ++i )
        getBrowser(i)->saveUnsavedItems();
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
    if ( !(header.startsWith("CopyQ v1") || header.startsWith("CopyQ v2")) || tabName.isEmpty() ) {
        file.close();
        return false;
    }

    // Find unique tab name.
    QString baseTabName = tabName;
    QStringList existingTabs = ui->tabWidget->tabs();
    int i = 0;
    while ( existingTabs.contains(tabName) ) {
        log(tabName);
        tabName = baseTabName + " (" + QString::number(++i) + ')';
    }

    ClipboardBrowser *c = createTab(tabName);
    ClipboardModel *model = static_cast<ClipboardModel *>( c->model() );

    deserializeData(model, &in);

    c->loadItems();
    c->saveItems();

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
                               tr("Cannot open file %1!")
                               .arg(quoteString(fileName)) );
        return false;
    }

    TabWidget *w = ui->tabWidget;
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

void MainWindow::action(const QVariantMap &data, const Command &cmd, const QModelIndex &outputIndex)
{
    QScopedPointer<ActionDialog> actionDialog( m_actionHandler->createActionDialog(ui->tabWidget->tabs()) );

    actionDialog->setInputData(data);
    actionDialog->setCommand(cmd.cmd);
    actionDialog->setSeparator(cmd.sep);
    actionDialog->setInput(cmd.input);
    actionDialog->setOutput(cmd.output);
    actionDialog->setOutputIndex(outputIndex);
    QString outputTab = cmd.outputTab;

    QStringList capturedTexts = cmd.re.capturedTexts();
    capturedTexts[0] = getTextData(data);
    actionDialog->setCapturedTexts(capturedTexts);

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
        actionDialog.take();
    } else {
        // Create action without showing action dialog.
        actionDialog->setOutputTabs(QStringList(), outputTab);
        actionDialog->createAction();
    }
}

void MainWindow::newTab(const QString &name)
{
    TabDialog *d = new TabDialog(TabDialog::TabNew, this);
    d->setAttribute(Qt::WA_DeleteOnClose, true);

    d->setTabs(ui->tabWidget->tabs());

    QString tabPath = name;

    if ( tabPath.isNull() ) {
        tabPath = ui->tabWidget->getCurrentTabPath();
        if ( ui->tabWidget->isTabGroup(tabPath) )
            tabPath.append('/');
    }

    d->setTabName( tabPath.mid(0, tabPath.lastIndexOf('/') + 1) );

    connect( d, SIGNAL(accepted(QString, int)),
             this, SLOT(addTab(QString)) );

    d->open();
}

void MainWindow::renameTabGroup(const QString &name)
{
    TabDialog *d = new TabDialog(TabDialog::TabGroupRename, this);
    d->setAttribute(Qt::WA_DeleteOnClose, true);

    d->setTabs(ui->tabWidget->tabs());
    d->setTabGroupName(name);

    connect( d, SIGNAL(accepted(QString, QString)),
             this, SLOT(renameTabGroup(QString, QString)) );

    d->open();
}

void MainWindow::renameTabGroup(const QString &newName, const QString &oldName)
{
    const QStringList tabs = ui->tabWidget->tabs();
    const QString tabPrefix = oldName + '/';

    for ( int i = 0; i < tabs.size(); ++i ) {
        const QString &tab = tabs[i];
        if ( tab == oldName || tab.startsWith(tabPrefix) )
            renameTab( newName + tab.mid(oldName.size()), i );
    }
}

void MainWindow::renameTab(int tab)
{
    TabDialog *d = new TabDialog(TabDialog::TabRename, this);
    d->setAttribute(Qt::WA_DeleteOnClose, true);
    int i = tab >= 0 ? tab : ui->tabWidget->currentIndex();
    if (i < 0)
        return;

    d->setTabIndex(i);
    d->setTabs(ui->tabWidget->tabs());
    d->setTabName(browser(i)->tabName());

    connect( d, SIGNAL(accepted(QString, int)),
             this, SLOT(renameTab(QString, int)) );

    d->open();
}

void MainWindow::renameTab(const QString &name, int tabIndex)
{
    if ( name.isEmpty() || ui->tabWidget->tabs().contains(name) )
        return;

    ClipboardBrowser *c = getBrowser(tabIndex);

    c->setTabName(name);
    ui->tabWidget->setTabText(tabIndex, name);

    ConfigurationManager *cm = ConfigurationManager::instance();
    cm->setTabs(ui->tabWidget->tabs());
}

void MainWindow::removeTabGroup(const QString &name)
{
    int answer = QMessageBox::question(
                this,
                tr("Remove All Tabs in Group?"),
                tr("Do you want to remove <strong>all tabs</strong> in group <strong>%1</strong>?"
                   ).arg(name),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes);
    if (answer == QMessageBox::Yes) {
        ui->tabWidget->setCurrentIndex(0);
        const QStringList tabs = ui->tabWidget->tabs();
        const QString tabPrefix = name + '/';
        const int currentTabIndex = ui->tabWidget->currentIndex();

        for ( int i = tabs.size() - 1; i >= 0; --i ) {
            const QString &tab = tabs[i];
            if ( tab == name || tab.startsWith(tabPrefix) ) {
                if (currentTabIndex == i)
                    ui->tabWidget->setCurrentIndex(0);
                ClipboardBrowser *c = getBrowser(i);
                c->purgeItems();
                c->deleteLater();
                ui->tabWidget->removeTab(i);
            }
        }

        ConfigurationManager::instance()->setTabs( ui->tabWidget->tabs() );
    }
}

void MainWindow::removeTab()
{
    removeTab(true, ui->tabWidget->currentIndex());
}

void MainWindow::removeTab(bool ask, int tabIndex)
{
    if (tabIndex < 0)
        return;

    TabWidget *w = ui->tabWidget;

    ClipboardBrowser *c = getBrowser(tabIndex);

    if ( c != NULL && w->count() > 1 ) {
        int answer = QMessageBox::Yes;
        if (ask) {
            answer = QMessageBox::question(
                        this,
                        tr("Remove Tab?"),
                        tr("Do you want to remove tab <strong>%1</strong>?"
                           ).arg( w->tabText(tabIndex).remove('&') ),
                        QMessageBox::Yes | QMessageBox::No,
                        QMessageBox::Yes);
        }
        if (answer == QMessageBox::Yes) {
            if ( ui->tabWidget->currentIndex() == tabIndex )
                ui->tabWidget->setCurrentIndex(tabIndex == 0 ? 1 : 0);
            c->purgeItems();
            c->deleteLater();
            w->removeTab(tabIndex);
            ConfigurationManager::instance()->setTabs( ui->tabWidget->tabs() );
        }
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}
