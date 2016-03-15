/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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
#include "common/appconfig.h"
#include "common/common.h"
#include "common/command.h"
#include "common/config.h"
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
#include "gui/configurationmanager.h"
#include "gui/iconfactory.h"
#include "gui/iconfactory.h"
#include "gui/iconselectdialog.h"
#include "gui/icons.h"
#include "gui/logdialog.h"
#include "gui/notification.h"
#include "gui/notificationdaemon.h"
#include "gui/tabdialog.h"
#include "gui/tabicons.h"
#include "gui/tabwidget.h"
#include "gui/theme.h"
#include "gui/traymenu.h"
#include "gui/windowgeometryguard.h"
#include "item/itemfactory.h"
#include "item/serialize.h"
#include "platform/platformnativeinterface.h"
#include "platform/platformwindow.h"

#ifdef Q_OS_MAC
#  include "platform/mac/foregroundbackgroundfilter.h"
#endif

#include <QAction>
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

namespace {

const QIcon iconClipboard() { return getIcon("clipboard", IconPaste); }
const QIcon iconTabIcon() { return getIconFromResources("tab_icon"); }
const QIcon iconTabNew() { return getIconFromResources("tab_new"); }
const QIcon iconTabRemove() { return getIconFromResources("tab_remove"); }
const QIcon iconTabRename() { return getIconFromResources("tab_rename"); }

const int clipboardNotificationId = 0;

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

Command automaticCommand(const QString &name, const QString &cmd)
{
    Command c;
    c.name = name;
    c.automatic = true;
    c.cmd = "copyq: if (monitoring()) {" + cmd + "}";

    return c;
}

void setAlwaysOnTop(QWidget *window, bool alwaysOnTop)
{
    if (!window)
        return;

    const Qt::WindowFlags flags = window->windowFlags();
    bool hasAlwaysOnTop = flags.testFlag(Qt::WindowStaysOnTopHint);

    if (alwaysOnTop != hasAlwaysOnTop) {
        window->setWindowFlags(flags ^ Qt::WindowStaysOnTopHint);
        // Workaround for QTBUG-28601.
        window->setAcceptDrops(true);
    }
}

bool isClipboardDataHidden(const QVariantMap &data)
{
    return data.value(mimeHidden).toByteArray() == "1";
}

#ifdef COPYQ_WS_X11
bool needSyncClipboardToSelection(const QVariantMap &data)
{
    return isClipboardData(data)
            && AppConfig().option<Config::copy_clipboard>()
            && !clipboardContains(QClipboard::Selection, data);
}

bool needSyncSelectionToClipboard(const QVariantMap &data)
{
    return !isClipboardData(data)
            && AppConfig().option<Config::copy_selection>()
            && !clipboardContains(QClipboard::Clipboard, data);
}

bool needStore(const QVariantMap &data)
{
    return isClipboardData(data)
            ? AppConfig().option<Config::check_clipboard>()
            : AppConfig().option<Config::check_selection>();
}
#else
bool needSyncClipboardToSelection(const QVariantMap &)
{
    return false;
}

bool needSyncSelectionToClipboard(const QVariantMap &)
{
    return false;
}

bool needStore(const QVariantMap &data)
{
    return isClipboardData(data)
            && AppConfig().option<Config::check_clipboard>();
}
#endif

template <typename Dialog>
Dialog *openDialog(QWidget *dialogParent)
{
    QScopedPointer<Dialog> dialog( new Dialog(dialogParent) );
    WindowGeometryGuard::create( dialog.data() );
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->setWindowIcon(appIcon());
    dialog->activateWindow();
    dialog->show();
    return dialog.take();
}

QString defaultTabName()
{
    const QString tab = AppConfig().option<Config::clipboard_tab>();
    return tab.isEmpty() ? defaultClipboardTabName() : tab;
}

void loadItemFactorySettings(ItemFactory *itemFactory)
{
    QSettings settings;

    // load settings for each plugin
    settings.beginGroup("Plugins");
    foreach ( ItemLoaderInterface *loader, itemFactory->loaders() ) {
        settings.beginGroup(loader->id());

        QVariantMap s;
        foreach (const QString &name, settings.allKeys()) {
            s[name] = settings.value(name);
        }
        loader->loadSettings(s);
        itemFactory->setLoaderEnabled( loader, settings.value("enabled", true).toBool() );

        settings.endGroup();
    }
    settings.endGroup();

    // load plugin priority
    const QStringList pluginPriority =
            settings.value("plugin_priority", QStringList()).toStringList();
    itemFactory->setPluginPriority(pluginPriority);
}

bool isItemActivationShortcut(const QKeySequence &shortcut)
{
    return (shortcut[0] == Qt::Key_Return || shortcut[0] == Qt::Key_Enter)
            && shortcut[1] == 0
            && shortcut[2] == 0
            && shortcut[3] == 0;
}

} // namespace

