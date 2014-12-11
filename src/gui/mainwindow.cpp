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

#include "common/action.h"
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
#include "gui/commandaction.h"
#include "gui/commanddialog.h"
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
#include <QModelIndex>
#include <QPushButton>
#include <QTimer>
#include <QToolBar>
#include <QPainter>

namespace {

const QIcon iconClipboard() { return getIcon("clipboard", IconPaste); }
const QIcon iconTabIcon() { return getIconFromResources("tab_icon"); }
const QIcon iconTabNew() { return getIconFromResources("tab_new"); }
const QIcon iconTabRemove() { return getIconFromResources("tab_remove"); }
const QIcon iconTabRename() { return getIconFromResources("tab_rename"); }

const int clipboardNotificationId = 0;

QIcon appIcon(AppIconFlags flags = AppIconNormal)
{
    return ConfigurationManager::instance()->iconFactory()->appIcon(flags);
}

bool canPaste()
{
    return !QApplication::queryKeyboardModifiers().testFlag(Qt::ControlModifier);
}

bool canExecuteCommand(const Command &command, const QVariantMap &data, const QString &sourceTabName)
{
    // Verify that an action is provided.
    if ( command.cmd.isEmpty() && !command.remove
         && (command.tab.isEmpty() || command.tab == sourceTabName) )
    {
        return false;
    }

    // Verify that data for given MIME is available.
    if ( !command.input.isEmpty() ) {
        const QList<QString> availableFormats = data.keys();
        if (command.input == mimeItems) {
            // Disallow applying action that takes serialized item more times.
            if ( availableFormats.contains(command.output) )
                return false;
        } else if ( !availableFormats.contains(command.input) ) {
            return false;
        }
    }

    // Verify that text is present when regex is defined.
    if ( !command.re.isEmpty() && !data.contains(mimeText) )
        return false;

    // Verify that and text, MIME type and window title are matched.
    const QString text = getTextData(data);
    const QString windowTitle = data.value(mimeWindowTitle).toString();
    if ( command.re.indexIn(text) == -1 || command.wndre.indexIn(windowTitle) == -1 )
        return false;

    return true;
}

bool hasFormat(const QVariantMap &data, const QString &format)
{
    if (format == mimeItems) {
        foreach (const QString &key, data.keys()) {
            if ( !key.startsWith(COPYQ_MIME_PREFIX) )
                return true;
        }
        return false;
    }

    return data.contains(format);
}

WId stealFocus(const QWidget &window)
{
    WId wid = window.winId();
    PlatformWindowPtr platformWindow = createPlatformNativeInterface()->getWindow(wid);
    if (platformWindow)
        platformWindow->raise();

    return wid;
}

template <typename WidgetOrAction>
void disableActionWhenTabGroupSelected(WidgetOrAction *action, MainWindow *window)
{
    QObject::connect( window, SIGNAL(tabGroupSelected(bool)),
                      action, SLOT(setDisabled(bool)) );
}

/// Adds information about current tab and selection if command is triggered by user.
QVariantMap addSelectionData(const ClipboardBrowser &c, const QVariantMap &data)
{
    QVariantMap result = data;
    const QItemSelectionModel *selectionModel = c.selectionModel();
    const QModelIndexList selectedIndexes = selectionModel->selectedIndexes();

    QList<QPersistentModelIndex> selected;
    selected.reserve(selectedIndexes.size());
    foreach (const QModelIndex &index, selectedIndexes)
        selected.append(index);

    result.insert(mimeCurrentTab, c.tabName());
    result.insert(mimeCurrentItem, QVariant::fromValue(QPersistentModelIndex(selectionModel->currentIndex())));
    result.insert(mimeSelectedItems, QVariant::fromValue(selected));

    return result;
}

QMenu *findSubMenu(const QString &name, const QMenu &menu)
{
    foreach (QAction *action, menu.actions()) {
        QMenu *subMenu = action->menu();
        if (subMenu && subMenu->title() == name)
            return subMenu;
    }

    return NULL;
}

QMenu *createSubMenus(QString *name, QMenu *menu)
{
    QStringList path = name->split('|');
    if (path.size() == 1)
        return menu;

    *name = path.takeLast();

    QMenu *parentMenu = menu;

    foreach (const QString &subMenuName, path) {
        QMenu *subMenu = findSubMenu(subMenuName, *parentMenu);

        if (!subMenu) {
            subMenu = new QMenu(subMenuName, parentMenu);
            parentMenu->addMenu(subMenu);
        }

        parentMenu = subMenu;
    }

    return parentMenu;
}

Command automaticCommand(
        const QString &name,
        const QString &cmd,
        bool forClipboard = true,
        const QString &input = QString())
{
    Command c;
    c.name = name;
    c.automatic = true;
    c.input = input;
    const QString negate = forClipboard ? "" : "!";
    c.cmd = "copyq: if (monitoring() && " + negate + "hasDataFromClipboard()) " + cmd;

    return c;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , cm(ConfigurationManager::createInstance(this))
    , ui(new Ui::MainWindow)
    , m_menuItem(NULL)
    , m_trayMenu( new TrayMenu(this) )
    , m_tray(NULL)
    , m_clipboardStoringDisabled(false)
    , m_actionToggleClipboardStoring()
    , m_sharedData(new ClipboardBrowserShared)
    , m_lastWindow()
    , m_notifications(NULL)
    , m_minimizeUnsupported(false)
    , m_actionHandler(new ActionHandler(this))
    , m_trayTab(NULL)
    , m_commandDialog(NULL)
    , m_ignoreUpdateTitle(false)
{
    ui->setupUi(this);
    menuBar()->setObjectName("menu_bar");
    createMenu();

    ui->tabWidget->addToolBars(this);
    addToolBar(Qt::RightToolBarArea, ui->toolBar);

    cm->registerWindowGeometry(this);
    restoreState( cm->mainWindowState(objectName()) );

    updateIcon();

    // signals & slots
    connect( m_trayMenu, SIGNAL(aboutToShow()),
             this, SLOT(updateTrayMenuItems()) );
    connect( m_trayMenu, SIGNAL(clipboardItemActionTriggered(uint,bool)),
             this, SLOT(onTrayActionTriggered(uint,bool)) );
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
    connect( m_actionHandler, SIGNAL(hasRunningActionChanged()),
             this, SLOT(updateIcon()) );
    connect( qApp, SIGNAL(aboutToQuit()),
             this, SLOT(onAboutToQuit()) );

    connect(&m_itemMenuCommandTester, SIGNAL(commandPassed(Command,bool)),
            SLOT(addCommandsToItemMenu(Command,bool)));
    connect(&m_trayMenuCommandTester, SIGNAL(commandPassed(Command,bool)),
            SLOT(addCommandsToTrayMenu(Command,bool)));
    connect(&m_automaticCommandTester, SIGNAL(commandPassed(Command,bool)),
            SLOT(automaticCommandTestFinished(Command,bool)));

    m_commands = loadCommands();
    loadSettings();

    ui->tabWidget->setCurrentIndex(0);

    initSingleShotTimer( &m_timerUpdateFocusWindows, 50, this, SLOT(updateFocusWindows()) );
    initSingleShotTimer( &m_timerUpdateContextMenu, 0, this, SLOT(updateContextMenuTimeout()) );
    initSingleShotTimer( &m_timerShowWindow, 250 );
    initSingleShotTimer( &m_timerTrayAvailable, 1000, this, SLOT(createTrayIfSupported()) );
    initSingleShotTimer( &m_timerTrayIconSnip, 250, this, SLOT(updateIconTimeout()) );

    // notify window if configuration changes
    connect( cm, SIGNAL(configurationChanged()),
             this, SLOT(loadSettings()) );
    connect( cm, SIGNAL(error(QString)),
             this, SLOT(showError(QString)) );

    // browse mode by default
    enterBrowseMode();
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
    if (m_options.confirmExit) {
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
    event->ignore();

    if ( closeMinimizes() )
        showMinimized();
    else
        QMainWindow::closeEvent(event);
}

void MainWindow::showEvent(QShowEvent *event)
{
    m_timerShowWindow.start();
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

    // - new
    act = createAction( Actions::File_New, SLOT(editNewItem()), menu );
    disableActionWhenTabGroupSelected(act, this);

    // - import tab
    createAction( Actions::File_ImportTab, SLOT(loadTab()), menu );

    // - export tab
    act = createAction( Actions::File_ExportTab, SLOT(saveTab()), menu );
    disableActionWhenTabGroupSelected(act, this);

    // - separator
    menu->addSeparator();

    // - preferences
    createAction( Actions::File_Preferences, SLOT(openPreferences()), menu );

    // - commands
    createAction( Actions::File_Commands, SLOT(openCommands()), menu );

    // - separator
    menu->addSeparator();

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

    // - find
    createAction( Actions::Edit_FindItems, SLOT(findNext()), menu );

    // - separator
    menu->addSeparator();

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
    disableActionWhenTabGroupSelected(m_menuItem, this);

    // Tabs
    menu = menubar->addMenu(tr("&Tabs"));

    // - new tab
    createAction( Actions::Tabs_NewTab, SLOT(newTab()), menu );

    // - rename tab
    act = createAction( Actions::Tabs_RenameTab, SLOT(renameTab()), menu );
    disableActionWhenTabGroupSelected(act, this);

    // - remove tab
    act = createAction( Actions::Tabs_RemoveTab, SLOT(removeTab()), menu );
    disableActionWhenTabGroupSelected(act, this);

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

    const int tabIndex = ui->tabWidget->tabs().indexOf(tab);
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
            isGroup ? menu.addAction( iconTabRemove(), tr("Remove Group %1").arg(quotedTab) ) : NULL;

    QAction *actIcon = menu.addAction( iconTabIcon(), tr("&Change Tab Icon") );

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

    // Show running icon for some minimal time (nice snip effect with default icon).
    if ( hasRunningAction() || m_timerTrayIconSnip.isActive() ) {
        flags |= AppIconRunning;

        if ( !m_timerTrayIconSnip.isActive() )
            m_timerTrayIconSnip.start(250);
    }

    const QIcon icon = appIcon(flags);

    setWindowIcon(icon);
    updateTrayIcon();
}

void MainWindow::updateIconTimeout()
{
    if ( !hasRunningAction() )
        updateIcon();
}

void MainWindow::updateContextMenuTimeout()
{
    ClipboardBrowser *c = getBrowser();

    if (c->editing())
        return;

    m_menuItem->addSeparator();

    cm->tabShortcuts()->setDisabledShortcuts(QList<QKeySequence>());

    addCommandsToMenu(m_menuItem, c->getSelectedItemData());

    addItemAction( Actions::Item_MoveToClipboard, c, SLOT(moveToClipboard()) );
    addItemAction( Actions::Item_ShowContent, c, SLOT(showItemContent()) );
    addItemAction( Actions::Item_Remove, c, SLOT(remove()) );
    addItemAction( Actions::Item_Edit, c, SLOT(editSelected()) );
    addItemAction( Actions::Item_EditNotes, c, SLOT(editNotes()) );
    addItemAction( Actions::Item_EditWithEditor, c, SLOT(openEditor()) );
    addItemAction( Actions::Item_Action, this, SLOT(action()) );

    updateToolBar();
}

void MainWindow::onAboutToQuit()
{
    cm->disconnect();
    cm->saveMainWindowState( objectName(), saveState() );
    ui->tabWidget->saveTabInfo();
    hideWindow();
    if (m_tray)
        m_tray->hide();

    m_itemMenuCommandTester.abort();
    m_trayMenuCommandTester.abort();
}

void MainWindow::onCommandDialogSaved()
{
    m_commands = loadCommands();
    updateContextMenu();
    emit commandsSaved();
}

void MainWindow::onSaveCommand(const Command &command)
{
    CommandDialog::Commands commands = loadCommands(false);
    commands.append(command);
    saveCommands(commands);

    if (m_commandDialog)
        m_commandDialog->addCommand(command);
}

void MainWindow::onCommandActionTriggered(const Command &command, const QVariantMap &data, int commandType)
{
    ClipboardBrowser *c = getBrowser();
    const QModelIndexList selected = c->selectionModel()->selectedIndexes();

    if ( !command.cmd.isEmpty() ) {
        bool triggeredFromBrowser = commandType == CommandAction::ItemCommand;
        if (command.transform) {
            foreach (const QModelIndex &index, selected) {
                QVariantMap data = itemData(index);
                if (triggeredFromBrowser)
                    data = addSelectionData(*c, data);
                if ( command.input.isEmpty() || hasFormat(data, command.input) )
                    action(data, command, index);
            }
        } else {
            const QVariantMap data2 =
                    triggeredFromBrowser ? addSelectionData(*c, data) : data;
            action(data2, command, QModelIndex());
        }
    }

    if ( !command.tab.isEmpty() && command.tab != c->tabName() ) {
        for (int i = selected.size() - 1; i >= 0; --i) {
            QVariantMap data = itemData(selected[i]);
            if ( !data.isEmpty() )
                emit addToTab(data, command.tab);
        }
    }

    if (command.remove) {
        const int lastRow = c->removeIndexes(selected);
        if (lastRow != -1)
            c->setCurrent(lastRow);
    }

    if (command.hideWindow)
        hideWindow();
}

void MainWindow::on_tabWidget_dropItems(const QString &tabName, QDropEvent *event)
{
    ClipboardBrowser *browser = createTab(tabName);

    if ( browser->isLoaded() ) {
        const QMimeData &data = *event->mimeData();
        const QVariantMap dataMap = data.hasFormat(mimeItems)
                ? cloneData(data, QStringList() << mimeItems) : cloneData(data);
        browser->paste(dataMap, 0);
        event->acceptProposedAction();
    }
}

void MainWindow::showContextMenu(const QPoint &position)
{
    m_menuItem->exec(position);
}

void MainWindow::updateContextMenu()
{
    m_itemMenuCommandTester.abort();

    foreach (QAction *action, m_menuItem->actions())
        action->deleteLater();

    foreach (QMenu *menu, m_menuItem->findChildren<QMenu*>())
        menu->deleteLater();

    m_menuItem->clear();
    m_timerUpdateContextMenu.start();
}

void MainWindow::action()
{
    ClipboardBrowser *c = browser();
    const QVariantMap data = c->getSelectedItemData();
    openActionDialog( data.isEmpty() ? createDataMap(mimeText, c->selectedText()) : data );
}

void MainWindow::automaticCommandTestFinished(const Command &command, bool passed)
{
    if (!passed)
        return;

    m_automaticCommands.append(command);

    if (!m_currentAutomaticCommand)
        runNextAutomaticCommand();
}

void MainWindow::automaticCommandFinished()
{
    Q_ASSERT(!m_currentAutomaticCommand);

    if (!m_automaticCommands.isEmpty()) {
        const Command command = m_automaticCommands.takeFirst();
        if (command.remove || command.transform) {
            m_automaticCommands.clear();
            COPYQ_LOG("Clipboard ignored by \"" + command.name + "\"");
        }
    }

    if (!m_automaticCommands.isEmpty())
        runNextAutomaticCommand();
}

void MainWindow::enableActionForCommand(QMenu *menu, const Command &command, bool enable)
{
    CommandAction *act = NULL;
    foreach (CommandAction *action, menu->findChildren<CommandAction*>()) {
        if (!action->isEnabled() && action->command().matchCmd == command.matchCmd) {
            act = action;
            break;
        }
    }

    Q_ASSERT(act);

    if (enable) {
        act->setEnabled(true);

        if (menu == m_menuItem) {
            QList<QKeySequence> shortcuts = act->shortcuts();
            if (!shortcuts.isEmpty()) {
                shortcuts << cm->tabShortcuts()->disabledShortcuts();
                cm->tabShortcuts()->setDisabledShortcuts(shortcuts);
            }

            updateToolBar();
        }
    } else {
        QMenu *menu = qobject_cast<QMenu*>(act->parentWidget());
        delete act;

        // Remove empty menus.
        while ( menu && menu->actions().isEmpty() ) {
            QMenu *parentMenu = qobject_cast<QMenu*>(menu->parentWidget());
            delete menu;
            menu = parentMenu;
        }
    }
}

void MainWindow::addCommandsToItemMenu(const Command &command, bool passed)
{
    enableActionForCommand(m_menuItem, command, passed);
}

void MainWindow::addCommandsToTrayMenu(const Command &command, bool passed)
{
    enableActionForCommand(m_trayMenu, command, passed);
}

void MainWindow::updateTitle(const QVariantMap &data)
{
    if (m_ignoreUpdateTitle)
        return;

    COPYQ_LOG("Updating window title");

    m_clipboardData = m_clipboardStoringDisabled ? QVariantMap() : data;

    updateWindowTitle();
    updateTrayTooltip();
    showClipboardMessage(m_clipboardData);
}

void MainWindow::updateNotifications()
{
    if (m_notifications == NULL)
        m_notifications = new NotificationDaemon(this);

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

    m_notifications->updateInterval(0, m_options.itemPopupInterval);

    m_notifications->updateNotifications();
}

void MainWindow::updateWindowTransparency(bool mouseOver)
{
    int opacity = 100 - (mouseOver || isActiveWindow() ? m_options.transparencyFocused : m_options.transparency);
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
    if ( m_options.activateFocuses() || m_options.activatePastes() )
        m_timerUpdateFocusWindows.start();
}

void MainWindow::setHideTabs(bool hide)
{
    ui->tabWidget->setTabBarHidden(hide);
}

bool MainWindow::closeMinimizes() const
{
    return !m_tray && !m_minimizeUnsupported;
}

NotificationDaemon *MainWindow::notificationDaemon()
{
    if (m_notifications == NULL)
        updateNotifications();

    return m_notifications;
}

ClipboardBrowser *MainWindow::createTab(const QString &name, bool *needSave)
{
    Q_ASSERT( !name.isEmpty() );

    int i = findTabIndex(name);

    if (needSave != NULL)
        *needSave = (i == -1);

    if (i != -1)
        return getBrowser(i);

    ClipboardBrowser *c = new ClipboardBrowser(this, m_sharedData);
    c->setTabName(name);
    c->loadSettings();

    connect( c, SIGNAL(changeClipboard(QVariantMap)),
             this, SLOT(setClipboard(QVariantMap)) );
    connect( c, SIGNAL(requestShow(const ClipboardBrowser*)),
             this, SLOT(showBrowser(const ClipboardBrowser*)) );
    connect( c, SIGNAL(error(QString)),
             this, SLOT(showError(QString)) );
    connect( c, SIGNAL(doubleClicked(QModelIndex)),
             this, SLOT(activateCurrentItem()) );
    connect( c, SIGNAL(addToTab(const QVariantMap,const QString)),
             this, SLOT(addToTab(const QVariantMap,const QString)) );
    connect( c, SIGNAL(itemCountChanged(QString,int)),
             ui->tabWidget, SLOT(setTabItemCount(QString,int)) );
    connect( c, SIGNAL(showContextMenu(QPoint)),
             this, SLOT(showContextMenu(QPoint)) );
    connect( c, SIGNAL(updateContextMenu()),
             this, SLOT(updateContextMenu()) );

    ui->tabWidget->addTab(c, name);

    return c;
}

QAction *MainWindow::createAction(Actions::Id id, const char *slot, QMenu *menu)
{
    ConfigTabShortcuts *shortcuts = cm->tabShortcuts();
    QAction *act = shortcuts->action(id, this, Qt::WindowShortcut);
    connect(act, SIGNAL(triggered()),
            this, slot, Qt::UniqueConnection);
    menu->addAction(act);
    return act;
}

QAction *MainWindow::addTrayAction(Actions::Id id)
{
    ConfigTabShortcuts *shortcuts = cm->tabShortcuts();
    QAction *act = shortcuts->action(id, NULL, Qt::WindowShortcut);
    m_trayMenu->addAction(act);
    return act;
}

void MainWindow::updateTabIcon(const QString &newName, const QString &oldName)
{
    const QString icon = cm->getIconNameForTabName(oldName);
    if ( !icon.isEmpty() ) {
        cm->setIconNameForTabName(oldName, QString());
        cm->setIconNameForTabName(newName, icon);
    }
}

QAction *MainWindow::addItemAction(Actions::Id id, QObject *receiver, const char *slot)
{
    QAction *act = cm->tabShortcuts()->action(id, getBrowser(), Qt::WidgetShortcut);
    connect( act, SIGNAL(triggered()), receiver, slot, Qt::UniqueConnection );
    m_menuItem->addAction(act);
    return act;
}

void MainWindow::addCommandsToMenu(QMenu *menu, const QVariantMap &data)
{
    if ( m_commands.isEmpty() )
        return;

    ClipboardBrowser *c = (menu == m_menuItem) ? getBrowser() : getTabForTrayMenu();
    const QString &tabName = c ? c->tabName() : QString();

    QList<Command> disabledCommands;
    QList<Command> commands;
    foreach (const Command &command, m_commands) {
        if ( command.inMenu && !command.name.isEmpty() && canExecuteCommand(command, data, tabName) ) {
            Command cmd = command;
            if ( cmd.outputTab.isEmpty() )
                cmd.outputTab = tabName;

            bool enabled = cmd.matchCmd.isEmpty();
            if (!enabled)
                disabledCommands.append(cmd);
            commands.append(cmd);
        }
    }

    CommandAction::Type type = (menu == m_menuItem)
            ? CommandAction::ItemCommand : CommandAction::ClipboardCommand;

    QList<QKeySequence> usedShortcuts = cm->tabShortcuts()->disabledShortcuts();

    foreach (const Command &command, commands) {
        QString name = command.name;
        QMenu *currentMenu = createSubMenus(&name, menu);
        QAction *act = new CommandAction(command, name, type, getBrowser(), currentMenu);
        currentMenu->addAction(act);

        if (!command.matchCmd.isEmpty()) {
            act->setDisabled(true);
        }

        if (type == CommandAction::ItemCommand) {
            QList<QKeySequence> uniqueShortcuts;

            foreach (const QString &shortcutText, command.shortcuts) {
                const QKeySequence shortcut(shortcutText, QKeySequence::PortableText);
                if ( !shortcut.isEmpty() && !usedShortcuts.contains(shortcut) ) {
                    if (act->isEnabled())
                        usedShortcuts.append(shortcut);
                    uniqueShortcuts.append(shortcut);
                }
            }

            if (!uniqueShortcuts.isEmpty()) {
                act->setShortcuts(uniqueShortcuts);
            }
        }

        connect(act, SIGNAL(triggerCommand(Command,QVariantMap,int)),
                this, SLOT(onCommandActionTriggered(Command,QVariantMap,int)));
    }

    cm->tabShortcuts()->setDisabledShortcuts(usedShortcuts);

    if (menu == m_menuItem) {
        m_itemMenuCommandTester.setCommands(disabledCommands, addSelectionData(*c, data));
        m_itemMenuCommandTester.start();
    } else {
        m_trayMenuCommandTester.setCommands(disabledCommands, data);
        m_trayMenuCommandTester.start();
    }
}

void MainWindow::updateToolBar()
{
    ui->toolBar->clear();

    if ( !ui->toolBar->isVisible() )
        return;

    foreach ( QAction *action, m_menuItem->actions() ) {
        if ( action->isSeparator() ) {
            ui->toolBar->addSeparator();
        } else if ( action->isEnabled() && !action->icon().isNull() ) {
            const QIcon icon = action->icon();
            const QString text = action->text().remove("&");
            const QString shortcut = action->shortcut().toString(QKeySequence::NativeText);
            const QString label = text + (shortcut.isEmpty() ? QString() : "\n[" + shortcut + "]");
            const QString tooltip = "<center>" + escapeHtml(text)
                    + (shortcut.isEmpty() ? QString() : "<br /><b>" + escapeHtml(shortcut) + "</b>") + "</center>";
            QAction *act = ui->toolBar->addAction( icon, label, action, SIGNAL(triggered()) );
            act->setToolTip(tooltip);
        }
    }
}

void MainWindow::updateWindowTitle()
{
    if (m_clipboardData.isEmpty()) {
        setWindowTitle("CopyQ");
    } else {
        const QString clipboardContent = textLabelForData(m_clipboardData);
        const QString sessionName = qApp->property("CopyQ_session_name").toString();
        const QString title = sessionName.isEmpty()
                ? tr("%1 - CopyQ",
                     "Main window title format (%1 is clipboard content label)")
                  .arg(clipboardContent)
                : tr("%1 - %2 - CopyQ",
                     "Main window title format (%1 is clipboard content label, %2 is session name)")
                  .arg(clipboardContent)
                  .arg(sessionName);

        setWindowTitle(title);
    }
}

void MainWindow::updateTrayTooltip()
{
    if (m_tray) {
        m_tray->setToolTip(
                    tr("Clipboard:\n%1", "Tray tooltip format")
                    .arg(textLabelForData(m_clipboardData)) );
    }
}

void MainWindow::updateTrayIcon()
{
    if (m_tray)
        m_tray->setIcon(windowIcon());
}

void MainWindow::initTray()
{
    if (m_tray) {
        // Hide tray on Ubuntu (buggy sni-qt)
        m_tray->hide();

        delete m_tray;
        m_tray = NULL;
    }

    if ( cm->value("disable_tray").toBool() )
        m_timerTrayAvailable.stop();
    else
        createTrayIfSupported();

    if (!m_tray) {
        if (m_timerMinimizing.interval() == 0) {
            // Check if window manager can minimize window properly.
            // If window is activated while minimizing, assume that minimizing is not supported.
            initSingleShotTimer( &m_timerMinimizing, 1000 );
            QApplication::processEvents();
            m_timerMinimizing.start();
            showMinimized();
        } else if (isHidden() && !isMinimized()) {
            showMinimized();
        }
    }
}

void MainWindow::runNextAutomaticCommand()
{
    Q_ASSERT(!m_automaticCommands.isEmpty());
    Q_ASSERT(!m_currentAutomaticCommand);

    const Command &command = m_automaticCommands.first();

    const QVariantMap &data = m_automaticCommandTester.data();

    if ( command.input.isEmpty()
         || command.input == mimeItems
         || data.contains(command.input) )
    {
        m_currentAutomaticCommand = action(data, command);
    }

    if (!command.tab.isEmpty())
        addToTab(data, command.tab);

    if (m_currentAutomaticCommand)
        connect(m_currentAutomaticCommand, SIGNAL(destroyed()), SLOT(automaticCommandFinished()));
    else
        automaticCommandFinished();
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
    if ( m_options.itemPopupInterval != 0 && m_options.clipboardNotificationLines > 0) {
        if (data.isEmpty()) {
            notificationDaemon()->removeNotification(clipboardNotificationId);
        } else {
            notificationDaemon()->create( data, m_options.clipboardNotificationLines, IconPaste,
                                          m_options.itemPopupInterval * 1000, clipboardNotificationId );
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

    const int key = event->key();
    const Qt::KeyboardModifiers modifiers = event->modifiers();

    if (m_options.hideTabs && key == Qt::Key_Alt)
        setHideTabs(false);

    if (browseMode() && m_options.viMode) {
        if (c->handleViKey(event))
            return;
        switch(key) {
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

    // Ctrl/Alt+0 to Ctrl/Alt+9 to focus tabs (0 to focus the last, 1 to focus the first and so on).
    if (modifiers == Qt::ControlModifier || modifiers == Qt::AltModifier) {
        if (key >= Qt::Key_0 && key <= Qt::Key_9) {
            const int index = (key == Qt::Key_0) ? ui->tabWidget->count() - 1
                                                 : key - Qt::Key_1;
            ui->tabWidget->setCurrentIndex(index);
            return;
        }
    }

    if ( key == Qt::Key_F3 || (modifiers.testFlag(Qt::ControlModifier) && key == Qt::Key_G) ) {
        if ( modifiers.testFlag(Qt::ShiftModifier) )
            findPrevious();
        else
            findNext();
        return;
    }

    if (modifiers == Qt::ControlModifier) {
        switch(key) {
            case Qt::Key_F:
                enterBrowseMode(false);
                return;
            case Qt::Key_Tab:
                nextTab();
                return;
            case Qt::Key_Backtab:
                previousTab();
                return;
            case Qt::Key_Return:
            case Qt::Key_Enter:
                if ( c->isLoaded() )
                    activateCurrentItem();
                return;
            default:
                QMainWindow::keyPressEvent(event);
                break;
        }
        return;
    }

    switch(key) {
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
                hideWindow();
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
    if (m_options.hideTabs && event->key() == Qt::Key_Alt)
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
        setHideTabs(m_options.hideTabs);
    } else if (type == QEvent::WindowActivate) {
        if ( m_timerMinimizing.isActive() ) {
            // Window manager ignores window minimizing -- hide it instead.
            m_minimizeUnsupported = true;
            hide();
            return true;
        }

        updateWindowTransparency();
    } else if (type == QEvent::WindowDeactivate) {
        m_timerShowWindow.start();
        m_lastWindow.clear();
        updateWindowTransparency();
        setHideTabs(m_options.hideTabs);
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

    ConfigTabAppearance *appearance = cm->tabAppearance();
    appearance->decorateToolBar(ui->toolBar);
    appearance->decorateMainWindow(this);

    m_options.confirmExit = cm->value("confirm_exit").toBool();

    // always on top window hint
    bool alwaysOnTop = cm->value("always_on_top").toBool();
    bool hasAlwaysOnTop = windowFlags().testFlag(Qt::WindowStaysOnTopHint);
    if (alwaysOnTop != hasAlwaysOnTop) {
        setWindowFlags(windowFlags() ^ Qt::WindowStaysOnTopHint);
        // Workaround for QTBUG-28601.
        setAcceptDrops(true);
    }

    // Vi mode
    m_options.viMode = cm->value("vi").toBool();

    m_options.transparency = qMax( 0, qMin(100, cm->value("transparency").toInt()) );
    m_options.transparencyFocused = qMax( 0, qMin(100, cm->value("transparency_focused").toInt()) );
    updateWindowTransparency();

    // tab bar position
    ui->tabWidget->setTreeModeEnabled(cm->value("tab_tree").toBool());
    ui->tabWidget->setTabItemCountVisible(cm->value("show_tab_item_count").toBool());

    m_options.hideTabs = cm->value("hide_tabs").toBool();
    setHideTabs(m_options.hideTabs);

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

    updateContextMenu();

    m_options.itemActivationCommands = ActivateNoCommand;
    if ( cm->value("activate_closes").toBool() )
        m_options.itemActivationCommands |= ActivateCloses;
    if ( cm->value("activate_focuses").toBool() )
        m_options.itemActivationCommands |= ActivateFocuses;
    if ( cm->value("activate_pastes").toBool() )
        m_options.itemActivationCommands |= ActivatePastes;

    m_options.trayItems = cm->value("tray_items").toInt();
    m_options.trayItemPaste = cm->value("tray_item_paste").toBool();
    m_options.trayCommands = cm->value("tray_commands").toBool();
    m_options.trayCurrentTab = cm->value("tray_tab_is_current").toBool();
    m_options.trayTabName = cm->value("tray_tab").toString();
    m_options.trayImages = cm->value("tray_images").toBool();
    m_options.itemPopupInterval = cm->value("item_popup_interval").toInt();
    m_options.clipboardNotificationLines = cm->value("clipboard_notification_lines").toInt();

    m_trayMenu->setStyleSheet( cm->tabAppearance()->getToolTipStyleSheet() );

    initTray();

    if (m_notifications != NULL)
        updateNotifications();

    updateIcon();

    ui->searchBar->loadSettings();

    m_lastWindow.clear();

    COPYQ_LOG("Configuration loaded");
}

void MainWindow::showWindow()
{
    if ( m_timerMinimizing.isActive() )
        return;

    if ( isActiveWindow() )
        return;

#ifdef COPYQ_WS_X11
    /* Re-initialize window in window manager so it can popup on current workspace. */
    hide();
#endif

    cm->restoreWindowGeometry(this);

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

    stealFocus(*this);
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
    if ( m_timerShowWindow.isActive() || isActiveWindow() ) {
        hideWindow();
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

void MainWindow::onTrayActionTriggered(uint clipboardItemHash, bool omitPaste)
{
    ClipboardBrowser *c = getTabForTrayMenu();

    if ( c->select(clipboardItemHash, MoveToClipboard) && m_lastWindow && !omitPaste && canPaste() ) {
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

void MainWindow::tabChanged(int current, int)
{
    bool currentIsTabGroup = current == -1;

    emit tabGroupSelected(currentIsTabGroup);

    if (currentIsTabGroup) {
        m_actionHandler->setCurrentTab(QString());
        return;
    }

    // update item menu (necessary for keyboard shortcuts to work)
    ClipboardBrowser *c = getBrowser();

    c->filterItems( ui->searchBar->filter() );

    if ( current >= 0 ) {
        if( !c->currentIndex().isValid() && isVisible() ) {
            c->setCurrent(0);
        }
    }

    setTabOrder(ui->searchBar, c);

    m_actionHandler->setCurrentTab(c->tabName());

    updateContextMenu();
}

void MainWindow::saveTabPositions()
{
    cm->setTabs( ui->tabWidget->tabs() );
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
            if ( c->isLoaded() )
                ui->tabWidget->setTabItemCount( newName, c->length() );
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

void MainWindow::addToTab(const QVariantMap &data, const QString &tabName)
{
    createTab(tabName)->add(data);
}

void MainWindow::updateFirstItem(const QVariantMap &data)
{
    ClipboardBrowser *c = browser(0);

    if ( !c->isLoaded() )
        return;

    if ( c->select(hash(data), MoveToTop) ) {
        COPYQ_LOG("Clipboard item: Moving to top");
        return;
    }

    QVariantMap newData = data;
    bool reselectFirst = false;

    // When selecting text under X11, clipboard data may change whenever selection changes.
    // Instead of adding item for each selection change, this updates previously added item.
    if ( newData.contains(mimeText) ) {
        const QModelIndex firstIndex = c->model()->index(0, 0);
        const QVariantMap previousData = itemData(firstIndex);

        if ( previousData.contains(mimeText)
             && newData.value(mimeWindowTitle) == previousData.value(mimeWindowTitle)
             && getTextData(newData).contains(getTextData(previousData))
             )
        {
            COPYQ_LOG("Clipboard item: Merging with top item");

            const QSet<QString> formatsToAdd = previousData.keys().toSet() - newData.keys().toSet();

            foreach (const QString &format, formatsToAdd)
                newData.insert(format, previousData[format]);

            // Remove merged item (if it's not edited).
            if (!c->editing() || c->currentIndex().row() != 0) {
                reselectFirst = c->currentIndex().row() == 0;
                c->model()->removeRow(0);
            }
        }
    }

    COPYQ_LOG("Clipboard item: Adding");

    c->add(newData);

    if (reselectFirst)
        c->setCurrent(0);
}

void MainWindow::runAutomaticCommands(const QVariantMap &data)
{
    QList<Command> commands;
    const QString &tabName = getBrowser(0)->tabName();
    foreach (const Command &command, m_commands) {
        if (command.automatic && canExecuteCommand(command, data, tabName)) {
            commands.append(command);
            if ( command.outputTab.isEmpty() )
                commands.last().outputTab = tabName;
        }
    }

    // Clear window title and tooltip.
    commands.prepend(automaticCommand("Reset Window Title" ,"updateTitle('')"));

    // Add new clipboard to the first tab (if configured so).
    if ( cm->value("check_clipboard").toBool() )
        commands.append(automaticCommand("Add Item from Clipboard", "updateFirst()"));

#ifdef COPYQ_WS_X11
    // Add new mouse selection to the first tab (if configured so).
    if ( cm->value("check_selection").toBool() )
        commands.append(automaticCommand("Add Item from Selection", "updateFirst()", false));

    if ( cm->value("copy_clipboard").toBool() )
        commands.append(automaticCommand("Clipboard -> Selection", "try { copySelection(input()) } catch (e) {}", true, "text/plain"));

    if ( cm->value("copy_selection").toBool() )
        commands.append(automaticCommand("Selection -> Clipboard" , "try { copy(input()) } catch (e) {}", false, "text/plain"));
#endif

    // Set window title, tooltip and show notification.
    commands.append(automaticCommand("Set Window Title", "updateTitle()"));

    abortAutomaticCommands();
    m_automaticCommandTester.setCommands(commands, data);

    if (m_currentAutomaticCommand) {
        connect(m_currentAutomaticCommand, SIGNAL(destroyed()),
                &m_automaticCommandTester, SLOT(start()));
    } else {
        m_automaticCommandTester.start();
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
    // Don't process the data further if any running clipboard monitor set the clipboard.
    if ( !ownsClipboardData(data) && !data.contains(mimeOwner) && containsAnyData(data) ) {
        m_ignoreUpdateTitle = false;
        runAutomaticCommands(data);
    } else {
        updateTitle(data);
        m_ignoreUpdateTitle = true;
    }
}

void MainWindow::setClipboard(const QVariantMap &data, QClipboard::Mode mode)
{
    emit changeClipboard(data, mode);
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

    if ( m_options.activateCloses() )
        hideWindow();

    if (lastWindow) {
        if (m_options.activateFocuses())
            lastWindow->raise();

        if (m_options.activatePastes() && canPaste()) {
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
        clearTitle();

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

void MainWindow::abortAutomaticCommands()
{
    m_automaticCommands.clear();
    m_automaticCommandTester.abort();

    if (m_currentAutomaticCommand) {
        COPYQ_LOG("Aborting automatic commands (current is \"" + m_currentAutomaticCommand->name() + "\")");
        disconnect(m_currentAutomaticCommand, SIGNAL(destroyed()),
                   this, SLOT(automaticCommandFinished()));
    }
}

QStringList MainWindow::tabs() const
{
    return ui->tabWidget->tabs();
}

ClipboardBrowser *MainWindow::getTabForTrayMenu()
{
    if (m_trayTab)
        return m_trayTab;

    if (m_options.trayCurrentTab)
        return browser();

    if ( m_options.trayTabName.isEmpty() )
        return browser(0);

    int i = findTabIndex(m_options.trayTabName);
    return i != -1 ? browser(i) : NULL;
}

void MainWindow::onFilterChanged(const QRegExp &re)
{
    enterBrowseMode( re.isEmpty() );
    browser()->filterItems(re);
}

void MainWindow::createTrayIfSupported()
{
    if ( QSystemTrayIcon::isSystemTrayAvailable() ) {
        Q_ASSERT(!m_tray);
        m_tray = new QSystemTrayIcon(this);
        connect( m_tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
                 this, SLOT(trayActivated(QSystemTrayIcon::ActivationReason)) );
        updateTrayIcon();
        updateTrayTooltip();
        m_tray->setContextMenu(m_trayMenu);
        m_tray->show();

        if ( isMinimized() )
            hideWindow();

        updateIcon();
    } else {
        m_timerTrayAvailable.start();
    }
}

void MainWindow::updateFocusWindows()
{
    if ( isActiveWindow() || (!m_options.activateFocuses() && !m_options.activatePastes()) )
        return;

    PlatformPtr platform = createPlatformNativeInterface();

    if ( m_options.activatePastes() || m_options.activateFocuses() ) {
        PlatformWindowPtr lastWindow = platform->getCurrentWindow();
        if (lastWindow)
            m_lastWindow = lastWindow;
    }
}

void MainWindow::enterSearchMode(const QString &txt)
{
    enterBrowseMode(false);
    ui->searchBar->setText( ui->searchBar->text() + txt );
}

void MainWindow::findNext(int where)
{
    if (browseMode()) {
        enterBrowseMode(false);
    } else {
        ClipboardBrowser *c = browser();
        if ( c->hasFocus() )
            c->setCurrent( c->currentIndex().row() + where );
        else
            c->setFocus();
    }
}

void MainWindow::findPrevious()
{
    findNext(-1);
}

void MainWindow::enterBrowseMode(bool browsemode)
{
    if (browsemode) {
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
    if (m_options.trayItemPaste)
        m_lastWindow = createPlatformNativeInterface()->getCurrentWindow();

    ClipboardBrowser *c = getTabForTrayMenu();

    m_trayMenuCommandTester.abort();

    m_trayMenu->clearClipboardItemActions();
    m_trayMenu->clearCustomActions();

    // Add items.
    const int len = (c != NULL) ? qMin( m_options.trayItems, c->length() ) : 0;
    const int current = c->currentIndex().row();
    for ( int i = 0; i < len; ++i ) {
        const QModelIndex index = c->model()->index(i, 0);
        m_trayMenu->addClipboardItemAction(index, m_options.trayImages, i == current);
    }

    // Add commands.
    if (m_options.trayCommands) {
        if (c == NULL)
            c = browser(0);

        // Show clipboard content as disabled item.
        const QString format = tr("&Clipboard: %1", "Tray menu clipboard item format");
        QAction *act = m_trayMenu->addAction( iconClipboard(),
                                            QString(), this, SLOT(showClipboardContent()) );
        act->setText( textLabelForData(m_clipboardData, act->font(), format, true) );
        m_trayMenu->addCustomAction(act);

        int i = m_trayMenu->actions().size();
        addCommandsToMenu(m_trayMenu, m_clipboardData);
        QList<QAction *> actions = m_trayMenu->actions();
        for ( ; i < actions.size(); ++i )
            m_trayMenu->addCustomAction(actions[i]);
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
    QScopedPointer<ClipboardDialog> clipboardDialog(new ClipboardDialog(this));
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

WId MainWindow::openActionDialog(const QVariantMap &data)
{
    QScopedPointer<ActionDialog> actionDialog( m_actionHandler->createActionDialog(ui->tabWidget->tabs()) );
    connect( actionDialog.data(), SIGNAL(saveCommand(Command)),
             this, SLOT(onSaveCommand(Command)) );
    actionDialog->setWindowIcon(appIcon(AppIconRunning));
    actionDialog->setInputData( addSelectionData(*browser(), data) );
    actionDialog->show();
    return stealFocus(*actionDialog.take());
}

void MainWindow::openPreferences()
{
    if ( !isEnabled() )
        return;

    cm->exec();
}

void MainWindow::openCommands()
{
    if ( !isEnabled() )
        return;

    if (m_commandDialog) {
        m_commandDialog->show();
        m_commandDialog->activateWindow();
    } else {
        m_commandDialog = new CommandDialog(this);
        m_commandDialog->setAttribute(Qt::WA_DeleteOnClose, true);
        m_commandDialog->show();
        connect(m_commandDialog, SIGNAL(commandsSaved()), this, SLOT(onCommandDialogSaved()));
    }
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
        ClipboardBrowser *c = qobject_cast<ClipboardBrowser*>(index.model()->parent());
        Q_ASSERT(c);
        c->loadItems();
        if (c->isLoaded())
            return c;
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

    QVariantMap data = c->copyIndexes(indexes);
    if ( indexes.size() == 1 ) {
        QVariantMap data2 = itemData(indexes.first());
        data2.remove(mimeItems);
        data.unite(data2);
    }

    setClipboard(data);
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

Action *MainWindow::action(const QVariantMap &data, const Command &cmd, const QModelIndex &outputIndex)
{
    if (cmd.wait) {
        QScopedPointer<ActionDialog> actionDialog( m_actionHandler->createActionDialog(ui->tabWidget->tabs()) );

        actionDialog->setInputData(addSelectionData(*browser(), data));
        actionDialog->setCommand(cmd);
        actionDialog->setOutputIndex(outputIndex);
        QString outputTab = cmd.outputTab;

        // Insert tab labels to action dialog's combo box.
        QStringList tabs;
        TabWidget *w = ui->tabWidget;
        for( int i = 0; i < w->count(); ++i )
            tabs << w->tabText(i);
        if ( outputTab.isEmpty() && w->currentIndex() > 0 )
            outputTab = w->tabText( w->currentIndex() );
        actionDialog->setOutputTabs(tabs, outputTab);

        actionDialog->show();
        actionDialog.take();
    } else if ( cmd.cmd.isEmpty() ) {
        m_actionHandler->addFinishedAction(cmd.name);
    } else {
        Action *act = new Action();
        act->setCommand( cmd.cmd, QStringList(getTextData(data)) );
        act->setInput(data, cmd.input);
        act->setOutputFormat(cmd.output);
        act->setItemSeparator(QRegExp(cmd.sep));
        act->setOutputTab(cmd.outputTab);
        act->setIndex(outputIndex);
        act->setName(cmd.name);
        act->setData(data);
        m_actionHandler->action(act);
        return act;
    }

    return NULL;
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
    IconSelectDialog dialog( cm->getIconNameForTabName(tabName), this );

    if ( dialog.exec() == QDialog::Accepted )
        setTabIcon(tabName, dialog.selectedIcon());
}

void MainWindow::setTabIcon(const QString &tabName, const QString &icon)
{
    if ( tabs().contains(tabName) ) {
        cm->setIconNameForTabName(tabName, icon);
        ui->tabWidget->updateTabIcon(tabName);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}
