/*
    Copyright (c) 2018, Lukas Holecek <hluk@email.cz>

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
#include "common/actionoutput.h"
#include "common/appconfig.h"
#include "common/common.h"
#include "common/command.h"
#include "common/commandstore.h"
#include "common/config.h"
#include "common/contenttype.h"
#include "common/display.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/shortcuts.h"
#include "common/textdata.h"
#include "common/timer.h"
#include "gui/aboutdialog.h"
#include "gui/actiondialog.h"
#include "gui/actionhandler.h"
#include "gui/clipboardbrowser.h"
#include "gui/clipboardbrowserplaceholder.h"
#include "gui/clipboardbrowsershared.h"
#include "gui/clipboarddialog.h"
#include "gui/commandaction.h"
#include "gui/commanddialog.h"
#include "gui/configurationmanager.h"
#include "gui/importexportdialog.h"
#include "gui/iconfactory.h"
#include "gui/iconfactory.h"
#include "gui/iconselectdialog.h"
#include "gui/icons.h"
#include "gui/logdialog.h"
#include "gui/notification.h"
#include "gui/notificationbutton.h"
#include "gui/notificationdaemon.h"
#include "gui/tabdialog.h"
#include "gui/tabicons.h"
#include "gui/tabwidget.h"
#include "gui/theme.h"
#include "gui/traymenu.h"
#include "gui/windowgeometryguard.h"
#include "item/itemfactory.h"
#include "item/serialize.h"
#include "platform/platformclipboard.h"
#include "platform/platformnativeinterface.h"
#include "platform/platformwindow.h"

#ifdef Q_OS_MAC
#  include "platform/mac/foregroundbackgroundfilter.h"
#endif

#include <QAction>
#include <QCloseEvent>
#include <QDesktopServices>
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
#include <QUrl>

#include <algorithm>
#include <memory>

namespace {

const int contextMenuUpdateIntervalMsec = 100;
const int trayMenuUpdateIntervalMsec = 100;

const QIcon iconClipboard() { return getIcon("clipboard", IconPaste); }
const QIcon iconTabIcon() { return getIconFromResources("tab_icon"); }
const QIcon iconTabNew() { return getIconFromResources("tab_new"); }
const QIcon iconTabRemove() { return getIconFromResources("tab_remove"); }
const QIcon iconTabRename() { return getIconFromResources("tab_rename"); }

const char propertyWidgetSizeGuarded[] = "CopyQ_widget_size_guarded";

/// Omit size changes of a widget.
class WidgetSizeGuard final : public QObject {
public:
    explicit WidgetSizeGuard(QWidget *guardedObject)
        : m_guarded(guardedObject)
    {
        if ( m_guarded->property(propertyWidgetSizeGuarded).toBool() ) {
            m_guarded = nullptr;
        } else {
            m_guarded->setProperty(propertyWidgetSizeGuarded, true);
            m_guarded->setFixedSize( m_guarded->size() );
        }
    }

    ~WidgetSizeGuard()
    {
        if (m_guarded) {
            m_guarded->setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
            m_guarded->adjustSize();
            m_guarded->resize( m_guarded->sizeHint() );
            m_guarded->setProperty(propertyWidgetSizeGuarded, false);
        }
    }

private:
    QWidget *m_guarded;
};

bool canPaste()
{
    return !QApplication::queryKeyboardModifiers().testFlag(Qt::ControlModifier);
}

bool matchData(const QRegExp &re, const QVariantMap &data, const QString &format)
{
    if ( re.isEmpty() )
        return true;

    const QString text = getTextData(data, format);
    return re.indexIn(text) != -1;
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
        if (command.input == mimeItems || command.input == "!OUTPUT") {
            // Disallow applying action that takes serialized item more times.
            if ( data.contains(command.output) )
                return false;
        } else if ( !data.contains(command.input) ) {
            return false;
        }
    }

    // Verify that and text matches given regexp.
    if ( !matchData(command.re, data, mimeText) )
        return false;

    // Verify that window title matches given regexp.
    if ( !matchData(command.wndre, data, mimeWindowTitle) )
        return false;

    return true;
}

void stealFocus(const QWidget &window)
{
    WId wid = window.winId();
    PlatformWindowPtr platformWindow = createPlatformNativeInterface()->getWindow(wid);
    if (platformWindow)
        platformWindow->raise();
}

template <typename WidgetOrAction>
void disableActionWhenTabGroupSelected(WidgetOrAction *action, MainWindow *window)
{
    QObject::connect( window, &MainWindow::tabGroupSelected,
                      action, &WidgetOrAction::setDisabled );
}

/// Adds information about current tab and selection if command is triggered by user.
QVariantMap addSelectionData(
        const ClipboardBrowser &c,
        const QModelIndex &currentIndex,
        const QModelIndexList &selectedIndexes)
{
    auto result = c.copyIndexes(selectedIndexes);

    result.insert(mimeCurrentTab, c.tabName());

    if ( currentIndex.isValid() ) {
        const QPersistentModelIndex current = currentIndex;
        result.insert(mimeCurrentItem, QVariant::fromValue(current));
    }

    if ( !selectedIndexes.isEmpty() ) {
        QList<QPersistentModelIndex> selected;
        selected.reserve(selectedIndexes.size());
        for (const auto &index : selectedIndexes)
            selected.append(index);
        qSort(selected);
        result.insert(mimeSelectedItems, QVariant::fromValue(selected));
    }

    return result;
}

QVariantMap addSelectionData(const ClipboardBrowser &c)
{
    const QModelIndexList selectedIndexes = c.selectionModel()->selectedIndexes();
    const auto current = c.selectionModel()->currentIndex();
    return addSelectionData(c, current, selectedIndexes);
}

QMenu *findSubMenu(const QString &name, const QMenu &menu)
{
    for (auto action : menu.actions()) {
        QMenu *subMenu = action->menu();
        if (subMenu && subMenu->title() == name)
            return subMenu;
    }

    return nullptr;
}

QMenu *createSubMenus(QString *name, QMenu *menu)
{
    QStringList path = name->split('|');
    if (path.size() == 1)
        return menu;

    *name = path.takeLast();

    QMenu *parentMenu = menu;

    for (const auto &subMenuName : path) {
        QMenu *subMenu = findSubMenu(subMenuName, *parentMenu);

        if (!subMenu) {
            subMenu = new QMenu(subMenuName, parentMenu);
            parentMenu->addMenu(subMenu);
        }

        parentMenu = subMenu;
    }

    return parentMenu;
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

template <typename Dialog>
Dialog *openDialog(QWidget *dialogParent)
{
    std::unique_ptr<Dialog> dialog( new Dialog(dialogParent) );
    WindowGeometryGuard::create( dialog.get() );
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->setWindowIcon(appIcon());
    dialog->activateWindow();
    dialog->show();
    return dialog.release();
}

bool isItemActivationShortcut(const QKeySequence &shortcut)
{
    return (shortcut[0] == Qt::Key_Return || shortcut[0] == Qt::Key_Enter)
            && shortcut[1] == 0
            && shortcut[2] == 0
            && shortcut[3] == 0;
}

QString importExportFileDialogFilter()
{
    return MainWindow::tr("CopyQ Items (*.cpq)");
}

/**
 * Returns QVariant value that can be serialized and deserialized with QDataStream.
 *
 * WORKAROUND: Invalid QVariant() can be serialized but deserialization fails.
 */
QVariant serializableValue(const QSettings &settings, const QString &key)
{
    const auto value = settings.value(key);
    if ( value.isValid() )
        return value;

    return QString();
}

bool isAnyApplicationWindowActive()
{
    if ( qApp->activeWindow() )
        return true;

    const auto platform = createPlatformNativeInterface();
    const auto currentWindow = platform->getCurrentWindow();
    if (!currentWindow)
        return false;

    const auto currentWindowTitle = currentWindow->getTitle();
    if ( currentWindowTitle.isEmpty() )
        return false;

    for ( auto window : qApp->topLevelWidgets() ) {
        const auto ownWindow = platform->getWindow( window->winId() );
        if ( ownWindow && currentWindowTitle == ownWindow->getTitle() )
            return true;
    }

    return false;
}

} // namespace

MainWindow::MainWindow(ItemFactory *itemFactory, QWidget *parent)
    : QMainWindow(parent)
    , cm(nullptr)
    , ui(new Ui::MainWindow)
    , m_menuItem(nullptr)
    , m_trayMenu( new TrayMenu(this) )
    , m_tray(nullptr)
    , m_clipboardStoringDisabled(false)
    , m_actionToggleClipboardStoring()
    , m_sharedData(std::make_shared<ClipboardBrowserShared>())
    , m_lastWindow()
    , m_notifications(nullptr)
    , m_actionHandler(new ActionHandler(this))
    , m_menu( new TrayMenu(this) )
    , m_menuMaxItemCount(-1)
    , m_commandDialog(nullptr)
    , m_wasMaximized(false)
    , m_showItemPreview(false)
    , m_menuItems(menuItems())
    , m_clipboardManager(m_actionHandler)
{
    ui->setupUi(this);

    menuBar()->setObjectName("menu_bar");
    createMenu();

    ui->tabWidget->addToolBars(this);
    addToolBar(Qt::RightToolBarArea, ui->toolBar);

    ui->dockWidgetItemPreview->hide();

    WindowGeometryGuard::create(this);
    restoreState( mainWindowState(objectName()) );
    // NOTE: QWidget::isVisible() returns false if parent is not visible.
    m_showItemPreview = !ui->dockWidgetItemPreview->isHidden();

    m_sharedData->itemFactory = itemFactory;

    updateIcon();

    updateFocusWindows();

    connect( m_trayMenu, &QMenu::aboutToShow,
             this, &MainWindow::updateFocusWindows );
    connect( m_trayMenu, &QMenu::aboutToHide,
             this, [this](){ m_timerRaiseLastWindowAfterMenuClosed.start(); } );
    connect( m_trayMenu, &TrayMenu::searchRequest,
             this, &MainWindow::filterTrayMenuItems );
    connect( m_trayMenu, &TrayMenu::clipboardItemActionTriggered,
             this, &MainWindow::onTrayActionTriggered );

    connect( m_menu, &QMenu::aboutToShow,
             this, &MainWindow::updateFocusWindows );
    connect( m_menu, &QMenu::aboutToHide,
             this, [this](){ m_timerRaiseLastWindowAfterMenuClosed.start(); } );
    connect( m_menu, &TrayMenu::searchRequest,
             this, &MainWindow::filterMenuItems );
    connect( m_menu, &TrayMenu::clipboardItemActionTriggered,
             this, &MainWindow::onMenuActionTriggered );

    connect( ui->tabWidget, &TabWidget::currentChanged,
             this, &MainWindow::tabChanged );
    connect( ui->tabWidget, &TabWidget::tabMoved,
             this, &MainWindow::saveTabPositions );
    connect( ui->tabWidget, &TabWidget::tabsMoved,
             this, &MainWindow::tabsMoved );
    connect( ui->tabWidget, &TabWidget::tabBarMenuRequested,
             this, &MainWindow::tabBarMenuRequested );
    connect( ui->tabWidget, &TabWidget::tabTreeMenuRequested,
             this, &MainWindow::tabTreeMenuRequested );
    connect( ui->tabWidget, &TabWidget::tabRenamed,
             this, &MainWindow::renameTab );
    connect( ui->tabWidget, &TabWidget::tabCloseRequested,
             this, &MainWindow::tabCloseRequested );
    connect( ui->tabWidget, &TabWidget::dropItems,
             this, &MainWindow::onTabWidgetDropItems);
    connect( ui->searchBar, &Utils::FilterLineEdit::filterChanged,
             this, &MainWindow::onFilterChanged );
    connect( m_actionHandler, &ActionHandler::runningActionsCountChanged,
             this, &MainWindow::updateIconSnip );
    connect( qApp, &QCoreApplication::aboutToQuit,
             this, &MainWindow::onAboutToQuit );
    connect( this, &MainWindow::configurationChanged,
             this, &MainWindow::loadSettings );

    connect(itemFactory, &ItemFactory::error,
            this, &MainWindow::showError);
    connect(itemFactory, &ItemFactory::addCommands,
            this, &MainWindow::addCommands);

    updateCommands();

    initSingleShotTimer( &m_timerUpdateFocusWindows, 100, this, &MainWindow::updateFocusWindows );
    initSingleShotTimer( &m_timerUpdateContextMenu, 0, this, &MainWindow::updateContextMenuTimeout );
    initSingleShotTimer( &m_timerUpdateTrayMenu, trayMenuUpdateIntervalMsec, this, &MainWindow::updateTrayMenuTimeout );
    initSingleShotTimer( &m_timerTrayAvailable, 1000, this, &MainWindow::createTrayIfSupported );
    initSingleShotTimer( &m_timerTrayIconSnip, 500, this, &MainWindow::updateIconSnipTimeout );
    initSingleShotTimer( &m_timerSaveTabPositions, 1000, this, &MainWindow::doSaveTabPositions );
    initSingleShotTimer( &m_timerRaiseLastWindowAfterMenuClosed, 50, this, &MainWindow::raiseLastWindowAfterMenuClosed);
    enableHideWindowOnUnfocus();

    m_trayMenu->setObjectName("TrayMenu");
    m_menu->setObjectName("Menu");
}