MainWindow::MainWindow(ItemFactory *itemFactory, QWidget *parent)
    : QMainWindow(parent)
    , cm(NULL)
    , ui(new Ui::MainWindow)
    , m_menuItem(NULL)
    , m_trayMenu( new TrayMenu(this) )
    , m_tray(NULL)
    , m_clipboardStoringDisabled(false)
    , m_actionToggleClipboardStoring()
    , m_sharedData(new ClipboardBrowserShared(itemFactory))
    , m_lastWindow()
    , m_notifications(NULL)
    , m_actionHandler(new ActionHandler(this))
    , m_trayTab(NULL)
    , m_commandDialog(NULL)
    , m_canUpdateTitleFromScript(true)
    , m_iconSnip(false)
    , m_wasMaximized(false)
    , m_menuItems(menuItems())
{
    ui->setupUi(this);
    menuBar()->setObjectName("menu_bar");
    createMenu();

    ui->tabWidget->addToolBars(this);
    addToolBar(Qt::RightToolBarArea, ui->toolBar);

    WindowGeometryGuard::create(this);
    restoreState( mainWindowState(objectName()) );

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
    connect( m_actionHandler, SIGNAL(runningActionsCountChanged()),
             this, SLOT(updateIconSnip()) );
    connect( qApp, SIGNAL(aboutToQuit()),
             this, SLOT(onAboutToQuit()) );
    connect( this, SIGNAL(configurationChanged()),
             this, SLOT(loadSettings()) );

    connect(&m_itemMenuCommandTester, SIGNAL(commandPassed(Command,bool)),
            SLOT(addCommandsToItemMenu(Command,bool)));
    connect(&m_trayMenuCommandTester, SIGNAL(commandPassed(Command,bool)),
            SLOT(addCommandsToTrayMenu(Command,bool)));
    connect(&m_automaticCommandTester, SIGNAL(commandPassed(Command,bool)),
            SLOT(automaticCommandTestFinished(Command,bool)));

    connect(itemFactory, SIGNAL(error(QString)),
            this, SLOT(showError(QString)));

    m_commands = loadCommands();
    loadSettings();

    ui->tabWidget->setCurrentIndex(0);

    initSingleShotTimer( &m_timerUpdateFocusWindows, 50, this, SLOT(updateFocusWindows()) );
    initSingleShotTimer( &m_timerUpdateContextMenu, 0, this, SLOT(updateContextMenuTimeout()) );
    initSingleShotTimer( &m_timerShowWindow, 250 );
    initSingleShotTimer( &m_timerTrayAvailable, 1000, this, SLOT(createTrayIfSupported()) );
    initSingleShotTimer( &m_timerTrayIconSnip, 250, this, SLOT(updateIconSnipTimeout()) );

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
    COPYQ_LOG("Got main window close event.");

    if ( closeMinimizes() ) {
        event->ignore();
        minimizeWindow();
    } else {
        QMainWindow::closeEvent(event);
    }
}

void MainWindow::showEvent(QShowEvent *event)
{
    m_timerShowWindow.start();
    QMainWindow::showEvent(event);
}

void MainWindow::hideEvent(QHideEvent *event)
{
    QMainWindow::hideEvent(event);
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

    // - separator
    menu->addSeparator();

    // - next tab
    createAction( Actions::Tabs_NextTab, SLOT(nextTab()), menu );

    // - previous tab
    createAction( Actions::Tabs_PreviousTab, SLOT(previousTab()), menu );

    // Help
    menu = menubar->addMenu(tr("&Help"));
    createAction( Actions::Help_ShowLog, SLOT(openLogDialog()), menu );
    createAction( Actions::Help_Help, SLOT(openAboutDialog()), menu );
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
    const QIcon icon = appIcon(m_iconSnip ? AppIconRunning : AppIconNormal);
    setWindowIcon(icon);
    updateTrayIcon();
}

void MainWindow::updateIconSnipTimeout()
{
    const bool shouldSnip = hasRunningAction() || m_clipboardStoringDisabled;
    if (m_iconSnip != shouldSnip) {
        m_iconSnip = shouldSnip;
        m_timerTrayIconSnip.start(250);
        updateIcon();
    }
}

