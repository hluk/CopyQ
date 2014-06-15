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
#include "common/log.h"
#include "common/mimetypes.h"
#include "gui/aboutdialog.h"
#include "gui/actiondialog.h"
#include "gui/actionhandler.h"
#include "gui/clipboardbrowser.h"
#include "gui/clipboarddialog.h"
#include "gui/configtabappearance.h"
#include "gui/configurationmanager.h"
#include "gui/iconfactory.h"
#include "gui/iconselectdialog.h"
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

const int clipboardNotificationId = 0;

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
    , m_trayTimer(NULL)
    , m_trayIconSnipTimer(NULL)
    , m_notifications(NULL)
    , m_timerMiminizing(NULL)
    , m_minimizeUnsupported(false)
    , m_actionHandler(new ActionHandler(this))
    , m_ignoreCurrentClipboard(true)
    , m_trayTab(NULL)
{
    ui->setupUi(this);
    menuBar()->setObjectName("menu_bar");

    ui->tabWidget->addToolBars(this);
    addToolBar(Qt::RightToolBarArea, ui->toolBar);

    ConfigurationManager *cm = ConfigurationManager::instance();
    cm->registerWindowGeometry(this);
    restoreState( cm->mainWindowState(objectName()) );

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
             this, SLOT(saveTabPositions()) );
    connect( ui->tabWidget, SIGNAL(tabsMoved(QString,QString)),
             this, SLOT(tabsMoved(QString,QString)) );
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

    // - active commands
    createAction( Actions::File_ProcessManager, SLOT(showProcessManagerDialog()), menu );

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

    createAction( Actions::Tabs_ChangeTabIcon, SLOT(setTabIcon()), menu );

    // Help
    menu = menubar->addMenu(tr("&Help"));
    createAction( Actions::Help_Help, SLOT(openAboutDialog()), menu );

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
    QAction *actNew = menu.addAction( iconTabNew(), tr("&New Tab") );
    QAction *actRenameGroup =
            isGroup ? menu.addAction( iconTabRename(), tr("Rename &Group %1").arg(quotedTab) ) : NULL;
    QAction *actRename =
            hasTab ? menu.addAction( iconTabRename(), tr("Re&name Tab %1").arg(quotedTab) ) : NULL;
    QAction *actRemove =
            hasTab ? menu.addAction( iconTabRemove(), tr("Re&move Tab %1").arg(quotedTab) ) : NULL;
    QAction *actRemoveGroup =
            isGroup ? menu.addAction( iconTabRename(), tr("Remove Group %1").arg(quotedTab) ) : NULL;

    QAction *actIcon = menu.addAction( tr("&Change Tab Icon") );

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
        else if (act == actIcon)
            setTabIcon(tab);
    }
}

void MainWindow::updateIcon()
{
    AppIconFlags flags = m_clipboardStoringDisabled ? AppIconDisabled : AppIconNormal;

    if (hasRunningAction()) {
        flags |= AppIconRunning;

        // Show running icon for some minimal time (nice snip effect with default icon).
        if (!m_trayIconSnipTimer) {
            m_trayIconSnipTimer = new QTimer(this);
            m_trayIconSnipTimer->start(250);
            m_trayIconSnipTimer->setSingleShot(true);
            connect( m_trayIconSnipTimer, SIGNAL(timeout()), this, SLOT(updateIcon()) );
        }
    } else if (m_trayIconSnipTimer) {
        if (m_trayIconSnipTimer->isActive()) {
            flags |= AppIconRunning;
        } else {
            delete m_trayIconSnipTimer;
            m_trayIconSnipTimer = NULL;
        }
    }

    const QIcon icon = appIcon(flags);

    if (m_options->showTray)
        m_tray->setIcon(icon);

    setWindowIcon(icon);
}

void MainWindow::onAboutToQuit()
{
    ConfigurationManager *cm = ConfigurationManager::instance();
    cm->disconnect();
    cm->saveMainWindowState( objectName(), saveState() );
    ui->tabWidget->saveTabInfo();
    hideWindow();
    m_tray->hide();
}