bool MainWindow::browseMode() const
{
    return !ui->searchBar->isVisible();
}

void MainWindow::exit()
{
    // Check if not editing in any tab (show and try to close editors).
    for ( int i = 0; i < ui->tabWidget->count(); ++i ) {
        auto c = getPlaceholder(i)->browser();
        if ( c && (c->isInternalEditorOpen() || c->isExternalEditorOpen()) ) {
            setCurrentTab(i);
            if ( !c->maybeCloseEditors() )
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

bool MainWindow::focusNextPrevChild(bool next)
{
    auto c = browser();
    if (!c)
        return false;

    // Fix tab order while searching in editor.
    if (c->isInternalEditorOpen() && !browseMode()) {
        if ( next && ui->searchBar->hasFocus() ) {
            c->setFocus();
            return true;
        }

        if ( !next && c->hasFocus() ) {
            ui->searchBar->setFocus();
            return true;
        }
    }

    return QMainWindow::focusNextPrevChild(next);
}

void MainWindow::createMenu()
{
    QMenuBar *menubar = menuBar();
    QMenu *menu;
    QAction *act;

    menubar->clear();

    // File
    menu = menubar->addMenu( tr("&File") );

    // - new
    act = createAction( Actions::File_New, &MainWindow::editNewItem, menu );
    disableActionWhenTabGroupSelected(act, this);

    // - import
    createAction( Actions::File_Import, &MainWindow::importData, menu );

    // - export
    createAction( Actions::File_Export, &MainWindow::exportData, menu );

    // - separator
    menu->addSeparator();

    // - preferences
    createAction( Actions::File_Preferences, &MainWindow::openPreferences, menu );

    // - commands
    createAction( Actions::File_Commands, &MainWindow::openCommands, menu );

    // - separator
    menu->addSeparator();

    // - show clipboard content
    createAction( Actions::File_ShowClipboardContent, &MainWindow::showClipboardContent, menu );

    // - active commands
    createAction( Actions::File_ProcessManager, &MainWindow::showProcessManagerDialog, menu );

    // - enable/disable
    m_actionToggleClipboardStoring = createAction( Actions::File_ToggleClipboardStoring,
                                                   &MainWindow::toggleClipboardStoring, menu );
    updateMonitoringActions();

    // - separator
    menu->addSeparator();

    // - exit
    createAction( Actions::File_Exit, &MainWindow::exit, menu );

    // Edit
    menu = menubar->addMenu( tr("&Edit") );

    // - find
    createAction( Actions::Edit_FindItems, &MainWindow::findNextOrPrevious, menu );

    // - separator
    menu->addSeparator();

    // - sort
    createAction( Actions::Edit_SortSelectedItems, &MainWindow::sortSelectedItems, menu );

    // - reverse order
    createAction( Actions::Edit_ReverseSelectedItems, &MainWindow::reverseSelectedItems, menu );

    // - separator
    menu->addSeparator();

    // - paste items
    createAction( Actions::Edit_PasteItems, &MainWindow::pasteItems, menu );

    // - copy items
    createAction( Actions::Edit_CopySelectedItems, &MainWindow::copyItems, menu );

    // Items
    m_menuItem = menubar->addMenu( tr("&Item") );
    disableActionWhenTabGroupSelected(m_menuItem, this);

    // Tabs
    menu = menubar->addMenu(tr("&Tabs"));

    // - new tab
    createAction( Actions::Tabs_NewTab, &MainWindow::openNewTabDialog, menu );

    // - rename tab
    act = createAction( Actions::Tabs_RenameTab, &MainWindow::openRenameTabDialog, menu );
    disableActionWhenTabGroupSelected(act, this);

    // - remove tab
    act = createAction( Actions::Tabs_RemoveTab, &MainWindow::removeTab, menu );
    disableActionWhenTabGroupSelected(act, this);

    createAction( Actions::Tabs_ChangeTabIcon, &MainWindow::setTabIcon, menu );

    // - separator
    menu->addSeparator();

    // - next tab
    createAction( Actions::Tabs_NextTab, &MainWindow::nextTab, menu );

    // - previous tab
    createAction( Actions::Tabs_PreviousTab, &MainWindow::previousTab, menu );

    // Help
    menu = menubar->addMenu(tr("&Help"));
    createAction( Actions::Help_Help, &MainWindow::openHelp, menu );
    createAction( Actions::Help_ShowLog, &MainWindow::openLogDialog, menu );
    createAction( Actions::Help_About, &MainWindow::openAboutDialog, menu );

    // Open Item Menu
    createAction( Actions::ItemMenu, &MainWindow::showContextMenu, nullptr );

    for (auto menu : menuBar()->findChildren<QMenu*>()) {
        connect( menu, &QMenu::aboutToShow,
                 this, &MainWindow::disableHideWindowOnUnfocus );
        connect( menu, &QMenu::aboutToHide,
                 this, &MainWindow::enableHideWindowOnUnfocus );
    }
}

void MainWindow::popupTabBarMenu(QPoint pos, const QString &tab)
{
    QMenu menu(ui->tabWidget);

    const int tabIndex = ui->tabWidget->tabs().indexOf(tab);
    bool hasTab = tabIndex != -1;
    bool isGroup = ui->tabWidget->isTabGroup(tab);

    const QString quotedTab = quoteString(tab);
    QAction *actNew = menu.addAction( iconTabNew(), tr("&New Tab") );
    QAction *actRenameGroup =
            isGroup ? menu.addAction( iconTabRename(), tr("Rename &Group %1").arg(quotedTab) ) : nullptr;
    QAction *actRename =
            hasTab ? menu.addAction( iconTabRename(), tr("Re&name Tab %1").arg(quotedTab) ) : nullptr;
    QAction *actRemove =
            hasTab ? menu.addAction( iconTabRemove(), tr("Re&move Tab %1").arg(quotedTab) ) : nullptr;
    QAction *actRemoveGroup =
            isGroup ? menu.addAction( iconTabRemove(), tr("Remove Group %1").arg(quotedTab) ) : nullptr;

    QAction *actIcon = menu.addAction( iconTabIcon(), tr("&Change Tab Icon") );

    QAction *act = menu.exec(pos);
    if (act != nullptr) {
        if (act == actNew)
            openNewTabDialog(tab);
        else if (act == actRenameGroup)
            openRenameTabGroupDialog(tab);
        else if (act == actRename)
            openRenameTabDialog(tabIndex);
        else if (act == actRemove)
            removeTab(true, tabIndex);
        else if (act == actRemoveGroup)
            removeTabGroup(tab);
        else if (act == actIcon)
            setTabIcon(tab);
    }
}

void MainWindow::updateContextMenu(int intervalMsec)
{
    m_itemMenuMatchCommands = MenuMatchCommands();

    for (auto action : m_menuItem->actions())
        delete action;

    for (auto menu : m_menuItem->findChildren<QMenu*>())
        delete menu;

    m_menuItem->clear();
    // Omit tool bar flickering.
    ui->toolBar->setUpdatesEnabled(false);
    ui->toolBar->setEnabled(false);

    m_timerUpdateContextMenu.start(intervalMsec);
}

void MainWindow::updateTrayMenu()
{
    if ( !m_timerUpdateTrayMenu.isActive() )
        m_timerUpdateTrayMenu.start();
    updateTrayMenuClipboard();
}

void MainWindow::updateTrayMenuClipboard()
{
    if (m_trayMenuClipboardAction) {
        const QString format = tr("&Clipboard: %1", "Tray menu clipboard item format");
        const auto font = m_trayMenuClipboardAction->font();
        const auto clipboardLabel = textLabelForData(m_clipboardData, font, format, true);
        m_trayMenuClipboardAction->setText(clipboardLabel);
    }

    m_trayMenu->markItemInClipboard(m_clipboardData);
}

void MainWindow::updateIcon()
{
    const QIcon icon = appIcon(m_iconSnip ? AppIconRunning : AppIconNormal);
    setWindowIcon(icon);
    if (m_tray)
        m_tray->setIcon(icon);
}

void MainWindow::updateIconSnipTimeout()
{
    const bool shouldSnip = m_forceIconSnip ? !m_iconSnip : m_clipboardStoringDisabled || hasRunningAction();
    m_forceIconSnip = false;
    if (m_iconSnip != shouldSnip) {
        m_iconSnip = shouldSnip;
        m_timerTrayIconSnip.start();
        updateIcon();
    }
}

void MainWindow::updateContextMenuTimeout()
{
    updateItemPreview();

    auto c = getPlaceholder()->createBrowser();
    if ( ui->tabWidget->isTabGroupSelected() || !c || c->isInternalEditorOpen()) {
        ui->toolBar->clear();
        ui->toolBar->setUpdatesEnabled(true);
        return;
    }

    setDisabledShortcuts(QList<QKeySequence>());

    addCommandsToItemMenu(c);

    m_menuItem->addSeparator();

    addItemAction( Actions::Item_MoveToClipboard, c, &ClipboardBrowser::moveToClipboard );
    addItemAction( Actions::Item_ShowContent, c, &ClipboardBrowser::showItemContent );
    QAction *togglePreviewAction =
            addItemAction( Actions::Item_ShowPreview, this, &MainWindow::updateItemPreview );
    addItemAction( Actions::Item_Remove, c, &ClipboardBrowser::remove );
    addItemAction( Actions::Item_Edit, c, &ClipboardBrowser::editSelected );
    addItemAction( Actions::Item_EditNotes, c, &ClipboardBrowser::editNotes );
    addItemAction( Actions::Item_EditWithEditor, c, &ClipboardBrowser::openEditor );
    addItemAction( Actions::Item_Action, this, &MainWindow::openActionDialog );
    addItemAction( Actions::Item_NextFormat, this, &MainWindow::nextItemFormat );
    addItemAction( Actions::Item_PreviousFormat, this, &MainWindow::previousItemFormat );

    m_menuItem->addSeparator();

    addItemAction( Actions::Item_MoveUp, this, &MainWindow::moveUp );
    addItemAction( Actions::Item_MoveDown, this, &MainWindow::moveDown );
    addItemAction( Actions::Item_MoveToTop, this, &MainWindow::moveToTop );
    addItemAction( Actions::Item_MoveToBottom, this, &MainWindow::moveToBottom );

    togglePreviewAction->setCheckable(true);
    togglePreviewAction->setChecked(m_showItemPreview);
    connect( togglePreviewAction, &QAction::toggled,
             this, &MainWindow::setItemPreviewVisible, Qt::UniqueConnection );

    updateToolBar();
}

void MainWindow::updateItemPreview()
{
    auto c = browser();
    if (!c)
        return;

    ui->dockWidgetItemPreview->setVisible(m_showItemPreview && !c->isInternalEditorOpen());

    QWidget *w = ui->dockWidgetItemPreview->isVisible() && !ui->tabWidget->isTabGroupSelected()
            ? c->currentItemPreview()
            : nullptr;

    ui->scrollAreaItemPreview->setVisible(w != nullptr);
    ui->scrollAreaItemPreview->setWidget(w);
    if (w) {
        ui->dockWidgetItemPreview->setStyleSheet( c->styleSheet() );
        w->show();
    }
}

void MainWindow::setItemPreviewVisible(bool visible)
{
    m_showItemPreview = visible;
    updateItemPreview();
}

void MainWindow::updateIconSnip()
{
    if ( !m_timerTrayIconSnip.isActive() )
        updateIconSnipTimeout();
}

void MainWindow::onAboutToQuit()
{
    if (cm)
        cm->disconnect();

    saveMainWindowState( objectName(), saveState() );
    hideWindow();
    if (m_tray)
        m_tray->hide();

    m_itemMenuMatchCommands = MenuMatchCommands();
    m_trayMenuMatchCommands = MenuMatchCommands();
}

void MainWindow::onCommandDialogSaved()
{
    updateCommands();
}

void MainWindow::onSaveCommand(const Command &command)
{
    auto commands = loadAllCommands();
    commands.append(command);
    setCommands(commands);
}

void MainWindow::onItemCommandActionTriggered(CommandAction *commandAction, const QString &triggeredShortcut)
{
    auto c = getPlaceholder()->createBrowser();
    if (!c)
        return;

    const QModelIndexList selected = c->selectionModel()->selectedIndexes();

    const auto command = commandAction->command();

    if ( !command.cmd.isEmpty() ) {
        if (command.transform) {
            for (const auto &index : selected) {
                const auto selection = QModelIndexList() << index;
                auto actionData = addSelectionData(*c, index, selection);
                if ( !triggeredShortcut.isEmpty() )
                    actionData.insert(mimeShortcut, triggeredShortcut);
                action(actionData, command, index);
            }
        } else {
            auto actionData = addSelectionData(*c);
            if ( !triggeredShortcut.isEmpty() )
                actionData.insert(mimeShortcut, triggeredShortcut);
            action(actionData, command, QModelIndex());
        }
    }

    if ( !command.tab.isEmpty() && command.tab != c->tabName() ) {
        auto c2 = tab(command.tab);
        if (c2) {
            for (int i = selected.size() - 1; i >= 0; --i) {
                const auto data = c->copyIndex(selected[i]);
                if ( !data.isEmpty() )
                    c2->addUnique(data, ClipboardMode::Clipboard);
            }
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

void MainWindow::onClipboardCommandActionTriggered(CommandAction *commandAction, const QString &triggeredShortcut)
{
    const QMimeData *data = clipboardData();
    if (data == nullptr)
        return;

    auto actionData = cloneData(*data);
    if ( !triggeredShortcut.isEmpty() )
        actionData.insert(mimeShortcut, triggeredShortcut);

    auto command = commandAction->command();

    action( actionData, command, QModelIndex() );
}

void MainWindow::onTabWidgetDropItems(const QString &tabName, const QMimeData *data)
{
    auto browser = tab(tabName);

    if (browser) {
        const QVariantMap dataMap = data->hasFormat(mimeItems)
                ? cloneData(*data, QStringList() << mimeItems) : cloneData(*data);
        browser->paste(dataMap, 0);
    }
}

void MainWindow::showContextMenuAt(QPoint position)
{
    // Restrict menu position to central widget.
    const auto localRect = centralWidget()->rect();
    const auto rect = QRect(
        centralWidget()->mapToGlobal(localRect.topLeft()),
        centralWidget()->mapToGlobal(localRect.bottomRight())
    );
    const QPoint positionInCentralWidget(
        qBound(rect.left(), position.x(), rect.right()),
        qBound(rect.top(), position.y(), rect.bottom())
    );

    m_menuItem->exec(positionInCentralWidget);
}

void MainWindow::showContextMenu()
{
    auto c = browser();
    if (!c)
        return;

    const auto index = c->currentIndex();
    if ( !index.isValid() )
        return;

    const auto itemRect = c->visualRect(index);
    const auto viewportPosition = itemRect.center();
    const auto position = c->mapToGlobal(viewportPosition);
    showContextMenuAt(position);
}

void MainWindow::nextItemFormat()
{
    auto c = browser();
    if (c)
        c->otherItemLoader(true);
}

void MainWindow::previousItemFormat()
{
    auto c = browser();
    if (c)
        c->otherItemLoader(false);
}

void MainWindow::moveUp()
{
    auto c = browser();
    if (c)
        c->move(Qt::Key_Up);
}

void MainWindow::moveDown()
{
    auto c = browser();
    if (c)
        c->move(Qt::Key_Down);
}

void MainWindow::moveToTop()
{
    auto c = browser();
    if (c)
        c->move(Qt::Key_Home);
}

void MainWindow::moveToBottom()
{
    auto c = browser();
    if (c)
        c->move(Qt::Key_End);
}

void MainWindow::onBrowserCreated(ClipboardBrowser *browser)
{
    connect( browser, &ClipboardBrowser::changeClipboard,
             this, &MainWindow::setClipboardAndSelection );
    connect( browser, &ClipboardBrowser::requestShow,
             this, &MainWindow::showBrowser );
    connect( browser, &ClipboardBrowser::error,
             this, &MainWindow::showError );
    connect( browser, &QAbstractItemView::doubleClicked,
             this, &MainWindow::activateCurrentItem );
    connect( browser, &ClipboardBrowser::itemCountChanged,
             ui->tabWidget, &TabWidget::setTabItemCount );
    connect( browser, &ClipboardBrowser::showContextMenu,
             this, &MainWindow::showContextMenuAt );
    connect( browser, &ClipboardBrowser::itemSelectionChanged,
             this, &MainWindow::onItemSelectionChanged );
    connect( browser, &ClipboardBrowser::itemsChanged,
             this, &MainWindow::onItemsChanged );
    connect( browser, &ClipboardBrowser::internalEditorStateChanged,
             this, &MainWindow::onInternalEditorStateChanged );
    connect( browser, &ClipboardBrowser::searchRequest,
             this, &MainWindow::findNextOrPrevious );
    connect( browser, &ClipboardBrowser::searchHideRequest,
             ui->searchBar, &Utils::FilterLineEdit::hide );
    connect( browser, &ClipboardBrowser::itemWidgetCreated,
             this, &MainWindow::onItemWidgetCreated );
}

void MainWindow::onItemSelectionChanged(const ClipboardBrowser *browser)
{
    if (browser == this->browser())
        updateContextMenu(0);
}

void MainWindow::onItemsChanged(const ClipboardBrowser *browser)
{
    if (browser == this->browser())
        updateContextMenu(contextMenuUpdateIntervalMsec);
    if (browser == getTabForTrayMenu())
        updateTrayMenu();
}

void MainWindow::onInternalEditorStateChanged(const ClipboardBrowser *browser)
{
    if (browser == this->browser())
        updateContextMenu(0);
}

void MainWindow::onNotificationButtonClicked(const NotificationButton &button)
{
    const QString mimeNotificationData = COPYQ_MIME_PREFIX "notification-data";

    QVariantMap data;
    data.insert(mimeNotificationData, button.data);

    Command cmd;
    cmd.cmd = button.script;
    cmd.input = mimeNotificationData;

    action(data, cmd, QModelIndex());
}

void MainWindow::onItemWidgetCreated(const PersistentDisplayItem &item)
{
    if ( m_displayCommands.isEmpty() )
        return;

    m_displayItemList.append(item);
    if (!m_currentDisplayAction)
        runDisplayCommands();
}

void MainWindow::onDisplayActionFinished()
{
    clearHiddenDisplayData();
    runDisplayCommands();
}

void MainWindow::onActionDialogAccepted(const Command &command, const QStringList &arguments, const QVariantMap &data)
{
    auto act = new Action();
    act->setCommand(command.cmd, arguments);
    act->setInputWithFormat(data, command.input);
    act->setName(command.name);
    act->setData(data);

    if ( !command.output.isEmpty() ) {
        if ( !command.sep.isEmpty() )
            actionOutput(this, act, command.output, command.outputTab, QRegExp(command.sep));
        else
            actionOutput(this, act, command.output, command.outputTab);
    }

    m_actionHandler->action(act);
}

void MainWindow::runDisplayCommands()
{
    if ( m_displayItemList.isEmpty() )
        return;

    m_currentDisplayItem = m_displayItemList.takeFirst();

    m_currentDisplayAction = runScript("runDisplayCommands()", m_currentDisplayItem.data());
    connect( m_currentDisplayAction.data(), &QObject::destroyed,
             this, &MainWindow::onDisplayActionFinished );
}

void MainWindow::clearHiddenDisplayData()
{
    for (int i = m_displayItemList.size() - 1; i >= 0; --i) {
        auto &item = m_displayItemList[i];
        if ( !item.isValid() )
            m_displayItemList.removeAt(i);
    }
}

void MainWindow::reloadBrowsers()
{
    for( int i = 0; i < ui->tabWidget->count(); ++i )
        getPlaceholder(i)->reloadBrowser();
}

int MainWindow::findTabIndexExactMatch(const QString &name)
{
    TabWidget *w = ui->tabWidget;

    for( int i = 0; i < w->count(); ++i ) {
        if ( name == w->tabName(i) )
            return i;
    }

    return -1;
}

void MainWindow::setClipboardData(const QVariantMap &data)
{
    m_clipboardData = data;
    updateContextMenu(contextMenuUpdateIntervalMsec);
    updateTrayMenu();
}

void MainWindow::setFilter(const QString &text)
{
    ui->searchBar->setText(text);
    getPlaceholder()->setFocus();
}

QString MainWindow::filter() const
{
    return ui->searchBar->text();
}

void MainWindow::updateNotifications()
{
    if (m_notifications == nullptr) {
        m_notifications = new NotificationDaemon(this);
        connect( m_notifications, &NotificationDaemon::notificationButtonClicked,
                 this, &MainWindow::onNotificationButtonClicked );
    }

    notificationDaemon()->setNotificationOpacity( theme().color("notification_bg").alphaF() );
    notificationDaemon()->setNotificationStyleSheet( theme().getNotificationStyleSheet() );

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

ClipboardBrowserPlaceholder *MainWindow::getPlaceholder(int index) const
{
    return qobject_cast<ClipboardBrowserPlaceholder*>( ui->tabWidget->widget(index) );
}

ClipboardBrowserPlaceholder *MainWindow::getPlaceholder() const
{
    return qobject_cast<ClipboardBrowserPlaceholder*>( ui->tabWidget->currentWidget() );
}

void MainWindow::delayedUpdateForeignFocusWindows()
{
    if ( isActiveWindow() || m_trayMenu->isActiveWindow() || m_menu->isActiveWindow() )
        m_timerUpdateFocusWindows.stop();
    else
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
    if (m_notifications == nullptr)
        updateNotifications();

    return m_notifications;
}

ClipboardBrowserPlaceholder *MainWindow::createTab(
        const QString &name, TabNameMatching nameMatch)
{
    Q_ASSERT( !name.isEmpty() );

    const int i = nameMatch == MatchExactTabName
            ? findTabIndexExactMatch(name)
            : findTabIndex(name);

    if (i != -1)
        return getPlaceholder(i);

    auto placeholder = new ClipboardBrowserPlaceholder(name, m_sharedData, this);
    connect( placeholder, &ClipboardBrowserPlaceholder::browserCreated,
             this, &MainWindow::onBrowserCreated );

    ui->tabWidget->addTab(placeholder, name);
    saveTabPositions();
    return placeholder;
}

template <typename SlotReturnType>
QAction *MainWindow::createAction(int id, MainWindowActionSlot<SlotReturnType> slot, QMenu *menu)
{
    QAction *act = actionForMenuItem(id, this, Qt::WindowShortcut);
    connect(act, &QAction::triggered, this, slot, Qt::UniqueConnection);
    if (menu)
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

template <typename Receiver, typename ReturnType>
QAction *MainWindow::addItemAction(int id, Receiver *receiver, ReturnType (Receiver::* slot)())
{
    QAction *act = actionForMenuItem(id, getPlaceholder(), Qt::WidgetWithChildrenShortcut);
    connect( act, &QAction::triggered, receiver, slot, Qt::UniqueConnection );
    m_menuItem->addAction(act);
    return act;
}

QVector<Command> MainWindow::commandsForMenu(const QVariantMap &data, const QString &tabName)
{
    QVector<Command> commands;
    for (const auto &command : m_menuCommands) {
        if ( canExecuteCommand(command, data, tabName) ) {
            Command cmd = command;
            if ( cmd.outputTab.isEmpty() )
                cmd.outputTab = tabName;
            commands.append(cmd);
        }
    }

    return commands;
}

void MainWindow::addCommandsToItemMenu(ClipboardBrowser *c)
{
    m_itemMenuMatchCommands = MenuMatchCommands();

    if ( m_menuCommands.isEmpty() )
        return;

    const auto data = addSelectionData(*c);
    const auto commands = commandsForMenu(data, c->tabName());

    QList<QKeySequence> usedShortcuts = m_disabledShortcuts;
    QList<QKeySequence> uniqueShortcuts;

    for (const auto &command : commands) {
        QString name = command.name;
        QMenu *currentMenu = createSubMenus(&name, m_menuItem);
        auto act = new CommandAction(command, name, currentMenu);
        c->addAction(act);

        addMenuMatchCommand(&m_itemMenuMatchCommands, command.matchCmd, act);

        connect(act, &CommandAction::triggerCommand,
                this, &MainWindow::onItemCommandActionTriggered);

        uniqueShortcuts.clear();

        for (const auto &shortcutText : command.shortcuts) {
            const QKeySequence shortcut(shortcutText, QKeySequence::PortableText);
            if ( !shortcut.isEmpty() && !usedShortcuts.contains(shortcut) ) {
                if (act->isEnabled())
                    usedShortcuts.append(shortcut);
                uniqueShortcuts.append(shortcut);

                if ( !isItemMenuDefaultActionValid() && isItemActivationShortcut(shortcut) )
                    m_menuItem->setDefaultAction(act);
            }
        }

        if (!uniqueShortcuts.isEmpty())
            act->setShortcuts(uniqueShortcuts);
    }

    setDisabledShortcuts(usedShortcuts);

    runMenuCommandFilters(&m_itemMenuMatchCommands, data);
}

void MainWindow::addCommandsToTrayMenu(const QVariantMap &clipboardData)
{
    m_trayMenuMatchCommands = MenuMatchCommands();

    if ( m_menuCommands.isEmpty() )
        return;

    auto c = getTabForTrayMenu();
    if (!c) {
        c = getPlaceholder()->createBrowser();
        if (!c)
            return;
    }

    // Pass current window title to commands in tray menu.
    auto data = clipboardData;
    if (m_lastWindow)
        data.insert( mimeWindowTitle, m_lastWindow->getTitle() );

    const auto commands = commandsForMenu(data, c->tabName());

    for (const auto &command : commands) {
        QString name = command.name;
        QMenu *currentMenu = createSubMenus(&name, m_trayMenu);
        auto act = new CommandAction(command, name, currentMenu);

        addMenuMatchCommand(&m_trayMenuMatchCommands, command.matchCmd, act);

        connect(act, &CommandAction::triggerCommand,
                this, &MainWindow::onClipboardCommandActionTriggered);
    }

    runMenuCommandFilters(&m_trayMenuMatchCommands, data);
}

void MainWindow::addMenuMatchCommand(MenuMatchCommands *menuMatchCommands, const QString &matchCommand, QAction *act)
{
    if ( !matchCommand.isEmpty() ) {
        act->setDisabled(true);
        menuMatchCommands->matchCommands.append(matchCommand);
        menuMatchCommands->actions.append(act);
    }
}

void MainWindow::runMenuCommandFilters(MenuMatchCommands *menuMatchCommands, const QVariantMap &data)
{
    if ( !menuMatchCommands->actions.isEmpty() ) {
        const auto act = runScript("runMenuCommandFilters()", data);
        menuMatchCommands->actionId = act->id();
    }
}

bool MainWindow::isItemMenuDefaultActionValid() const
{
    const auto defaultAction = m_menuItem->defaultAction();
    return defaultAction != nullptr && defaultAction->isEnabled();
}

void MainWindow::updateToolBar()
{
    ui->toolBar->clear();
    ui->toolBar->setEnabled(true);
    ui->toolBar->setUpdatesEnabled(true);

    if ( ui->toolBar->isHidden() )
        return;

    QAction *act = actionForMenuItem(Actions::File_New, this, Qt::WindowShortcut);
    ui->toolBar->addAction(act);

    for ( auto action : m_menuItem->actions() ) {
        if ( action->isSeparator() ) {
            ui->toolBar->addSeparator();
        } else if ( !action->icon().isNull() ) {
            const QIcon icon = action->icon();
            const QString text = action->text().remove("&");
            const QString shortcut = action->shortcut().toString(QKeySequence::NativeText);
            const QString label = text + (shortcut.isEmpty() ? QString() : "\n[" + shortcut + "]");
            const QString tooltip = "<center>" + escapeHtml(text)
                    + (shortcut.isEmpty() ? QString() : "<br /><b>" + escapeHtml(shortcut) + "</b>") + "</center>";
            act = ui->toolBar->addAction(icon, label);
            connect(act, &QAction::triggered, action, &QAction::triggered);
            act->setToolTip(tooltip);

            act->setEnabled(action->isEnabled());
            connect( action, &QAction::changed,
                     act, [=]() { act->setEnabled(action->isEnabled()); } );

            if ( action->isCheckable() ) {
                act->setCheckable(true);
                act->setChecked(action->isChecked());
                connect( act, &QAction::triggered,
                         action, &QAction::setChecked );
                connect( action, &QAction::toggled,
                         act, &QAction::setChecked );
            }
        }
    }
}

void MainWindow::initTray()
{
    if ( AppConfig().option<Config::disable_tray>() ) {
        if (m_tray) {
            // Hide tray on Ubuntu (buggy sni-qt)
            m_tray->hide();

            delete m_tray;
            m_tray = nullptr;
        }

        m_timerTrayAvailable.stop();
    } else if (!m_tray) {
        createTrayIfSupported();
    }

    if (closeMinimizes() && isHidden() && !isMinimized())
        minimizeWindow();
}

bool MainWindow::isWindowVisible() const
{
    return !isMinimized() && isVisible() && m_isActiveWindow;
}

void MainWindow::onEscape()
{
    if ( browseMode() ) {
        hideWindow();
        auto c = browser();
        if (c)
            c->setCurrent(0);
    } else {
        enterBrowseMode();
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
    for (const auto &shortcut : m_disabledShortcuts)
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

void MainWindow::addMenuItems(TrayMenu *menu, ClipboardBrowser *c, int maxItemCount, const QString &searchText)
{
    WidgetSizeGuard sizeGuard(menu);
    menu->clearClipboardItems();

    if (!c)
        return;

    const int current = c->currentIndex().row();
    int itemCount = 0;
    for ( int i = 0; i < c->length() && itemCount < maxItemCount; ++i ) {
        const QModelIndex index = c->model()->index(i, 0);
        if ( !searchText.isEmpty() ) {
            const QString itemText = index.data(contentType::text).toString().toLower();
            if ( !itemText.contains(searchText.toLower()) )
                continue;
        }
        menu->addClipboardItemAction(index, m_options.trayImages, i == current);
        ++itemCount;
    }
}

void MainWindow::activateMenuItem(ClipboardBrowser *c, const QVariantMap &data, bool omitPaste)
{
    if ( m_sharedData->moveItemOnReturnKey ) {
        const auto itemHash = ::hash(data);
        c->moveToTop(itemHash);
    }

    if ( QGuiApplication::queryKeyboardModifiers().testFlag(Qt::ShiftModifier) )
        m_clipboardManager.setClipboard( createDataMap(mimeText, data.value(mimeText) ) );
    else
        m_clipboardManager.setClipboard(data);

    if (!m_lastWindow)
        updateFocusWindows();

    PlatformWindowPtr lastWindow = m_lastWindow;

    if ( m_options.trayItemPaste && lastWindow && !omitPaste && canPaste() ) {
        COPYQ_LOG( QString("Pasting item from tray menu to \"%1\".")
                   .arg(lastWindow->getTitle()) );
        lastWindow->pasteClipboard();
    }
}

bool MainWindow::toggleMenu(TrayMenu *menu, QPoint pos)
{
    if ( menu->isVisible() ) {
        menu->close();
        return false;
    }

    menu->popup( toScreen(pos, menu->width(), menu->height()) );

    menu->raise();
    menu->activateWindow();
    QApplication::setActiveWindow(menu);
    QApplication::processEvents();
    stealFocus(*menu);

    return true;
}

bool MainWindow::toggleMenu(TrayMenu *menu)
{
    return toggleMenu(menu, QCursor::pos());
}

bool MainWindow::exportDataFrom(const QString &fileName, const QStringList &tabs, bool exportConfiguration, bool exportCommands)
{
    QFile file(fileName);
    if ( !file.open(QIODevice::WriteOnly | QIODevice::Truncate) )
        return false;

    QDataStream out(&file);
    return exportDataV3(&out, tabs, exportConfiguration, exportCommands);
}

bool MainWindow::exportDataV3(QDataStream *out, const QStringList &tabs, bool exportConfiguration, bool exportCommands)
{
    QVariantList tabsList;
    for (const auto &tab : tabs) {
        const auto i = findTabIndex(tab);
        if (i == -1)
            continue;

        auto placeholder = getPlaceholder(i);
        const bool wasLoaded = placeholder->isDataLoaded();
        auto c = placeholder->createBrowserAgain();
        if (!c) {
            log(QString("Failed to open tab \"%s\" for export").arg(tab), LogError);
            return false;
        }

        const auto &tabName = c->tabName();

        bool saved = false;
        QByteArray tabBytes;
        {
            QDataStream tabOut(&tabBytes, QIODevice::WriteOnly);
            tabOut.setVersion(QDataStream::Qt_4_7);
            saved = serializeData(*c->model(), &tabOut);
        }

        if (!wasLoaded)
            placeholder->expire();

        if (!saved) {
            log(QString("Failed to export tab \"%s\"").arg(tab), LogError);
            return false;
        }

        const auto iconName = getIconNameForTabName(tabName);

        QVariantMap tabMap;
        tabMap["name"] = tabName;
        tabMap["data"] = tabBytes;
        if ( !iconName.isEmpty() )
            tabMap["icon"] = iconName;

        tabsList.append(tabMap);
    }

    QVariantMap settingsMap;
    if (exportConfiguration) {
        const QSettings settings;

        for (const auto &key : settings.allKeys()) {
            if ( !key.startsWith("Commands/") )
                settingsMap[key] = serializableValue(settings, key);
        }
    }

    QVariantList commandsList;
    if (exportCommands) {
        QSettings settings;

        const int commandCount = settings.beginReadArray("Commands");
        commandsList.reserve(commandCount);
        for (int i = 0; i < commandCount; ++i) {
            settings.setArrayIndex(i);

            QVariantMap commandMap;
            for ( const auto &key : settings.allKeys() )
                commandMap[key] = serializableValue(settings, key);

            commandsList.append(commandMap);
        }

        settings.endArray();
    }

    QVariantMap data;
    if ( !tabsList.isEmpty() )
        data["tabs"] = tabsList;
    if ( !settingsMap.isEmpty() )
        data["settings"] = settingsMap;
    if ( !commandsList.isEmpty() )
        data["commands"] = commandsList;

    out->setVersion(QDataStream::Qt_4_7);
    (*out) << QByteArray("CopyQ v3");
    (*out) << data;

    return out->status() == QDataStream::Ok;
}

bool MainWindow::importDataV3(QDataStream *in, ImportOptions options)
{
    QByteArray header;
    (*in) >> header;
    if ( !header.startsWith("CopyQ v3") )
        return false;

    QVariantMap data;
    (*in) >> data;
    if ( in->status() != QDataStream::Ok )
        return false;

    const auto tabsList = data.value("tabs").toList();

    QStringList tabs;
    tabs.reserve( tabsList.size() );
    for (const auto &tabMapValue : tabsList) {
        const auto tabMap = tabMapValue.toMap();
        const auto oldTabName = tabMap["name"].toString();
        tabs.append(oldTabName);
    }

    const auto settingsMap = data.value("settings").toMap();
    const auto commandsList = data.value("commands").toList();

    bool importConfiguration = true;
    bool importCommands = true;

    if (options == ImportOptions::Select) {
        ImportExportDialog importDialog(this);
        importDialog.setWindowTitle( tr("CopyQ Options for Import") );
        importDialog.setTabs(tabs);
        importDialog.setHasConfiguration( !settingsMap.isEmpty() );
        importDialog.setHasCommands( !commandsList.isEmpty() );
        importDialog.setConfigurationEnabled(true);
        importDialog.setCommandsEnabled(true);
        if ( importDialog.exec() != QDialog::Accepted )
            return true;

        tabs = importDialog.selectedTabs();
        importConfiguration = importDialog.isConfigurationEnabled();
        importCommands = importDialog.isCommandsEnabled();
    }

    for (const auto &tabMapValue : tabsList) {
        const auto tabMap = tabMapValue.toMap();
        const auto oldTabName = tabMap["name"].toString();
        if ( !tabs.contains(oldTabName) )
            continue;

        auto tabName = oldTabName;
        renameToUnique( &tabName, ui->tabWidget->tabs() );

        const auto iconName = tabMap.value("icon").toString();
        if ( !iconName.isEmpty() )
            setIconNameForTabName(tabName, iconName);

        auto c = createTab(tabName, MatchExactTabName)->createBrowser();
        if (!c) {
            log(QString("Failed to create tab \"%s\" for import").arg(tabName), LogError);
            return false;
        }

        const auto tabBytes = tabMap.value("data").toByteArray();
        QDataStream tabIn(tabBytes);
        tabIn.setVersion(QDataStream::Qt_4_7);

        // Don't read items based on current value of "maxitems" option since
        // the option can be later also imported.
        const int maxItems = importConfiguration ? Config::maxItems : m_sharedData->maxItems;
        if ( !deserializeData( c->model(), &tabIn, maxItems ) ) {
            log(QString("Failed to import tab \"%s\"").arg(tabName), LogError);
            return false;
        }

        const auto i = findTabIndex(tabName);
        if (i != -1)
            getPlaceholder(i)->expire();
    }

    if (importConfiguration) {
        // Configuration dialog shouldn't be open.
        if (cm) {
            log("Failed to import configuration while configuration dialog is open", LogError);
            return false;
        }

        Settings settings;

        for (auto it = settingsMap.constBegin(); it != settingsMap.constEnd(); ++it)
            settings.setValue( it.key(), it.value() );

        emit configurationChanged();
    }

    if (importCommands) {
        // Close command dialog.
        if ( !maybeCloseCommandDialog() ) {
            log("Failed to import command while command dialog is open", LogError);
            return false;
        }

        // Re-create command dialog again later.
        if (m_commandDialog) {
            m_commandDialog->deleteLater();
            m_commandDialog = nullptr;
        }

        Settings settings;

        int i = settings.beginReadArray("Commands");
        settings.endArray();

        settings.beginWriteArray("Commands");

        for ( const auto &commandDataValue : commandsList ) {
            settings.setArrayIndex(i++);
            const auto commandMap = commandDataValue.toMap();
            for (auto it = commandMap.constBegin(); it != commandMap.constEnd(); ++it)
                settings.setValue( it.key(), it.value() );
        }

        settings.endArray();

        onCommandDialogSaved();
    }

    return in->status() == QDataStream::Ok;
}

void MainWindow::updateCommands()
{
    m_automaticCommands.clear();
    m_menuCommands.clear();
    m_scriptCommands.clear();

    QVector<Command> displayCommands;

    const auto commands = loadEnabledCommands();
    for (const auto &command : commands) {
        const auto type = command.type();

        if (type & CommandType::Automatic)
            m_automaticCommands.append(command);

        if (type & CommandType::Display)
            displayCommands.append(command);

        if (type & CommandType::Menu)
            m_menuCommands.append(command);

        if (type & CommandType::Script)
            m_scriptCommands.append(command);
    }

    if (m_displayCommands != displayCommands) {
        m_displayItemList.clear();
        m_displayCommands = displayCommands;
        reloadBrowsers();
    }

    updateContextMenu(contextMenuUpdateIntervalMsec);
    if (m_options.trayCommands)
        updateTrayMenu();
    emit commandsSaved();
}

void MainWindow::disableHideWindowOnUnfocus()
{
    m_timerHideWindowIfNotActive.disconnect();
}

void MainWindow::enableHideWindowOnUnfocus()
{
    initSingleShotTimer( &m_timerHideWindowIfNotActive, 250, this, &MainWindow::hideWindowIfNotActive );
}

void MainWindow::hideWindowIfNotActive()
{
    if ( isVisible() && !isAnyApplicationWindowActive() )
        hideWindow();
}

const Theme &MainWindow::theme() const
{
    return m_sharedData->theme;
}

Action *MainWindow::runScript(const QString &script, const QVariantMap &data)
{
    auto act = new Action();
    act->setCommand(QStringList() << "copyq" << "eval" << "--" << script);
    act->setData(data);
    runInternalAction(act);
    return act;
}

int MainWindow::findTabIndex(const QString &name)
{
    TabWidget *w = ui->tabWidget;

    const int found = findTabIndexExactMatch(name);
    if (found != -1)
        return found;

    // Ignore key hints ('&').
    if ( !hasKeyHint(name) ) {
        for( int i = 0; i < w->count(); ++i ) {
            QString tabName = w->tabName(i);
            if ( name == removeKeyHint(&tabName) )
                return i;
        }
    }

    return -1;
}

ClipboardBrowser *MainWindow::tab(const QString &name)
{
    return createTab(name, MatchSimilarTabName)->createBrowser();
}

bool MainWindow::hasRunningAction() const
{
    return m_actionHandler->runningActionCount() > 0;
}

bool MainWindow::maybeCloseCommandDialog()
{
    return !m_commandDialog || m_commandDialog->maybeClose(this);
}

void MainWindow::showError(const QString &msg)
{
    auto notification = createNotification();
    notification->setTitle( tr("CopyQ Error", "Notification error message title") );
    notification->setMessage(msg);
    notification->setIcon(IconTimesCircle);
}

Notification *MainWindow::createNotification(const QString &id)
{
    return notificationDaemon()->createNotification(id);
}

void MainWindow::addCommands(const QVector<Command> &commands)
{
    openCommands();
    if (m_commandDialog)
        m_commandDialog->addCommands(commands);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    const int key = event->key();
    const Qt::KeyboardModifiers modifiers = event->modifiers();

    // Search items or text in editor (search previous when Shift is pressed).
    if ( key == Qt::Key_F3
         || (modifiers.testFlag(Qt::ControlModifier) && (key == Qt::Key_F || key == Qt::Key_G)) )
    {
        findNextOrPrevious();
        return;
    }

    auto c = browser();
    if (c && c->isInternalEditorOpen())
        return;

    if (m_options.hideTabs && key == Qt::Key_Alt)
        setHideTabs(false);

    if (m_options.viMode) {
        if (modifiers == Qt::ControlModifier && key == Qt::Key_BracketLeft) {
            onEscape();
            return;
        }

        if (browseMode()) {
            if (c && handleViKey(event, c))
                return;

            switch(key) {
            case Qt::Key_Slash:
                enterSearchMode();
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

    if (m_options.emacsMode) {
      if ((modifiers == Qt::ControlModifier && key == Qt::Key_G)
          || (key == Qt::Key_Escape)) {
        onEscape();
        return;
      }

      if (browseMode()) {
        if (c && handleEmacsKey(event, c))
          return;

        if (key == Qt::Key_S && modifiers == Qt::ControlModifier) {
          enterSearchMode();
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

    if (modifiers == Qt::ControlModifier) {
        switch(key) {
            case Qt::Key_Return:
            case Qt::Key_Enter:
                if (c)
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
            if ( c && !c->hasFocus() )
                c->setFocus();
            break;

        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (c)
                activateCurrentItem();
            else
                getPlaceholder()->createBrowser();
            break;

        case Qt::Key_Tab:
            QMainWindow::keyPressEvent(event);
            break;

#ifndef Q_OS_MAC
        case Qt::Key_Backspace:
            // fallthrough
#endif // Q_OS_MAC
        case Qt::Key_Escape:
            onEscape();
            break;

        default:
            QMainWindow::keyPressEvent(event);
            if ( !event->isAccepted() ) {
                const auto txt = event->text();
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
        if ( !isActiveWindow() )
            updateFocusWindows();
        updateWindowTransparency(true);
    } else if (type == QEvent::Leave) {
        updateWindowTransparency(false);
        setHideTabs(m_options.hideTabs);
    } else if (type == QEvent::WindowActivate) {
        m_isActiveWindow = true;
        if ( !isActiveWindow() )
            updateFocusWindows();
        updateWindowTransparency();
        enableHideWindowOnUnfocus();
    } else if (type == QEvent::WindowDeactivate) {
        updateWindowTransparency();
        setHideTabs(m_options.hideTabs);
        if (m_options.closeOnUnfocus)
            m_timerHideWindowIfNotActive.start();
        m_timerUpdateFocusWindows.start();
    } else if (type == QEvent::Hide) {
        m_wasMaximized = isMaximized();
    }

    return QMainWindow::event(event);
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
    delayedUpdateForeignFocusWindows();
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::loadSettings()
{
    COPYQ_LOG("Loading configuration");

    QSettings settings;
    m_sharedData->itemFactory->loadItemFactorySettings(&settings);

    settings.beginGroup("Theme");
    m_sharedData->theme.loadTheme(settings);
    settings.endGroup();

    theme().decorateToolBar(ui->toolBar);
    theme().decorateMainWindow(this);
    ui->scrollAreaItemPreview->setObjectName("ClipboardBrowser");
    theme().decorateItemPreview(ui->scrollAreaItemPreview);

    setUseSystemIcons( theme().useSystemIcons() );

    AppConfig appConfig;

    m_options.confirmExit = appConfig.option<Config::confirm_exit>();

    // always on top window hint
    bool alwaysOnTop = appConfig.option<Config::always_on_top>();
    setAlwaysOnTop(this, alwaysOnTop);
    setAlwaysOnTop(m_commandDialog.data(), alwaysOnTop);

    // Vi mode
    m_options.viMode = appConfig.option<Config::vi>();
    m_trayMenu->setViModeEnabled(m_options.viMode);
    m_menu->setViModeEnabled(m_options.viMode);

    // Emacs mode
    m_options.emacsMode = appConfig.option<Config::emacs>();
    m_trayMenu->setEmacsModeEnabled(m_options.emacsMode);
    m_menu->setEmacsModeEnabled(m_options.emacsMode);

    // Number search
    m_sharedData->numberSearch = appConfig.option<Config::number_search>();
    m_trayMenu->setNumberSearchEnabled(m_sharedData->numberSearch);
    m_menu->setNumberSearchEnabled(m_sharedData->numberSearch);

    m_options.transparency = appConfig.option<Config::transparency>();
    m_options.transparencyFocused = appConfig.option<Config::transparency_focused>();
    updateWindowTransparency();

    // save unsaved tab data
    if ( ui->tabWidget->count() != 0 ) {
        if ( m_timerSaveTabPositions.isActive() )
            doSaveTabPositions();
        ui->tabWidget->saveTabInfo();
    }

    // tab bar position
    const bool tabTreeEnabled = appConfig.option<Config::tab_tree>();
    ui->tabWidget->setTreeModeEnabled(tabTreeEnabled);
    ui->tabWidget->setTabItemCountVisible(appConfig.option<Config::show_tab_item_count>());
    for ( auto scrollArea : ui->tabWidget->toolBar()->findChildren<QAbstractScrollArea*>() )
        theme().decorateScrollArea(scrollArea);

    m_options.hideTabs = appConfig.option<Config::hide_tabs>();
    setHideTabs(m_options.hideTabs);

    bool hideToolbar = appConfig.option<Config::hide_toolbar>();
    ui->toolBar->clear();
    ui->toolBar->setHidden(hideToolbar);
    bool hideToolBarLabels = appConfig.option<Config::hide_toolbar_labels>();
    ui->toolBar->setToolButtonStyle(hideToolBarLabels ? Qt::ToolButtonIconOnly
                                                      : Qt::ToolButtonTextUnderIcon);

    m_options.hideMainWindow = appConfig.option<Config::hide_main_window>();
    m_options.closeOnUnfocus = appConfig.option<Config::close_on_unfocus>();

    // shared data for browsers
    m_sharedData->editor = appConfig.option<Config::editor>();
    m_sharedData->maxItems = appConfig.option<Config::maxitems>();
    m_sharedData->textWrap = appConfig.option<Config::text_wrap>();
    m_sharedData->viMode = appConfig.option<Config::vi>();
    m_sharedData->emacsMode = appConfig.option<Config::emacs>();
    m_sharedData->saveOnReturnKey = !appConfig.option<Config::edit_ctrl_return>();
    m_sharedData->moveItemOnReturnKey = appConfig.option<Config::move>();
    m_sharedData->showSimpleItems = appConfig.option<Config::show_simple_items>();
    m_sharedData->minutesToExpire = appConfig.option<Config::expire_tab>();

    reloadBrowsers();

    // create tabs
    const QStringList tabs = savedTabs();
    for (const auto &name : tabs)
        createTab(name, MatchExactTabName);

    Q_ASSERT( ui->tabWidget->count() > 0 );
    setTabs(tabs); // Save any tabs loaded from new tab files.

    ui->tabWidget->updateTabs();

    m_timerSaveTabPositions.stop();

    updateContextMenu(contextMenuUpdateIntervalMsec);

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
    m_options.trayMenuOpenOnLeftClick = appConfig.option<Config::tray_menu_open_on_left_click>();
    m_options.clipboardTab = appConfig.option<Config::clipboard_tab>();

    m_trayMenu->setStyleSheet( theme().getToolTipStyleSheet() );
    m_menu->setStyleSheet( theme().getToolTipStyleSheet() );

    initTray();
    updateTrayMenu();

    if (m_notifications != nullptr)
        updateNotifications();

    updateIcon();

    ui->searchBar->loadSettings();

    settings.beginGroup("Shortcuts");
    loadShortcuts(&m_menuItems, settings);
    settings.endGroup();

    enterBrowseMode();

    COPYQ_LOG("Configuration loaded");
}

void MainWindow::openHelp()
{
    QDesktopServices::openUrl( QUrl("https://copyq.readthedocs.io") );
}

void MainWindow::showWindow()
{
    if ( isWindowVisible() )
        return;

    m_trayMenu->close();
    m_menu->close();

    updateFocusWindows();

    moveToCurrentWorkspace(this);

    if (m_wasMaximized || isMaximized())
        showMaximized();
    else
        showNormal();
    raise();
    activateWindow();

    auto c = browser();
    if (c) {
        if ( !c->isInternalEditorOpen() )
            c->scrollTo( c->currentIndex() );
        c->setFocus();
    }

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
    if ( isWindowVisible() ) {
        hideWindow();
        return false;
    }

    showWindow();
    return true;
}

void MainWindow::setCurrentTab(const ClipboardBrowser *browser)
{
    int i = 0;
    for( ; i < ui->tabWidget->count() && getPlaceholder(i)->browser() != browser; ++i ) {}
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

void MainWindow::onMenuActionTriggered(const QVariantMap &data, bool omitPaste)
{
    activateMenuItem( getTabForMenu(), data, omitPaste );
}

void MainWindow::onTrayActionTriggered(const QVariantMap &data, bool omitPaste)
{
    activateMenuItem( getTabForTrayMenu(), data, omitPaste );
}

void MainWindow::trayActivated(QSystemTrayIcon::ActivationReason reason)
{
#ifdef Q_OS_MAC
    // Don't do this on OS X, we only ever get "Trigger"
    Q_UNUSED(reason);
#else
    if ( reason == QSystemTrayIcon::MiddleClick
         || (m_options.trayMenuOpenOnLeftClick && reason == QSystemTrayIcon::Trigger) )
    {
        toggleMenu();
    } else if ( reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick ) {
        // Like toggleVisible() but hide window if visible and not focused
        // (this seems better behavior when using mouse).
        if (!isMinimized() && isVisible())
            hideWindow();
        else
            showWindow();

    }
#endif // Q_OS_MAC
}

bool MainWindow::toggleMenu()
{
    m_trayMenu->search(QString());

    if ( !m_trayMenu->isVisible() && m_timerUpdateTrayMenu.isActive() ) {
        m_timerUpdateTrayMenu.stop();
        updateTrayMenuTimeout();
    }

    return toggleMenu(m_trayMenu);
}

bool MainWindow::toggleMenu(const QString &tabName, int itemCount, QPoint position)
{
    // Just close the previously opened menu if parameters are the same.
    if ( m_menu->isVisible()
         && (m_menuTabName == tabName && m_menuMaxItemCount == itemCount) )
    {
        m_menu->close();
        return false;
    }

    WidgetSizeGuard sizeGuard(m_menu);

    m_menuTabName = tabName;
    m_menuMaxItemCount = itemCount;
    if (m_menuMaxItemCount < 0)
        m_menuMaxItemCount = m_options.trayItems > 0 ? m_options.trayItems : 10;

    m_menu->clearAllActions();
    filterMenuItems(QString());

    if ( m_menu->isVisible() )
        m_menu->close();

    if ( m_menu->isEmpty() )
        return false;

    if (position.x() >= 0 && position.y() >= 0)
        return toggleMenu(m_menu, position);

    return toggleMenu(m_menu);
}

void MainWindow::tabChanged(int current, int)
{
    bool currentIsTabGroup = current == -1;

    emit tabGroupSelected(currentIsTabGroup);

    if (!currentIsTabGroup) {
        // update item menu (necessary for keyboard shortcuts to work)
        auto c = browser();
        if (c) {
            c->filterItems( browseMode() ? QRegExp() : ui->searchBar->filter() );

            if ( current >= 0 ) {
                if( !c->currentIndex().isValid() && isVisible() ) {
                    c->setCurrent(0);
                }
            }

            setTabOrder(ui->searchBar, c);
        }
    }

    updateContextMenu(0);

    if (m_options.trayCurrentTab)
        updateTrayMenu();
}

void MainWindow::saveTabPositions()
{
    m_timerSaveTabPositions.start();
}

void MainWindow::doSaveTabPositions()
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
        auto placeholder = getPlaceholder(i);
        const QString oldTabName = placeholder->tabName();

        if ( (oldTabName == oldPrefix || oldTabName.startsWith(prefix)) && newPrefix != oldPrefix) {
            const QString newName = newPrefix + oldTabName.mid(oldPrefix.size());
            updateTabIcon(newName, placeholder->tabName());
            placeholder->setTabName(newName);
            auto c = placeholder->browser();
            if (c)
                ui->tabWidget->setTabItemCount( newName, c->length() );
        }
    }

    saveTabPositions();
}

void MainWindow::tabBarMenuRequested(QPoint pos, int tab)
{
    auto placeholder = getPlaceholder(tab);
    if (placeholder == nullptr)
        return;
    const QString tabName = placeholder->tabName();
    popupTabBarMenu(pos, tabName);
}

void MainWindow::tabTreeMenuRequested(QPoint pos, const QString &groupPath)
{
    popupTabBarMenu(pos, groupPath);
}

void MainWindow::tabCloseRequested(int tab)
{
    removeTab(true, tab);
}

QVariant MainWindow::config(const QStringList &nameValue)
{
    ConfigurationManager configurationManager;

    if ( nameValue.isEmpty() ) {
        QStringList options = configurationManager.options();
        options.sort();
        QString opts;
        for (const auto &option : options)
            opts.append( option + "\n  " + configurationManager.optionToolTip(option).replace('\n', "\n  ") + '\n' );
        return opts;
    }

    QStringList unknownOptions;
    const auto validOptions = configurationManager.options();

    // Check if option names are valid.
    for (int i = 0; i < nameValue.size(); i += 2) {
        const auto &name = nameValue[i];
        if ( !validOptions.contains(name) )
            unknownOptions.append( nameValue[i] );
    }

    if ( !unknownOptions.isEmpty() )
        return unknownOptions;

    configurationManager.loadSettings();

    QVariantMap result;
    bool emitConfigurationChanged = false;
    for (int i = 0; i < nameValue.size(); i += 2) {
        const auto &name = nameValue[i];
        const auto value = nameValue.value(i + 1);
        if ( !value.isNull() && configurationManager.setOptionValue(name, value) )
            emitConfigurationChanged = true;

        result.insert( nameValue[i], configurationManager.optionValue(name) );
    }

    if (emitConfigurationChanged) {
        configurationManager.setAutostartEnable();
        emit configurationChanged();
    }

    return result;
}

QVariantMap MainWindow::actionData(int id) const
{
    return m_actionHandler->actionData(id);
}

void MainWindow::setActionData(int id, const QVariantMap &data)
{
    m_actionHandler->setActionData(id, data);
}

void MainWindow::setCommands(const QVector<Command> &commands)
{
    if ( !maybeCloseCommandDialog() )
        return;

    saveCommands(commands);
    updateCommands();
}

void MainWindow::setSessionIconColor(QColor color)
{
    ::setSessionIconColor(color);
    updateIcon();
}

void MainWindow::setSessionIconTag(const QString &tag)
{
    ::setSessionIconTag(tag);
    updateIcon();
}

void MainWindow::setSessionIconTagColor(QColor color)
{
    ::setSessionIconTagColor(color);
    updateIcon();
}

QColor MainWindow::sessionIconColor() const
{
    return ::sessionIconColor();
}

QString MainWindow::sessionIconTag() const
{
    return ::sessionIconTag();
}

QColor MainWindow::sessionIconTagColor() const
{
    return ::sessionIconTagColor();
}

void MainWindow::setTrayTooltip(const QString &tooltip)
{
    if (m_tray)
        m_tray->setToolTip(tooltip);
}

QStringList MainWindow::menuItemMatchCommands(int actionId)
{
    if (actionId == m_itemMenuMatchCommands.actionId)
        return m_itemMenuMatchCommands.matchCommands;

    if (actionId == m_trayMenuMatchCommands.actionId)
        return m_trayMenuMatchCommands.matchCommands;

    return QStringList();
}

bool MainWindow::setMenuItemEnabled(int actionId, int menuItemMatchCommandIndex, bool enabled)
{
    if (actionId != m_trayMenuMatchCommands.actionId && actionId != m_itemMenuMatchCommands.actionId)
        return false;

    const auto &menuMatchCommands = actionId == m_trayMenuMatchCommands.actionId
            ? m_trayMenuMatchCommands
            : m_itemMenuMatchCommands;

    if (menuMatchCommands.actions.size() <= menuItemMatchCommandIndex)
        return false;

    auto action = menuMatchCommands.actions[menuItemMatchCommandIndex];
    if (!action)
        return true;

    action->setEnabled(enabled);

    if (enabled) {
        const auto shortcuts = action->shortcuts();
        if ( !shortcuts.isEmpty() ) {
            setDisabledShortcuts(m_disabledShortcuts + shortcuts);

            if ( !isItemMenuDefaultActionValid() ) {
                for (const auto &shortcut : shortcuts) {
                    if ( isItemActivationShortcut(shortcut) ) {
                        m_menuItem->setDefaultAction(action);
                        break;
                    }
                }
            }
        }
    } else if ( actionId == m_trayMenuMatchCommands.actionId || !m_menuItem->isVisible() ) {
        action->deleteLater();
    }

    return true;
}

QVariantMap MainWindow::setDisplayData(int actionId, const QVariantMap &data)
{
    if (!m_currentDisplayAction || m_currentDisplayAction->id() != actionId)
        return QVariantMap();

    m_currentDisplayItem.setData(data);

    clearHiddenDisplayData();

    if ( m_displayItemList.isEmpty() )
        return QVariantMap();

    m_currentDisplayItem = m_displayItemList.takeFirst();
    m_actionHandler->setActionData(actionId, m_currentDisplayItem.data());
    return m_currentDisplayItem.data();
}

void MainWindow::snip()
{
    m_forceIconSnip = true;
    updateIconSnip();
}

void MainWindow::nextTab()
{
    ui->tabWidget->nextTab();
}

void MainWindow::previousTab()
{
    ui->tabWidget->previousTab();
}

void MainWindow::setClipboard(const QVariantMap &data, ClipboardMode mode)
{
    m_clipboardManager.setClipboard(data, mode);
}

void MainWindow::setClipboardAndSelection(const QVariantMap &data)
{
    m_clipboardManager.setClipboard(data);
}

void MainWindow::moveToClipboard(ClipboardBrowser *c, int row)
{
    if (c) {
        const auto index = c->index(row);
        if ( index.isValid() )
            c->moveToClipboard(index);
        else
            m_clipboardManager.setClipboard(QVariantMap());
    } else {
        m_clipboardManager.setClipboard(QVariantMap());
    }
}

void MainWindow::activateCurrentItem()
{
    // Omit activating item multiple times in quick succession.
    if (m_activatingItem)
        return;

    m_activatingItem = true;
    activateCurrentItemHelper();
    m_activatingItem = false;
}

void MainWindow::activateCurrentItemHelper()
{
    if ( QApplication::queryKeyboardModifiers() == Qt::NoModifier
         && isItemMenuDefaultActionValid() )
    {
        m_menuItem->defaultAction()->trigger();
        return;
    }

    auto c = browser();
    if (!c)
        return;

    // Perform custom actions on item activation.
    PlatformWindowPtr lastWindow = m_lastWindow;
    const bool paste = lastWindow && m_options.activatePastes() && canPaste();
    const bool activateWindow = paste || (lastWindow && m_options.activateFocuses());

    // Copy current item or selection to clipboard.
    // While clipboard is being set (in separate process)
    // activate target window for pasting.
    c->moveToClipboard();

    if ( m_options.activateCloses() )
        hideWindow();

    if (activateWindow)
        lastWindow->raise();

    enterBrowseMode();

    if (paste) {
        COPYQ_LOG( QString("Pasting item from main window to \"%1\".")
                   .arg(lastWindow->getTitle()) );
        lastWindow->pasteClipboard();
    }
}

void MainWindow::disableClipboardStoring(bool disable)
{
    if (m_clipboardStoringDisabled == disable)
        return;

    m_clipboardStoringDisabled = disable;
    emit disableClipboardStoringRequest(disable);

    updateMonitoringActions();
    updateIconSnip();

    if (m_clipboardStoringDisabled)
        runScript("setTitle(); showDataNotification()");

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

QStringList MainWindow::tabs() const
{
    return ui->tabWidget->tabs();
}

ClipboardBrowser *MainWindow::getTabForMenu()
{
    const auto i = findTabIndex(m_menuTabName);
    return i != -1 ? browser(i) : nullptr;
}

ClipboardBrowser *MainWindow::getTabForTrayMenu()
{
    if (m_options.trayCurrentTab)
        return browser();

    if ( m_options.trayTabName.isEmpty() )
        return m_options.clipboardTab.isEmpty() ? nullptr : tab(m_options.clipboardTab);

    int i = findTabIndex(m_options.trayTabName);
    return i != -1 ? browser(i) : nullptr;
}

void MainWindow::onFilterChanged(const QRegExp &re)
{
    if (re.isEmpty())
        enterBrowseMode();
    else if ( browseMode() )
        enterSearchMode();

    auto c = browser();
    if (c)
        c->filterItems(re);
    updateItemPreview();
}

void MainWindow::createTrayIfSupported()
{
    if ( QSystemTrayIcon::isSystemTrayAvailable() ) {
        Q_ASSERT(!m_tray);
        m_tray = new QSystemTrayIcon(this);
        connect( m_tray, &QSystemTrayIcon::activated,
                 this, &MainWindow::trayActivated );
        updateIcon();
        m_tray->setContextMenu(m_trayMenu);
        m_tray->show();

        if ( isMinimized() )
            hideWindow();
    } else {
        m_timerTrayAvailable.start();
    }
}

void MainWindow::raiseLastWindowAfterMenuClosed()
{
    if ( m_lastWindow && !isAnyApplicationWindowActive() )
        m_lastWindow->raise();
}

void MainWindow::updateFocusWindows()
{
    m_isActiveWindow = isActiveWindow();

    if ( QApplication::activePopupWidget() )
        return;

    PlatformPtr platform = createPlatformNativeInterface();
    PlatformWindowPtr lastWindow = platform->getCurrentWindow();
    if (lastWindow) {
        COPYQ_LOG( QString("Focus window is \"%1\"").arg(lastWindow->getTitle()) );
        m_lastWindow = lastWindow;
    }
}

void MainWindow::updateShortcuts()
{
    if ( m_timerUpdateContextMenu.isActive() ) {
        m_timerUpdateContextMenu.stop();
        updateContextMenuTimeout();
    }
}

void MainWindow::findNextOrPrevious()
{
    if (browseMode()) {
        enterSearchMode();
    } else {
        auto c = browser();
        if (!c)
            return;

        const bool next = !QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier);
        if ( c->isInternalEditorOpen() ) {
            ui->searchBar->setFocus(Qt::ShortcutFocusReason);
            if (next)
                c->findNext();
            else
                c->findPrevious();
        } else {
            c->setFocus();
            c->setCurrent( c->currentIndex().row() + (next ? 1 : -1) );
        }
    }
}

void MainWindow::enterBrowseMode()
{
    getPlaceholder()->setFocus();
    ui->searchBar->hide();

    auto c = browser();
    if (c)
        c->filterItems(QRegExp());
}

void MainWindow::enterSearchMode()
{
    ui->searchBar->show();
    ui->searchBar->setFocus(Qt::ShortcutFocusReason);

    if ( !ui->searchBar->text().isEmpty() ) {
        auto c = browser();
        if (c) {
            const int currentRow = c->currentIndex().row();
            c->filterItems( ui->searchBar->filter() );
            c->setCurrent(currentRow);
        }
    }
}

void MainWindow::enterSearchMode(const QString &txt)
{
    const bool searchModeActivated = !ui->searchBar->isVisible();

    ui->searchBar->show();
    ui->searchBar->setFocus(Qt::ShortcutFocusReason);

    if (searchModeActivated)
        ui->searchBar->setText(txt);
    else
        ui->searchBar->setText( ui->searchBar->text() + txt );

    auto c = browser();
    if (c)
        c->filterItems( ui->searchBar->filter() );
}

void MainWindow::updateTrayMenuTimeout()
{
    // Update tray only if not currently visible.
    if ( m_trayMenu->isVisible() ) {
        updateTrayMenu();
        return;
    }

    WidgetSizeGuard sizeGuard(m_trayMenu);

    m_trayMenuMatchCommands = MenuMatchCommands();
    m_trayMenu->clearAllActions();

    QAction *act = m_trayMenu->addAction( appIcon(), tr("&Show/Hide") );
    connect(act, &QAction::triggered, this, &MainWindow::toggleVisible);
    m_trayMenu->setDefaultAction(act);
    addTrayAction(Actions::File_Preferences);
    addTrayAction(Actions::File_ToggleClipboardStoring);
    m_trayMenu->addSeparator();
    addTrayAction(Actions::File_Exit);

    filterTrayMenuItems(QString());

    // Add commands.
    if (m_options.trayCommands) {
        // Show clipboard content as disabled item.
        m_trayMenuClipboardAction = m_trayMenu->addAction(iconClipboard(), QString());
        connect(m_trayMenuClipboardAction.data(), &QAction::triggered,
                this, &MainWindow::showClipboardContent);
        m_trayMenu->addCustomAction(m_trayMenuClipboardAction);

        int i = m_trayMenu->actions().size();
        addCommandsToTrayMenu(m_clipboardData);
        QList<QAction *> actions = m_trayMenu->actions();
        for ( ; i < actions.size(); ++i )
            m_trayMenu->addCustomAction(actions[i]);
    }

    updateTrayMenuClipboard();
}

void MainWindow::filterMenuItems(const QString &searchText)
{
    addMenuItems(m_menu, getTabForMenu(), m_menuMaxItemCount, searchText);
}

void MainWindow::filterTrayMenuItems(const QString &searchText)
{
    addMenuItems(m_trayMenu, getTabForTrayMenu(), m_options.trayItems, searchText);
    m_trayMenu->markItemInClipboard(m_clipboardData);
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
    connect( clipboardDialog, &ClipboardDialog::changeClipboard,
             this, &MainWindow::setClipboardAndSelection );
}

void MainWindow::showProcessManagerDialog()
{
    m_actionHandler->showProcessManagerDialog();
}

ActionDialog *MainWindow::openActionDialog(const QVariantMap &data)
{
    auto actionDialog = new ActionDialog(this);
    actionDialog->setAttribute(Qt::WA_DeleteOnClose, true);

    actionDialog->setInputData(data);

    const auto tabs = ui->tabWidget->tabs();
    actionDialog->setOutputTabs(tabs);

    const int currentTabIndex = ui->tabWidget->currentIndex();
    if (currentTabIndex >= 0) {
        const auto currentTab = ui->tabWidget->tabName(currentTabIndex);
        actionDialog->setCurrentTab(currentTab);
    }

    connect( actionDialog, &ActionDialog::accepted,
             this, &MainWindow::onActionDialogAccepted );

    connect( actionDialog, &ActionDialog::saveCommand,
             this, &MainWindow::onSaveCommand );

    actionDialog->show();
    stealFocus(*actionDialog);

    return actionDialog;
}

void MainWindow::openActionDialog()
{
    auto c = browser();
    const auto data = c ? addSelectionData(*c) : QVariantMap();
    openActionDialog(data);
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
    connect( &configurationManager, &ConfigurationManager::configurationChanged,
             this, &MainWindow::configurationChanged );
    connect( &configurationManager, &ConfigurationManager::error,
             this, &MainWindow::showError );
    connect( &configurationManager, &ConfigurationManager::commandsSaved,
             this, &MainWindow::updateCommands );

    // WORKAROUND: Fix drag'n'drop in list in modal dialog for Qt 5.9.2 (QTBUG-63846).
    configurationManager.setWindowModality(Qt::WindowModal);

    cm = &configurationManager;
    configurationManager.exec();
    cm = nullptr;
}

void MainWindow::openCommands()
{
    if ( !isEnabled() )
        return;

    if (m_commandDialog) {
        m_commandDialog->show();
        m_commandDialog->activateWindow();
    } else {
        const QVector<Command> pluginCommands = m_sharedData->itemFactory->commands();
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
        connect(this, &QObject::destroyed, m_commandDialog.data(), &QWidget::close);
        connect(m_commandDialog.data(), &CommandDialog::commandsSaved, this, &MainWindow::onCommandDialogSaved);
    }

    if (cm && cm->isVisible())
        m_commandDialog->setWindowModality(Qt::ApplicationModal);
}

ClipboardBrowser *MainWindow::browser(int index)
{
    return getPlaceholder(index)->createBrowser();
}

ClipboardBrowser *MainWindow::browser()
{
    return browser( ui->tabWidget->currentIndex() );
}

ClipboardBrowser *MainWindow::browserForItem(const QModelIndex &index)
{
    if ( index.isValid() ) {
        auto c = qobject_cast<ClipboardBrowser*>(index.model()->parent());
        Q_ASSERT(c);
        return c;
    }

    return nullptr;
}

void MainWindow::addTab(const QString &name)
{
    createTab(name, MatchExactTabName);
    auto w = ui->tabWidget;
    w->setCurrentIndex( w->count()-1 );
    saveTabPositions();
}

void MainWindow::editNewItem()
{
    auto c = browser( ui->tabWidget->currentIndex() );
    if (!c)
        return;

    showWindow();
    if ( !c->isInternalEditorOpen() ) {
        c->setFocus();
        c->editNew();
    }
}

void MainWindow::pasteItems()
{
    const QMimeData *data = clipboardData();
    if (data == nullptr)
        return;

    auto c = browser();
    if (!c)
        return;

    QModelIndexList list = c->selectionModel()->selectedIndexes();
    std::sort( list.begin(), list.end() );
    const int row = list.isEmpty() ? 0 : list.first().row();
    c->paste( cloneData(*data), row );
}

void MainWindow::copyItems()
{
    auto c = browser();
    if (!c)
        return;

    const QModelIndexList indexes = c->selectionModel()->selectedRows();
    if ( indexes.isEmpty() )
        return;

    const auto data = c->copyIndexes(indexes);
    m_clipboardManager.setClipboard(data);
}

bool MainWindow::saveTab(const QString &fileName, int tabIndex)
{
    QFile file(fileName);
    if ( !file.open(QIODevice::WriteOnly | QIODevice::Truncate) )
        return false;

    QDataStream out(&file);
    out.setVersion(QDataStream::Qt_4_7);

    int i = tabIndex >= 0 ? tabIndex : ui->tabWidget->currentIndex();
    auto c = browser(i);
    if (!c)
        return false;

    out << QByteArray("CopyQ v2") << c->tabName();
    serializeData(*c->model(), &out);

    file.close();

    return true;
}

bool MainWindow::exportData()
{
    ImportExportDialog exportDialog(this);
    exportDialog.setWindowTitle( tr("CopyQ Options for Export") );
    exportDialog.setTabs( ui->tabWidget->tabs() );
    if ( !ui->tabWidget->isTabGroupSelected() )
        exportDialog.setCurrentTab( getPlaceholder()->tabName() );
    if ( exportDialog.exec() != QDialog::Accepted )
        return false;

    auto fileName = QFileDialog::getSaveFileName(
                this, QString(), QString(), importExportFileDialogFilter() );
    if ( fileName.isNull() )
        return false;

    if ( !fileName.endsWith(".cpq") )
        fileName.append(".cpq");

    const auto tabs = exportDialog.selectedTabs();
    const bool exportConfiguration = exportDialog.isConfigurationEnabled();
    const bool exportCommands = exportDialog.isCommandsEnabled();

    if ( !exportDataFrom(fileName, tabs, exportConfiguration, exportCommands) ) {
        QMessageBox::critical(
                    this, tr("CopyQ Export Error"),
                    tr("Failed to export file %1!")
                    .arg(quoteString(fileName)) );
        return false;
    }

    return true;
}

void MainWindow::saveTabs()
{
    for( int i = 0; i < ui->tabWidget->count(); ++i ) {
        auto c = getPlaceholder(i)->browser();
        if (c)
            c->saveUnsavedItems();
    }
    ui->tabWidget->saveTabInfo();
}

bool MainWindow::loadTab(const QString &fileName)
{
    QFile file(fileName);
    if ( !file.open(QIODevice::ReadOnly) )
        return false;

    QDataStream in(&file);
    in.setVersion(QDataStream::Qt_4_7);

    QByteArray header;
    QString tabName;
    in >> header >> tabName;
    if ( !(header.startsWith("CopyQ v1") || header.startsWith("CopyQ v2")) || tabName.isEmpty() ) {
        file.close();
        return false;
    }

    // Find unique tab name.
    renameToUnique(&tabName, ui->tabWidget->tabs());

    auto c = createTab(tabName, MatchExactTabName)->createBrowser();
    if (!c)
        return false;

    deserializeData(c->model(), &in, m_sharedData->maxItems);

    c->loadItems();
    c->saveItems();

    file.close();

    ui->tabWidget->setCurrentIndex( ui->tabWidget->count() - 1 );

    return true;
}

bool MainWindow::importDataFrom(const QString &fileName, ImportOptions options)
{
    // Compatibility with v2.9.0 and earlier.
    if ( loadTab(fileName) )
        return true;

    QFile file(fileName);
    if ( !file.open(QIODevice::ReadOnly) )
        return false;

    QDataStream in(&file);
    in.setVersion(QDataStream::Qt_4_7);

    return importDataV3(&in, options);
}

bool MainWindow::exportAllData(const QString &fileName)
{
    const auto tabs = ui->tabWidget->tabs();
    const bool exportConfiguration = true;
    const bool exportCommands = true;

    return exportDataFrom(fileName, tabs, exportConfiguration, exportCommands);
}

bool MainWindow::importData()
{
    const auto fileName = QFileDialog::getOpenFileName(
                this, QString(), QString(), importExportFileDialogFilter() );
    if ( fileName.isNull() )
        return false;

    if ( !importDataFrom(fileName, ImportOptions::Select) ) {
        QMessageBox::critical(
                    this, tr("CopyQ Import Error"),
                    tr("Failed to import file %1!")
                    .arg(quoteString(fileName)) );
        return false;
    }

    return true;
}

void MainWindow::sortSelectedItems()
{
    auto c = browser();
    if (c)
        c->sortItems( c->selectionModel()->selectedRows() );
}

void MainWindow::reverseSelectedItems()
{
    auto c = browser();
    if (c)
        c->reverseItems( c->selectionModel()->selectedRows() );
}

Action *MainWindow::action(const QVariantMap &data, const Command &cmd, const QModelIndex &outputIndex)
{
    if (cmd.wait) {
        auto actionDialog = openActionDialog(data);
        if ( !cmd.outputTab.isEmpty() )
            actionDialog->setCurrentTab(cmd.outputTab);
        actionDialog->setCommand(cmd);
    } else if ( cmd.cmd.isEmpty() ) {
        m_actionHandler->addFinishedAction(cmd.name);
    } else {
        auto act = new Action();
        act->setCommand( cmd.cmd, QStringList(getTextData(data)) );
        act->setInputWithFormat(data, cmd.input);
        act->setName(cmd.name);
        act->setData(data);

        if ( !cmd.output.isEmpty() ) {
            if ( outputIndex.isValid() )
                actionOutput(this, act, cmd.output, outputIndex);
            else if ( !cmd.sep.isEmpty() )
                actionOutput(this, act, cmd.output, cmd.outputTab, QRegExp(cmd.sep));
            else
                actionOutput(this, act, cmd.output, cmd.outputTab);
        }

        m_actionHandler->action(act);
        return act;
    }

    return nullptr;
}

void MainWindow::runInternalAction(Action *action)
{
    m_actionHandler->internalAction(action);
}

bool MainWindow::isInternalActionId(int id) const
{
    return m_actionHandler->isInternalActionId(id);
}

void MainWindow::openNewTabDialog(const QString &name)
{
    auto d = new TabDialog(TabDialog::TabNew, this);
    d->setAttribute(Qt::WA_DeleteOnClose, true);
    d->setTabs(ui->tabWidget->tabs());

    d->setTabName(name);

    connect( d, &TabDialog::newTabNameAccepted,
             this, &MainWindow::addTab );

    d->open();
}

void MainWindow::openNewTabDialog()
{
    QString tabPath = ui->tabWidget->getCurrentTabPath();
    if ( ui->tabWidget->isTabGroup(tabPath) )
        tabPath.append('/');

    openNewTabDialog(tabPath);
}

void MainWindow::openRenameTabGroupDialog(const QString &name)
{
    auto d = new TabDialog(TabDialog::TabGroupRename, this);
    d->setAttribute(Qt::WA_DeleteOnClose, true);
    d->setTabs(ui->tabWidget->tabs());
    d->setTabGroupName(name);

    connect( d, &TabDialog::treeTabNameAccepted,
             this, &MainWindow::renameTabGroup );

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

void MainWindow::openRenameTabDialog(int tabIndex)
{
    auto d = new TabDialog(TabDialog::TabRename, this);
    d->setAttribute(Qt::WA_DeleteOnClose, true);
    d->setTabIndex(tabIndex);
    d->setTabs(ui->tabWidget->tabs());
    d->setTabName( getPlaceholder(tabIndex)->tabName() );

    connect( d, &TabDialog::barTabNameAccepted,
             this, &MainWindow::renameTab );

    d->open();
}

void MainWindow::openRenameTabDialog()
{
    const int tabIndex = ui->tabWidget->currentIndex();
    if (tabIndex >= 0)
        openRenameTabDialog(tabIndex);
}

void MainWindow::renameTab(const QString &name, int tabIndex)
{
    if ( name.isEmpty() || ui->tabWidget->tabs().contains(name) )
        return;

    auto placeholder = getPlaceholder(tabIndex);
    if (placeholder) {
        updateTabIcon(name, placeholder->tabName());
        placeholder->setTabName(name);
        ui->tabWidget->setTabName(tabIndex, name);
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

                auto placeholder = getPlaceholder(i);
                placeholder->removeItems();
                placeholder->deleteLater();

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

    auto placeholder = getPlaceholder(tabIndex);

    if ( placeholder != nullptr && w->count() > 1 ) {
        int answer = QMessageBox::Yes;
        if (ask) {
            answer = QMessageBox::question(
                        this,
                        tr("Remove Tab?"),
                        tr("Do you want to remove tab <strong>%1</strong>?"
                           ).arg( w->tabName(tabIndex).remove('&') ),
                        QMessageBox::Yes | QMessageBox::No,
                        QMessageBox::Yes);
        }
        if (answer == QMessageBox::Yes) {
            placeholder->removeItems();
            placeholder->deleteLater();
            w->removeTab(tabIndex);
            saveTabPositions();
        }
    }
}

void MainWindow::setTabIcon()
{
    if ( ui->tabWidget->isTabGroupSelected() )
        setTabIcon( ui->tabWidget->getCurrentTabPath() );
    else
        setTabIcon( getPlaceholder()->tabName() );
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
    disconnect();
    delete ui;
}