void MainWindow::updateContextMenuTimeout()
{
    ClipboardBrowser *c = getBrowser();

    if (c->editing())
        return;

    setDisabledShortcuts(QList<QKeySequence>());

    addCommandsToMenu(m_menuItem, c->getSelectedItemData());

    m_menuItem->addSeparator();

    addItemAction( Actions::Item_MoveToClipboard, c, SLOT(moveToClipboard()) );
    addItemAction( Actions::Item_ShowContent, c, SLOT(showItemContent()) );
    addItemAction( Actions::Item_Remove, c, SLOT(remove()) );
    addItemAction( Actions::Item_Edit, c, SLOT(editSelected()) );
    addItemAction( Actions::Item_EditNotes, c, SLOT(editNotes()) );
    addItemAction( Actions::Item_EditWithEditor, c, SLOT(openEditor()) );
    addItemAction( Actions::Item_Action, this, SLOT(action()) );
    addItemAction( Actions::Item_NextFormat, this, SLOT(nextItemFormat()) );
    addItemAction( Actions::Item_PreviousFormat, this, SLOT(previousItemFormat()) );

    m_menuItem->addSeparator();

    addItemAction( Actions::Item_MoveUp, this, SLOT(moveUp()) );
    addItemAction( Actions::Item_MoveDown, this, SLOT(moveDown()) );
    addItemAction( Actions::Item_MoveToTop, this, SLOT(moveToTop()) );
    addItemAction( Actions::Item_MoveToBottom, this, SLOT(moveToBottom()) );

    updateToolBar();
}

void MainWindow::updateIconSnip()
{
    if ( !m_timerTrayIconSnip.isActive() ) {
        m_iconSnip = !m_iconSnip;
        m_timerTrayIconSnip.start(250);
        updateIcon();
    }
}

void MainWindow::onAboutToQuit()
{
    if (cm)
        cm->disconnect();

    saveMainWindowState( objectName(), saveState() );
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
                addToTab(data, command.tab);
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
    ClipboardBrowser *browser = tab(tabName);

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

    foreach (QMenu *menu, m_menuItem->findChildren<QMenu*>())
        menu->deleteLater();

    foreach (QAction *action, m_menuItem->actions())
        action->deleteLater();

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
    if (passed)
        runAutomaticCommand(command);
    else
        m_automaticCommandTester.start();
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
            const QList<QKeySequence> shortcuts = act->shortcuts();
            if (!shortcuts.isEmpty())
                setDisabledShortcuts(m_disabledShortcuts + shortcuts);

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
    m_itemMenuCommandTester.start();
}

void MainWindow::addCommandsToTrayMenu(const Command &command, bool passed)
{
    enableActionForCommand(m_trayMenu, command, passed);
    m_trayMenuCommandTester.start();
}

void MainWindow::nextItemFormat()
{
    browser()->otherItemLoader(true);
}

void MainWindow::previousItemFormat()
{
    browser()->otherItemLoader(false);
}

void MainWindow::moveUp()
{
    browser()->move(Qt::Key_Up);
}

void MainWindow::moveDown()
{
    browser()->move(Qt::Key_Down);
}

void MainWindow::moveToTop()
{
    browser()->move(Qt::Key_Home);
}

void MainWindow::moveToBottom()
{
    browser()->move(Qt::Key_End);
}

int MainWindow::findTabIndexExactMatch(const QString &name)
{
    TabWidget *w = ui->tabWidget;

    for( int i = 0; i < w->count(); ++i ) {
        if ( name == w->tabText(i) )
            return i;
    }

    return -1;
}

void MainWindow::updateTitle(const QVariantMap &data)
{
    COPYQ_LOG("Updating window title");

    m_clipboardData = m_clipboardStoringDisabled ? QVariantMap() : data;

    updateWindowTitle();
    updateTrayTooltip();
    showClipboardMessage(m_clipboardData);
}

bool MainWindow::canUpdateTitleFromScript() const
{
    return m_canUpdateTitleFromScript;
}

void MainWindow::setFilter(const QString &text)
{
    ui->searchBar->setText(text);
}

void MainWindow::updateNotifications()
{
    if (m_notifications == NULL)
        m_notifications = new NotificationDaemon(this);

    const Theme theme;
    notificationDaemon()->setNotificationOpacity( theme.color("notification_bg").alphaF() );
    notificationDaemon()->setNotificationStyleSheet( theme.getNotificationStyleSheet() );

    AppConfig appConfig;
    int id = appConfig.option<Config::notification_position>();
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

    const int x = appConfig.option<Config::notification_horizontal_offset>();
    const int y = appConfig.option<Config::notification_vertical_offset>();
    m_notifications->setOffset(x, y);

    const int w = appConfig.option<Config::notification_maximum_width>();
    const int h = appConfig.option<Config::notification_maximum_height>();
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
                    getIcon("", m_clipboardStoringDisabled ? IconCheck : IconBan));
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
    return !m_tray;
}