void MainWindow::on_tabWidget_dropItems(const QString &tabName, const QMimeData &data)
{
    ClipboardBrowser *browser = createTab(tabName);
    browser->loadItemsAgain();
    if ( browser->isLoaded() ) {
        const QVariantMap dataMap = data.hasFormat(mimeItems)
                ? cloneData(data, QStringList() << mimeItems) : cloneData(data);
        browser->paste(dataMap, 0);
    }
}

void MainWindow::updateNotifications()
{
    if (m_notifications == NULL)
        m_notifications = new NotificationDaemon(this);

    ConfigurationManager *cm = ConfigurationManager::instance();
    const ConfigTabAppearance *appearance = cm->tabAppearance();
    notificationDaemon()->setNotificationOpacity( appearance->themeColor("notification_bg").alphaF() );
    notificationDaemon()->setNotificationStyleSheet( appearance->getNotificationStyleSheet() );

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

    const int x = cm->value("notification_horizontal_offset").toInt();
    const int y = cm->value("notification_vertical_offset").toInt();
    m_notifications->setOffset(x, y);

    const int w = cm->value("notification_maximum_width").toInt();
    const int h = cm->value("notification_maximum_height").toInt();
    m_notifications->setMaximumSize(w, h);

    m_notifications->updateInterval(0, m_options->itemPopupInterval);

    m_notifications->updateNotifications();
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

int MainWindow::findBrowserIndex(const QString &id)
{
    return ui->tabWidget->tabs().indexOf(id);
}

ClipboardBrowser *MainWindow::getBrowser(int index) const
{
    return qobject_cast<ClipboardBrowser*>( ui->tabWidget->widget(index) );
}

ClipboardBrowser *MainWindow::getBrowser() const
{
    return qobject_cast<ClipboardBrowser*>(ui->tabWidget->currentWidget());
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
    connect( c, SIGNAL(requestActionDialog(const QVariantMap, const Command, const QModelIndex)),
             this, SLOT(action(const QVariantMap&, const Command, const QModelIndex)) );
    connect( c, SIGNAL(requestActionDialog(const QVariantMap)),
             this, SLOT(openActionDialog(const QVariantMap)) );
    connect( c, SIGNAL(requestShow(const ClipboardBrowser*)),
             this, SLOT(showBrowser(const ClipboardBrowser*)) );
    connect( c, SIGNAL(requestHide()),
             this, SLOT(closeAndReturnFocus()) );
    connect( c, SIGNAL(error(QString)),
             this, SLOT(showError(QString)) );
    connect( c, SIGNAL(doubleClicked(QModelIndex)),
             this, SLOT(activateCurrentItem()) );
    connect( c, SIGNAL(contextMenuUpdated()),
             this, SLOT(onItemMenuUpdated()) );
    connect( c, SIGNAL(addToTab(const QVariantMap,const QString)),
             this, SLOT(addToTab(const QVariantMap,const QString)),
             Qt::DirectConnection );
    connect( c, SIGNAL(itemCountChanged(QString,int)),
             ui->tabWidget, SLOT(setTabItemCount(QString,int)) );

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

void MainWindow::updateTabIcon(const QString &newName, const QString &oldName)
{
    const QString icon = ConfigurationManager::instance()->getIconForTabName(oldName);
    if ( !icon.isEmpty() ) {
        ConfigurationManager::instance()->setIconForTabName(oldName, QString());
        ConfigurationManager::instance()->setIconForTabName(newName, icon);
    }
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
        saveTabPositions();
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

QByteArray MainWindow::getActionData(const QByteArray &actionId, const QString &format)
{
    return m_actionHandler->getActionData(actionId, format);
}

void MainWindow::showMessage(const QString &title, const QString &msg,
                             QSystemTrayIcon::MessageIcon icon, int msec, int notificationId)
{
    ushort icon2 = 0;

    switch (icon) {
    case QSystemTrayIcon::Information:
        icon2 = IconInfoSign;
        break;
    case QSystemTrayIcon::Warning:
        icon2 = IconWarningSign;
        break;
    case QSystemTrayIcon::Critical:
        icon2 = IconRemoveSign;
        break;
    default:
        break;
    }

    showMessage(title, msg, icon2, msec, notificationId);
}

void MainWindow::showMessage(const QString &title, const QString &msg, ushort icon,
                             int msec, int notificationId)
{
    notificationDaemon()->create(title, msg, icon, msec, notificationId);
}

void MainWindow::showClipboardMessage(const QVariantMap &data)
{
    if ( m_options->itemPopupInterval != 0 && m_options->clipboardNotificationLines > 0) {
        if (data.isEmpty()) {
            notificationDaemon()->removeNotification(clipboardNotificationId);
        } else {
            notificationDaemon()->create( data, m_options->clipboardNotificationLines, IconPaste,
                                          m_options->itemPopupInterval * 1000, clipboardNotificationId );
        }
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
    COPYQ_LOG("Loading configuration");

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
    ui->tabWidget->setTabItemCountVisible(cm->value("show_tab_item_count").toBool());

    m_options->hideTabs = cm->value("hide_tabs").toBool();
    setHideTabs(m_options->hideTabs);

    bool hideToolbar = cm->value("hide_toolbar").toBool();
    ui->toolBar->clear();
    ui->toolBar->setHidden(hideToolbar);
    bool hideToolBarLabels = cm->value("hide_toolbar_labels").toBool();
    ui->toolBar->setToolButtonStyle(hideToolBarLabels ? Qt::ToolButtonIconOnly
                                                      : Qt::ToolButtonTextUnderIcon);

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

    ui->tabWidget->updateTabs();

    browser()->setContextMenu(m_menuItem);

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
        connect( m_trayTimer, SIGNAL(timeout()), this, SLOT(createTrayIfSupported()) );
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

    COPYQ_LOG("Configuration loaded");
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

bool MainWindow::toggleMenu(ClipboardBrowser *browser)
{
    m_trayTab = browser;
    m_trayMenu->toggle();
    m_trayTab = NULL;
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

void MainWindow::saveTabPositions()
{
    ConfigurationManager::instance()->setTabs( ui->tabWidget->tabs() );
}

void MainWindow::tabsMoved(const QString &oldPrefix, const QString &newPrefix)
{
    const QStringList tabs = ui->tabWidget->tabs();
    Q_ASSERT( oldPrefix == newPrefix || !tabs.contains(oldPrefix) );
    Q_ASSERT( !tabs.contains(QString()) );
    Q_ASSERT( tabs.toSet().size() == tabs.size() );

    const QString prefix = oldPrefix + '/';

    // Rename tabs if needed.
    for (int i = 0 ; i < tabs.size(); ++i) {
        ClipboardBrowser *c = getBrowser(i);
        const QString oldTabName = c->tabName();

        if ( (oldTabName == oldPrefix || oldTabName.startsWith(prefix)) && newPrefix != oldPrefix) {
            const QString newName = newPrefix + oldTabName.mid(oldPrefix.size());
            updateTabIcon(newName, c->tabName());
            c->setTabName(newName);
        }
    }

    saveTabPositions();
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
    removeTab(true, tab);
}

void MainWindow::addToTab(const QVariantMap &data, const QString &tabName, bool moveExistingToTop)
{
    ClipboardBrowser *c = getBrowser( tabName.isEmpty() ? 0 : findBrowserIndex(tabName) );

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
    m_clipboardData = m_clipboardStoringDisabled ? QVariantMap() : data;
    m_ignoreCurrentClipboard = false;

    if ( m_clipboardData.isEmpty() ) {
        m_tray->setToolTip(QString());
        setWindowTitle("CopyQ");
    } else {
        QString text = textLabelForData(m_clipboardData);
        m_tray->setToolTip( tr("Clipboard:\n%1", "Tray tooltip format").arg(text) );

        const QString clipboardContent = textLabelForData(m_clipboardData);
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

    showClipboardMessage(m_clipboardData);
}

void MainWindow::setClipboard(const QVariantMap &data)
{
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

    if (m_clipboardStoringDisabled)
        clipboardChanged(QVariantMap());

    COPYQ_LOG( QString("Clipboard monitoring %1.")
               .arg(m_clipboardStoringDisabled ? "disabled" : "enabled") );
}

bool MainWindow::isMonitoringEnabled() const
{
    return !m_clipboardStoringDisabled;
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

void MainWindow::ignoreCurrentClipboard()
{
    clipboardChanged(QVariantMap());
    m_ignoreCurrentClipboard = true;
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
            opts.append( option + "\n  " + cm->optionToolTip(option).replace('\n', "\n  ") + '\n' );
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

QList<int> MainWindow::selectedItems() const
{
    ClipboardBrowser *browser = getBrowser();
    QModelIndexList selectedRows = browser->selectionModel()->selectedRows();

    QList<int> result;
    result.reserve( selectedRows.size() );

    foreach (const QModelIndex &index, selectedRows)
        result.append(index.row());

    return result;
}

ClipboardBrowser *MainWindow::getTabForTrayMenu()
{
    if (m_trayTab)
        return m_trayTab;

    return m_options->trayCurrentTab ? browser()
                            : m_options->trayTabName.isEmpty() ? browser(0) : findTab(m_options->trayTabName);
}

void MainWindow::onFilterChanged(const QRegExp &re)
{
    enterBrowseMode( re.isEmpty() );
    browser()->filterItems(re);
}

void MainWindow::createTrayIfSupported()
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
            const QString text = action->text().remove("&");
            const QString shortcut = action->shortcut().toString(QKeySequence::NativeText);
            const QString label = text + (shortcut.isEmpty() ? QString() : "\n[" + shortcut + "]");
            const QString tooltip = "<center>" + escapeHtml(text)
                    + (shortcut.isEmpty() ? QString() : "<br /><b>" + escapeHtml(shortcut) + "</b>") + "</center>";
            ui->toolBar->addAction( icon, label, action, SIGNAL(triggered()) )->setToolTip(tooltip);
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

        if (!m_clipboardData.isEmpty()) {
            // Show clipboard content as disabled item.
            const QString format = tr("&Clipboard: %1", "Tray menu clipboard item format");
            QAction *act = m_trayMenu->addAction( iconClipboard(),
                                                QString(), this, SLOT(showClipboardContent()) );
            act->setText( textLabelForData(m_clipboardData, act->font(), format, true) );
            m_trayMenu->addCustomAction(act);

            int i = m_trayMenu->actions().size();
            c->addCommandsToMenu(m_trayMenu, m_clipboardData);
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

void MainWindow::showProcessManagerDialog()
{
    m_actionHandler->showProcessManagerDialog();
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

ClipboardBrowser *MainWindow::browser()
{
    return browser( ui->tabWidget->currentIndex() );
}

ClipboardBrowser *MainWindow::browserForItem(const QModelIndex &index)
{
    if ( index.isValid() ) {
        for ( int i = 0; i < ui->tabWidget->count(); ++i ) {
            ClipboardBrowser *c = getBrowser(i);
            if (c->model() == index.model()) {
                c->loadItems();
                return c;
            }
        }
    }

    return NULL;
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
    renameToUnique(&tabName, ui->tabWidget->tabs());

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
    actionDialog->setCommand(cmd);
    actionDialog->setOutputIndex(outputIndex);
    QString outputTab = cmd.outputTab;

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
    if (c) {
        updateTabIcon(name, c->tabName());
        c->setTabName(name);
        ui->tabWidget->setTabText(tabIndex, name);
        saveTabPositions();
    }
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

        saveTabPositions();
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
            saveTabPositions();
        }
    }
}

void MainWindow::setTabIcon()
{
    setTabIcon( browser()->tabName() );
}

void MainWindow::setTabIcon(const QString &tabName)
{
    ConfigurationManager *cm = ConfigurationManager::instance();
    IconSelectDialog dialog( cm->getIconForTabName(tabName), this );

    if ( dialog.exec() == QDialog::Accepted ) {
        cm->setIconForTabName( tabName, dialog.selectedIcon() );
        ui->tabWidget->updateTabIcon(tabName);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}