NotificationDaemon *MainWindow::notificationDaemon()
{
    if (m_notifications == NULL)
        updateNotifications();

    return m_notifications;
}

ClipboardBrowser *MainWindow::createTab(
        const QString &name, TabNameMatching nameMatch, bool *needSave)
{
    Q_ASSERT( !name.isEmpty() );

    const int i = nameMatch == MatchExactTabName
            ? findTabIndexExactMatch(name)
            : findTabIndex(name);

    if (needSave != NULL)
        *needSave = (i == -1);

    if (i != -1)
        return getBrowser(i);

    ClipboardBrowser *c = new ClipboardBrowser(m_sharedData, this);
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
    connect( c, SIGNAL(itemCountChanged(QString,int)),
             ui->tabWidget, SLOT(setTabItemCount(QString,int)) );
    connect( c, SIGNAL(showContextMenu(QPoint)),
             this, SLOT(showContextMenu(QPoint)) );
    connect( c, SIGNAL(updateContextMenu()),
             this, SLOT(updateContextMenu()) );

    ui->tabWidget->addTab(c, name);

    return c;
}

QAction *MainWindow::createAction(int id, const char *slot, QMenu *menu)
{
    QAction *act = actionForMenuItem(id, this, Qt::WindowShortcut);
    connect(act, SIGNAL(triggered()),
            this, slot, Qt::UniqueConnection);
    menu->addAction(act);
    return act;
}

QAction *MainWindow::addTrayAction(int id)
{
    QAction *act = actionForMenuItem(id, m_trayMenu, Qt::WindowShortcut);
    m_trayMenu->addAction(act);
    return act;
}

void MainWindow::updateTabIcon(const QString &newName, const QString &oldName)
{
    const QString icon = getIconNameForTabName(oldName);
    if ( !icon.isEmpty() )
        setIconNameForTabName(newName, icon);
}

QAction *MainWindow::addItemAction(int id, QObject *receiver, const char *slot)
{
    QAction *act = actionForMenuItem(id, getBrowser(), Qt::WidgetShortcut);
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

    QList<QKeySequence> usedShortcuts = m_disabledShortcuts;

    if (menu == m_menuItem)
        m_activateCurrentItemAction = NULL;

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

                    if (menu == m_menuItem
                            && m_activateCurrentItemAction.isNull()
                            && isItemActivationShortcut(shortcut))
                    {
                        m_activateCurrentItemAction = act;
                        menu->setDefaultAction(act);
                    }
                }
            }

            if (!uniqueShortcuts.isEmpty()) {
                act->setShortcuts(uniqueShortcuts);
            }
        }

        connect(act, SIGNAL(triggerCommand(Command,QVariantMap,int)),
                this, SLOT(onCommandActionTriggered(Command,QVariantMap,int)));
    }

    setDisabledShortcuts(usedShortcuts);

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

    if ( ui->toolBar->isHidden() )
        return;

    QAction *act = actionForMenuItem(Actions::File_New, this, Qt::WindowShortcut);
    ui->toolBar->addAction(act);

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
    if ( AppConfig().option<Config::disable_tray>() ) {
        if (m_tray) {
            // Hide tray on Ubuntu (buggy sni-qt)
            m_tray->hide();

            delete m_tray;
            m_tray = NULL;
        }

        m_timerTrayAvailable.stop();
    } else if (!m_tray) {
        createTrayIfSupported();
    }

    if (closeMinimizes() && isHidden() && !isMinimized())
        minimizeWindow();
}

void MainWindow::runAutomaticCommand(const Command &command)
{
    Q_ASSERT(!m_currentAutomaticCommand);

    const QVariantMap &data = m_automaticCommandTester.data();

    if (command.remove || command.transform) {
        COPYQ_LOG("Clipboard ignored by \"" + command.name + "\"");
        m_automaticCommandTester.abort();
    }

    if ( command.input.isEmpty()
         || command.input == mimeItems
         || data.contains(command.input) )
    {
        m_currentAutomaticCommand = action(data, command);
    }

    if (!command.tab.isEmpty())
        addToTab(data, command.tab);

    if (m_currentAutomaticCommand)
        m_automaticCommandTester.waitForAction(m_currentAutomaticCommand);
    else
        m_automaticCommandTester.start();
}

bool MainWindow::isWindowVisible() const
{
    return !isMinimized() && isVisible() && isActiveWindow();
}

ClipboardBrowser *MainWindow::clipboardTab()
{
    return m_options.clipboardTab.isEmpty() ? NULL : tab(m_options.clipboardTab);
}

void MainWindow::onEscape()
{
    if ( browseMode() ) {
        hideWindow();
        getBrowser()->setCurrent(0);
    } else {
        resetStatus();
    }
}

void MainWindow::setDisabledShortcuts(const QList<QKeySequence> &shortcuts)
{
    m_disabledShortcuts = shortcuts;
    updateActionShortcuts();
}

void MainWindow::updateActionShortcuts(int id)
{
    Q_ASSERT(id < m_actions.size());
    Q_ASSERT(id < m_menuItems.size());

    QAction *action = m_actions[id];
    if (!action)
        return;

    QList<QKeySequence> shortcuts = m_menuItems[id].shortcuts;
    foreach (const QKeySequence &shortcut, m_disabledShortcuts)
        shortcuts.removeAll(shortcut);

    action->setShortcuts(shortcuts);
}

void MainWindow::updateActionShortcuts()
{
    for (int id = 0; id < m_actions.size(); ++id)
        updateActionShortcuts(id);
}

QAction *MainWindow::actionForMenuItem(int id, QWidget *parent, Qt::ShortcutContext context)
{
    Q_ASSERT(id < m_menuItems.size());

    m_actions.resize(m_menuItems.size());

    QPointer<QAction> &action = m_actions[id];
    const MenuItem &item = m_menuItems[id];

    if (!action) {
        action = new QAction(item.text, parent);
        action->setShortcutContext(context);
        parent->addAction(action);
    }

    action->setIcon( getIcon(item.iconName, item.iconId) );
    updateActionShortcuts(id);

    return action;
}

int MainWindow::findTabIndex(const QString &name)
{
    TabWidget *w = ui->tabWidget;

    const int i = findTabIndexExactMatch(name);
    if (i != -1)
        return i;

    // Ignore key hints ('&').
    if ( !hasKeyHint(name) ) {
        for( int i = 0; i < w->count(); ++i ) {
            QString tabName = w->tabText(i);
            if ( name == removeKeyHint(tabName) )
                return i;
        }
    }

    return -1;
}

ClipboardBrowser *MainWindow::tab(const QString &name)
{
    bool needSave;
    ClipboardBrowser *c = createTab(name, MatchSimilarTabName, &needSave);
    if (needSave)
        saveTabPositions();
    c->loadItems();
    return c;
}

bool MainWindow::hasRunningAction() const
{
    return m_actionHandler->hasRunningAction();
}

bool MainWindow::maybeCloseCommandDialog()
{
    return !m_commandDialog || m_commandDialog->maybeClose(this);
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
    notificationDaemon()->create(title, msg, icon, msec, true, notificationId);
}

void MainWindow::showClipboardMessage(const QVariantMap &data)
{
    if ( m_options.itemPopupInterval != 0 && m_options.clipboardNotificationLines > 0) {
        if (data.isEmpty()) {
            notificationDaemon()->removeNotification(clipboardNotificationId);
        } else {
            notificationDaemon()->create(
                        data, m_options.clipboardNotificationLines, IconPaste,
                        m_options.itemPopupInterval * 1000, false, clipboardNotificationId );
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

    if (m_options.viMode) {
        if (modifiers == Qt::ControlModifier && key == Qt::Key_BracketLeft) {
            onEscape();
            return;
        }

        if (browseMode()) {
            if (handleViKey(event, c))
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

    if (modifiers == Qt::AltModifier)
        return;

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
            onEscape();
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
        updateWindowTransparency();
    } else if (type == QEvent::WindowDeactivate) {
        m_timerShowWindow.start();
        m_lastWindow.clear();
        updateWindowTransparency();
        setHideTabs(m_options.hideTabs);
    } else if (type == QEvent::Hide) {
        m_wasMaximized = isMaximized();
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

    loadItemFactorySettings(m_sharedData->itemFactory);

    const Theme theme;
    theme.decorateToolBar(ui->toolBar);
    theme.decorateMainWindow(this);

    AppConfig appConfig;

    m_options.confirmExit = appConfig.option<Config::confirm_exit>();

    // always on top window hint
    bool alwaysOnTop = appConfig.option<Config::always_on_top>();
    setAlwaysOnTop(this, alwaysOnTop);
    setAlwaysOnTop(m_commandDialog.data(), alwaysOnTop);

    // Vi mode
    m_options.viMode = appConfig.option<Config::vi>();
    m_trayMenu->setViModeEnabled(m_options.viMode);

    m_options.transparency = appConfig.option<Config::transparency>();
    m_options.transparencyFocused = appConfig.option<Config::transparency_focused>();
    updateWindowTransparency();

    // tab bar position
    const bool tabTreeEnabled = appConfig.option<Config::tab_tree>();
    ui->tabWidget->setTreeModeEnabled(tabTreeEnabled);
    ui->tabWidget->setTabItemCountVisible(appConfig.option<Config::show_tab_item_count>());
    if (tabTreeEnabled)
        theme.decorateScrollArea(ui->tabWidget->tabTree());

    m_options.hideTabs = appConfig.option<Config::hide_tabs>();
    setHideTabs(m_options.hideTabs);

    bool hideToolbar = appConfig.option<Config::hide_toolbar>();
    ui->toolBar->clear();
    ui->toolBar->setHidden(hideToolbar);
    bool hideToolBarLabels = appConfig.option<Config::hide_toolbar_labels>();
    ui->toolBar->setToolButtonStyle(hideToolBarLabels ? Qt::ToolButtonIconOnly
                                                      : Qt::ToolButtonTextUnderIcon);

    m_options.hideMainWindow = appConfig.option<Config::hide_main_window>();

    // shared data for browsers
    m_sharedData->loadFromConfiguration();

    // create tabs
    QStringList tabs = savedTabs();
    foreach (const QString &name, tabs) {
        bool settingsLoaded;
        ClipboardBrowser *c = createTab(name, MatchExactTabName, &settingsLoaded);
        if (!settingsLoaded)
            c->loadSettings();
    }

    if ( ui->tabWidget->count() == 0 )
        addTab( defaultTabName() );

    ui->tabWidget->updateTabs();

    updateContextMenu();

    m_options.itemActivationCommands = ActivateNoCommand;
    if ( appConfig.option<Config::activate_closes>() )
        m_options.itemActivationCommands |= ActivateCloses;
    if ( appConfig.option<Config::activate_focuses>() )
        m_options.itemActivationCommands |= ActivateFocuses;
    if ( appConfig.option<Config::activate_pastes>() )
        m_options.itemActivationCommands |= ActivatePastes;

    m_options.trayItems = appConfig.option<Config::tray_items>();
    m_options.trayItemPaste = appConfig.option<Config::tray_item_paste>();
    m_options.trayCommands = appConfig.option<Config::tray_commands>();
    m_options.trayCurrentTab = appConfig.option<Config::tray_tab_is_current>();
    m_options.trayTabName = appConfig.option<Config::tray_tab>();
    m_options.trayImages = appConfig.option<Config::tray_images>();
    m_options.itemPopupInterval = appConfig.option<Config::item_popup_interval>();
    m_options.clipboardNotificationLines = appConfig.option<Config::clipboard_notification_lines>();
    m_options.clipboardTab = appConfig.option<Config::clipboard_tab>();

    m_trayMenu->setStyleSheet( theme.getToolTipStyleSheet() );

    initTray();

    if (m_notifications != NULL)
        updateNotifications();

    updateIcon();

    ui->searchBar->loadSettings();

    m_lastWindow.clear();

    QSettings settings;
    settings.beginGroup("Shortcuts");
    loadShortcuts(&m_menuItems, settings);
    settings.endGroup();

    COPYQ_LOG("Configuration loaded");
}

void MainWindow::showWindow()
{
    if ( isWindowVisible() )
        return;

    moveToCurrentWorkspace(this);

    updateFocusWindows();

    if (m_wasMaximized || isMaximized())
        showMaximized();
    else
        showNormal();
    raise();
    activateWindow();

    // if no item is selected then select first
    ClipboardBrowser *c = browser();
    if( c->selectionModel()->selectedIndexes().size() <= 1 &&
            c->currentIndex().row() <= 0 ) {
        c->setCurrent(0);
    }

    if ( !c->editing() )
        c->scrollTo( c->currentIndex() );
    c->setFocus();

    QApplication::setActiveWindow(this);

    stealFocus(*this);
}

void MainWindow::hideWindow()
{
    if ( closeMinimizes() )
        minimizeWindow();
    else
        hide();
}

void MainWindow::minimizeWindow()
{
    if (m_options.hideMainWindow)
        hide();
    else
        showMinimized();
}

bool MainWindow::toggleVisible()
{
    // Showing/hiding window in quick succession doesn't work well on X11.
    if ( m_timerShowWindow.isActive() || isWindowVisible() ) {
        hideWindow();
        return false;
    }

    showWindow();
    return true;
}

void MainWindow::setCurrentTab(const ClipboardBrowser *browser)
{
    int i = 0;
    for( ; i < ui->tabWidget->count() && getBrowser(i) != browser; ++i ) {}
    setCurrentTab(i);
}

void MainWindow::showBrowser(const ClipboardBrowser *browser)
{
    setCurrentTab(browser);
    showWindow();
}

bool MainWindow::setCurrentTab(int index)
{
    if ( index < 0 || ui->tabWidget->count() <= index )
        return false;

    ui->tabWidget->setCurrentIndex(index);
    return true;
}

void MainWindow::showBrowser(int index)
{
    if (setCurrentTab(index))
        showWindow();
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
    setTabs( ui->tabWidget->tabs() );
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
    tab(tabName)->addUnique(data);
}

void MainWindow::updateFirstItem(const QVariantMap &data)
{
    // Synchronize clipboard and X11 selection.
    if (needSyncClipboardToSelection(data))
        emit changeClipboard(data, QClipboard::Selection);

    if (needSyncSelectionToClipboard(data))
        emit changeClipboard(data, QClipboard::Clipboard);

    if (needStore(data)) {
        ClipboardBrowser *c = clipboardTab();
        if ( c && c->isLoaded() )
            c->addUnique(data);
    }
}

QString MainWindow::getUserOptionsDescription() const
{
    ConfigurationManager configurationManager;

    QStringList options = configurationManager.options();
    options.sort();
    QString opts;
    foreach (const QString &option, options)
        opts.append( option + "\n  " + configurationManager.optionToolTip(option).replace('\n', "\n  ") + '\n' );
    return opts;
}

QString MainWindow::getUserOptionValue(const QString &name) const
{
    ConfigurationManager configurationManager;
    return configurationManager.optionValue(name);
}

void MainWindow::setUserOptionValue(const QString &name, const QString &value)
{
    ConfigurationManager configurationManager;
    if ( configurationManager.setOptionValue(name, value) )
        emit configurationChanged();
}

bool MainWindow::hasUserOption(const QString &name) const
{
    ConfigurationManager configurationManager;
    return configurationManager.options().contains(name);
}

QString syncCommand(const QString &type)
{
    return "try {"
           "  var x = input()"
           "  copy" + type + "(input())"
           "} catch (e) {}";
}

void MainWindow::runAutomaticCommands(const QVariantMap &data)
{
    bool isClipboard = isClipboardData(data);

    // Don't abort currently commands if X11 selection changes rapidly.
    if (!isClipboard && (!m_automaticCommandTester.isCompleted() || m_currentAutomaticCommand))
        return;

    QList<Command> commands;
    const QString tabName = defaultTabName();
    foreach (const Command &command, m_commands) {
        if (command.automatic && canExecuteCommand(command, data, tabName)) {
            commands.append(command);
            if ( command.outputTab.isEmpty() )
                commands.last().outputTab = tabName;
        }
    }

    // Add new clipboard to the first tab (if configured so).
    if ( needSyncClipboardToSelection(data)
            || needSyncSelectionToClipboard(data)
            || needStore(data) )
    {
        commands.append(automaticCommand("Add Item from Clipboard", "updateFirst()"));
    }

    if (isClipboard) {
        // First, clear window title and tooltip.
        commands.prepend(automaticCommand("Reset Window Title" ,"updateTitle('')"));

        // Lastly, set window title, tooltip and show notification.
        // This may not be executed if any previous user-defined automatic
        // command chooses to ignore clipboard content.
        commands.append(automaticCommand("Set Window Title", "updateTitle()"));
    }

    m_automaticCommandTester.abort();
    m_automaticCommandTester.setCommands(commands, data);

    if (!m_currentAutomaticCommand)
        m_automaticCommandTester.start();
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
    if ( !ownsClipboardData(data)
         && !isClipboardDataHidden(data)
         && containsAnyData(data) )
    {
        m_canUpdateTitleFromScript = true;
        runAutomaticCommands(data);
    } else if (isClipboardData(data)) {
        m_canUpdateTitleFromScript = false;
        updateTitle(data);
    }
}

void MainWindow::setClipboard(const QVariantMap &data, QClipboard::Mode mode)
{
    emit changeClipboard(data, mode);
}

void MainWindow::setClipboard(const QVariantMap &data)
{
    setClipboard(data, QClipboard::Clipboard);
#ifdef COPYQ_WS_X11
    setClipboard(data, QClipboard::Selection);
#endif
}

void MainWindow::activateCurrentItem()
{
    if ( !m_activateCurrentItemAction.isNull() ) {
        m_activateCurrentItemAction->trigger();
        return;
    }

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
    updateIconSnip();

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
    m_automaticCommandTester.abort();

    if (m_currentAutomaticCommand)
        COPYQ_LOG("Aborting automatic commands (current is \"" + m_currentAutomaticCommand->name() + "\")");
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
        return clipboardTab();

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
    if ( isActiveWindow() )
        return;

    if ( !m_options.activateFocuses() && !m_options.activatePastes() )
        return;

    PlatformPtr platform = createPlatformNativeInterface();
    PlatformWindowPtr lastWindow = platform->getCurrentWindow();
    if (lastWindow)
        m_lastWindow = lastWindow;
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

    clearTrayMenu();

    QAction *act = m_trayMenu->addAction(
                appIcon(), tr("&Show/Hide"), this, SLOT(toggleVisible()) );
    m_trayMenu->setDefaultAction(act);
    addTrayAction(Actions::File_New);
    act = addTrayAction(Actions::Item_Action);
    act->setWhatsThis( tr("Open action dialog") );
    addTrayAction(Actions::File_Preferences);
    addTrayAction(Actions::File_ToggleClipboardStoring);
    m_trayMenu->addSeparator();
    addTrayAction(Actions::File_Exit);

    // Add items.
    const int len = (c != NULL) ? qMin( m_options.trayItems, c->length() ) : 0;
    const int current = c->currentIndex().row();
    for ( int i = 0; i < len; ++i ) {
        const QModelIndex index = c->model()->index(i, 0);
        m_trayMenu->addClipboardItemAction(index, m_options.trayImages, i == current);
    }

    // Add commands.
    if (m_options.trayCommands) {
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

void MainWindow::clearTrayMenu()
{
    m_trayMenuCommandTester.abort();
    m_trayMenu->clearAllActions();
}

void MainWindow::openLogDialog()
{
    openDialog<LogDialog>(this);
}

void MainWindow::openAboutDialog()
{
    openDialog<AboutDialog>(this);
}

void MainWindow::showClipboardContent()
{
    ClipboardDialog *clipboardDialog = openDialog<ClipboardDialog>(this);
    connect( clipboardDialog, SIGNAL(changeClipboard(QVariantMap)),
             this, SLOT(setClipboard(QVariantMap)) );
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

    actionDialog->setOutputIndex(getBrowser()->currentIndex());

    actionDialog->show();
    return stealFocus(*actionDialog.take());
}

void MainWindow::openPreferences()
{
    if ( !isEnabled() )
        return;

    if (cm) {
        cm->activateWindow();
        return;
    }

    ConfigurationManager configurationManager(m_sharedData->itemFactory, this);
    WindowGeometryGuard::create(&configurationManager);

    // notify window if configuration changes
    connect( &configurationManager, SIGNAL(configurationChanged()),
             this, SIGNAL(configurationChanged()) );
    connect( &configurationManager, SIGNAL(error(QString)),
             this, SLOT(showError(QString)) );
    connect( &configurationManager, SIGNAL(openCommandDialogRequest()),
             this, SLOT(openCommands()) );

    cm = &configurationManager;
    configurationManager.exec();
    cm = NULL;
}

void MainWindow::openCommands()
{
    if ( !isEnabled() )
        return;

    if (m_commandDialog) {
        m_commandDialog->show();
        m_commandDialog->activateWindow();
    } else {
        const QList<Command> pluginCommands = m_sharedData->itemFactory->commands();
        QStringList formats = m_sharedData->itemFactory->formatsToSave();
        formats.prepend(mimeText);
        formats.removeDuplicates();

        QWidget *parent = this;
        if (cm)
            parent = cm;

        m_commandDialog = new CommandDialog(pluginCommands, formats, parent);
        if (windowFlags() & Qt::WindowStaysOnTopHint)
            setAlwaysOnTop(m_commandDialog.data(), true);
        m_commandDialog->setAttribute(Qt::WA_DeleteOnClose, true);
        m_commandDialog->show();
        connect(this, SIGNAL(destroyed()), m_commandDialog, SLOT(close()));
        connect(m_commandDialog, SIGNAL(commandsSaved()), this, SLOT(onCommandDialogSaved()));
    }

    if (cm && cm->isVisible())
        m_commandDialog->setWindowModality(Qt::ApplicationModal);
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
    ClipboardBrowser *c = createTab(name, MatchExactTabName);
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

    out << QByteArray("CopyQ v2") << c->tabName();
    serializeData(*c->model(), &out);

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
    ui->tabWidget->saveTabInfo();
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

    ClipboardBrowser *c = createTab(tabName, MatchExactTabName);

    deserializeData(c->model(), &in);

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
    IconSelectDialog dialog( getIconNameForTabName(tabName), this );

    if ( dialog.exec() == QDialog::Accepted )
        setTabIcon(tabName, dialog.selectedIcon());
}

void MainWindow::setTabIcon(const QString &tabName, const QString &icon)
{
    if ( tabs().contains(tabName) || ui->tabWidget->isTabGroup(tabName) ) {
        setIconNameForTabName(tabName, icon);
        ui->tabWidget->updateTabIcon(tabName);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}
