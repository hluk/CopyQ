/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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
#include "common/tabs.h"
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
const int itemPreviewUpdateIntervalMsec = 100;

const QIcon iconClipboard() { return getIcon("clipboard", IconPaste); }
const QIcon iconTabIcon() { return getIconFromResources("tab_icon"); }
const QIcon iconTabNew() { return getIconFromResources("tab_new"); }
const QIcon iconTabRemove() { return getIconFromResources("tab_remove"); }
const QIcon iconTabRename() { return getIconFromResources("tab_rename"); }

const char propertyWidgetSizeGuarded[] = "CopyQ_widget_size_guarded";

const char propertyActionFilterCommandFailed[] = "CopyQ_action_filter_command_failed";

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

bool matchData(const QRegularExpression &re, const QVariantMap &data, const QString &format)
{
    if ( re.pattern().isEmpty() )
        return true;

    const QString text = getTextData(data, format);
    return text.contains(re);
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
    PlatformWindowPtr platformWindow = platformNativeInterface()->getWindow(wid);
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
        std::sort(selected.begin(), selected.end());
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

// WORKAROUND: setWindowFlags() hides the window.
// See: https://doc.qt.io/qt-5/qwidget.html#windowFlags-prop
bool setWindowFlag(QWidget *window, Qt::WindowType flag, bool enable)
{
    if (!window)
        return false;

    const Qt::WindowFlags flags = window->windowFlags();
    const bool wasEnabled = flags.testFlag(flag);
    if (wasEnabled == enable)
        return false;

    const bool wasVisible = window->isVisible();
    const bool wasActive = window->isActiveWindow();
    window->setWindowFlags(flags ^ flag);
    if (wasVisible) {
        if (wasActive) {
            window->show();
        } else {
            const bool showWithoutActivating = window->testAttribute(Qt::WA_ShowWithoutActivating);
            window->setAttribute(Qt::WA_ShowWithoutActivating);
            window->show();
            window->setAttribute(Qt::WA_ShowWithoutActivating, showWithoutActivating);
        }

        if (wasActive) {
            window->raise();
            window->activateWindow();
            QApplication::setActiveWindow(window);
            stealFocus(*window);
        }
    }

    return true;
}

void setAlwaysOnTop(QWidget *window, bool alwaysOnTop)
{
    if ( setWindowFlag(window, Qt::WindowStaysOnTopHint, alwaysOnTop) ) {
        // Workaround for QTBUG-28601.
        window->setAcceptDrops(true);
    }
}

void setHideInTaskBar(QWidget *window, bool hideInTaskBar)
{
    setWindowFlag(window, Qt::Tool, hideInTaskBar);
}

template<typename Dialog, typename ...Ts>
Dialog *openDialog(Ts... arguments)
{
    std::unique_ptr<Dialog> dialog( new Dialog(arguments...) );
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

    const auto platform = platformNativeInterface();
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

bool hasDialogOpen(QWidget *parent)
{
    for ( auto window : qApp->topLevelWidgets() ) {
        if ( window->isVisible() && window->parentWidget() == parent )
            return true;
    }

    return false;
}

void deleteSubMenus(QObject *parent)
{
    for (auto subMenu : parent->findChildren<QMenu*>()) {
        if (subMenu->parent() == parent)
            delete subMenu;
    }
}

void clearActions(QMenu *menu)
{
    for (QAction *action : menu->actions()) {
        action->setVisible(false);
        action->setDisabled(true);
        action->setShortcuts({});
        action->deleteLater();
        menu->removeAction(action);
    }

    deleteSubMenus(menu);
    menu->clear();
}

void clearActions(QToolBar *toolBar)
{
    for (QAction *action : toolBar->actions()) {
        // Omit removing action from other menus.
        if (action->parent() == toolBar)
            delete action;
    }

    deleteSubMenus(toolBar);
    toolBar->clear();
}

bool hasCommandFuzzy(const QVector<Command> &commands, const Command &command)
{
    return std::any_of(std::begin(commands), std::end(commands), [&command](const Command &cmd){
        return command.name == cmd.name || command.cmd == cmd.cmd;
    });
}

} // namespace

class ToolBar final : public QToolBar {
public:
    explicit ToolBar(QWidget *parent)
        : QToolBar(parent)
    {
        setObjectName("toolBar");
        setContextMenuPolicy(Qt::PreventContextMenu);
        initSingleShotTimer( &m_timerUnfreeze, 50, this, &ToolBar::unfreeze );
    }

    void setFrozen(bool frozen) {
        if (frozen) {
            m_frozen = true;
            setUpdatesEnabled(false);
            setEnabled(false);
            m_timerUnfreeze.stop();
        } else {
            m_timerUnfreeze.start();
        }
    }

protected:
    void paintEvent(QPaintEvent *ev) override
    {
        if (m_frozen)
            return;

        QToolBar::paintEvent(ev);
    }

private:
    void unfreeze()
    {
        m_frozen = false;
        setEnabled(true);
        setUpdatesEnabled(true);
        update();
    }

    bool m_frozen = false;
    QTimer m_timerUnfreeze;
};

MainWindow::MainWindow(const ClipboardBrowserSharedPtr &sharedData, QWidget *parent)
    : QMainWindow(parent)
    , cm(nullptr)
    , ui(new Ui::MainWindow)
    , m_menuItem(nullptr)
    , m_trayMenu( new TrayMenu(this) )
    , m_tray(nullptr)
    , m_toolBar(new ToolBar(this))
    , m_actionToggleClipboardStoring()
    , m_sharedData(sharedData)
    , m_lastWindow()
    , m_menu( new TrayMenu(this) )
    , m_menuMaxItemCount(-1)
    , m_commandDialog(nullptr)
    , m_menuItems(menuItems())
    , m_clipboard(platformNativeInterface()->clipboard())
{
    ui->setupUi(this);

#ifdef Q_OS_MAC
    // Open above fullscreen windows on OS X.
    setWindowModality(Qt::WindowModal);
    setWindowFlag(Qt::Sheet);
#endif

    menuBar()->setObjectName("menu_bar");
    createMenu();

    ui->tabWidget->addToolBars(this);
    addToolBar(Qt::RightToolBarArea, m_toolBar);

    ui->dockWidgetItemPreview->setFocusProxy(ui->scrollAreaItemPreview);
    ui->dockWidgetItemPreview->hide();

    WindowGeometryGuard::create(this);
    restoreState( mainWindowState(objectName()) );
    // NOTE: QWidget::isVisible() returns false if parent is not visible.
    m_showItemPreview = !ui->dockWidgetItemPreview->isHidden();

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
    connect( qApp, &QCoreApplication::aboutToQuit,
             this, &MainWindow::onAboutToQuit );

    connect(m_sharedData->itemFactory, &ItemFactory::error,
            this, &MainWindow::showError);
    connect(m_sharedData->itemFactory, &ItemFactory::addCommands,
            this, &MainWindow::addCommands);

    initSingleShotTimer( &m_timerUpdateFocusWindows, 100, this, &MainWindow::updateFocusWindows );
    initSingleShotTimer( &m_timerUpdateContextMenu, 0, this, &MainWindow::updateContextMenuTimeout );
    initSingleShotTimer( &m_timerUpdateTrayMenuItems, trayMenuUpdateIntervalMsec, this, &MainWindow::updateTrayMenuItemsTimeout );
    initSingleShotTimer( &m_timerUpdatePreview, 0, this, &MainWindow::updateItemPreviewTimeout );
    initSingleShotTimer( &m_timerTrayAvailable, 1000, this, [this]() { setTrayEnabled(); } );
    initSingleShotTimer( &m_timerSaveTabPositions, 1000, this, &MainWindow::doSaveTabPositions );
    initSingleShotTimer( &m_timerRaiseLastWindowAfterMenuClosed, 50, this, &MainWindow::raiseLastWindowAfterMenuClosed);
    enableHideWindowOnUnfocus();

    m_trayMenu->setObjectName("TrayMenu");
    m_menu->setObjectName("Menu");

    auto act = m_trayMenu->addAction( appIcon(), tr("&Show/Hide") );
    connect(act, &QAction::triggered, this, &MainWindow::toggleVisible);
    m_trayMenu->setDefaultAction(act);
    addTrayAction(Actions::File_Preferences);
    addTrayAction(Actions::File_ToggleClipboardStoring);
    m_trayMenu->addSeparator();
    addTrayAction(Actions::File_Exit);
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
    event->ignore();
    hideWindow();
    COPYQ_LOG("Got main window close event.");
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

    // Some action should not be triggered from tab widget or preview dock.
    QWidget *actionParent = ui->tabWidget;

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

    // - show preview
    QAction *togglePreviewAction =
            createAction( Actions::File_ShowPreview, &MainWindow::toggleItemPreviewVisible, menu );
    togglePreviewAction->setCheckable(true);
    togglePreviewAction->setChecked(m_showItemPreview);

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
    createAction( Actions::Edit_FindItems, &MainWindow::findNextOrPrevious, menu, actionParent );

    // - separator
    menu->addSeparator();

    // - sort
    createAction( Actions::Edit_SortSelectedItems, &MainWindow::sortSelectedItems, menu, actionParent );

    // - reverse order
    createAction( Actions::Edit_ReverseSelectedItems, &MainWindow::reverseSelectedItems, menu, actionParent );

    // - separator
    menu->addSeparator();

    // - paste items
    createAction( Actions::Edit_PasteItems, &MainWindow::pasteItems, menu, actionParent );

    // - copy items
    createAction( Actions::Edit_CopySelectedItems, &MainWindow::copyItems, menu, actionParent );

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
    interruptMenuCommandFilters(&m_itemMenuMatchCommands);

    // Omit tool bar flickering.
    m_toolBar->setFrozen(true);

    clearActions(m_menuItem);

    m_timerUpdateContextMenu.start(intervalMsec);
}

void MainWindow::updateTrayMenuItems()
{
    if ( !m_timerUpdateTrayMenuItems.isActive() )
        m_timerUpdateTrayMenuItems.start();
}

void MainWindow::updateTrayMenuCommands()
{
    m_trayMenu->clearCustomActions();

    if (!m_options.trayCommands)
        return;

    const QString format = tr("&Clipboard: %1", "Tray menu clipboard item format");
    const auto font = m_trayMenu->font();
    const auto clipboardLabel = textLabelForData(m_clipboardData, font, format, true);

    QAction *clipboardAction = m_trayMenu->addAction( iconClipboard(), clipboardLabel );
    connect(clipboardAction, &QAction::triggered,
            this, &MainWindow::showClipboardContent);
    m_trayMenu->addCustomAction(clipboardAction);

    m_trayMenu->markItemInClipboard(m_clipboardData);

    int i = m_trayMenu->actions().size();
    addCommandsToTrayMenu(m_clipboardData);
    QList<QAction *> actions = m_trayMenu->actions();
    for ( ; i < actions.size(); ++i )
        m_trayMenu->addCustomAction(actions[i]);
}

void MainWindow::updateIcon()
{
    const QIcon icon = appIcon();
    setWindowIcon(icon);
    if (m_tray)
        m_tray->setIcon(icon);
}

void MainWindow::updateContextMenuTimeout()
{
    auto c = browserOrNull();
    if ( ui->tabWidget->isTabGroupSelected() || !c || c->isInternalEditorOpen()) {
        clearActions(m_toolBar);
        m_toolBar->setFrozen(false);
        return;
    }

    addCommandsToItemMenu(c);

    m_menuItem->addSeparator();

    addItemAction( Actions::Item_MoveToClipboard, c, &ClipboardBrowser::moveToClipboard );
    addItemAction( Actions::Item_ShowContent, this, &MainWindow::showItemContent );
    addItemAction( Actions::Item_Remove, c, &ClipboardBrowser::remove );
    addItemAction( Actions::Item_Edit, c, &ClipboardBrowser::editSelected );
    addItemAction( Actions::Item_EditNotes, c, &ClipboardBrowser::editNotes );
    addItemAction( Actions::Item_EditWithEditor, c, &ClipboardBrowser::openEditor );
    addItemAction( Actions::Item_Action, this, &MainWindow::openActionDialog );

    m_menuItem->addSeparator();

    addItemAction( Actions::Item_MoveUp, this, &MainWindow::moveUp );
    addItemAction( Actions::Item_MoveDown, this, &MainWindow::moveDown );
    addItemAction( Actions::Item_MoveToTop, this, &MainWindow::moveToTop );
    addItemAction( Actions::Item_MoveToBottom, this, &MainWindow::moveToBottom );

    // Disable the show-preview option when the preview dock is closed.
    connect( ui->dockWidgetItemPreview, &QDockWidget::visibilityChanged,
             this, [this]() {
                if ( ui->dockWidgetItemPreview->isHidden() )
                    setItemPreviewVisible(false);
             }, Qt::UniqueConnection );

    updateToolBar();
    updateActionShortcuts();
}

void MainWindow::updateItemPreviewAfterMs(int ms)
{
    m_timerUpdatePreview.start(ms);
}

void MainWindow::updateItemPreviewTimeout()
{
    const bool showPreview = m_showItemPreview;

    auto c = browserOrNull();
    if (c && c->length() > 0) {
        ui->dockWidgetItemPreview->setVisible(m_showItemPreview && !c->isInternalEditorOpen());

        QWidget *w = ui->dockWidgetItemPreview->isVisible() && !ui->tabWidget->isTabGroupSelected()
                ? c->currentItemPreview(ui->scrollAreaItemPreview)
                : nullptr;

        ui->scrollAreaItemPreview->setVisible(w != nullptr);
        ui->scrollAreaItemPreview->setWidget(w);
        if (w) {
            ui->dockWidgetItemPreview->setStyleSheet( c->styleSheet() );
            w->show();
        }
    } else {
        ui->dockWidgetItemPreview->hide();
    }

    m_showItemPreview = showPreview;
    m_timerUpdatePreview.stop();
}

void MainWindow::toggleItemPreviewVisible()
{
    setItemPreviewVisible(!m_showItemPreview);
}

void MainWindow::setItemPreviewVisible(bool visible)
{
    if (m_showItemPreview == visible)
        return;

    m_showItemPreview = visible;
    updateItemPreviewAfterMs(0);
}

bool MainWindow::isItemPreviewVisible() const
{
    return m_showItemPreview;
}

void MainWindow::onAboutToQuit()
{
    if (cm)
        cm->disconnect();

    saveMainWindowState( objectName(), saveState() );
    hideWindow();
    if (m_tray)
        m_tray->hide();

    stopMenuCommandFilters(&m_itemMenuMatchCommands);
    stopMenuCommandFilters(&m_trayMenuMatchCommands);
    terminateAction(&m_displayActionId);
}

void MainWindow::onSaveCommand(const Command &command)
{
    auto commands = loadAllCommands();
    commands.append(command);
    setCommands(commands);
}

void MainWindow::onItemCommandActionTriggered(CommandAction *commandAction, const QString &triggeredShortcut)
{
    COPYQ_LOG( QString("Trigger: %1").arg(commandAction->text()) );
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
        browser->add(dataMap, 0);
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
    connect( browser, &ClipboardBrowser::searchShowRequest,
             this, &MainWindow::onSearchShowRequest );
    connect( browser, &ClipboardBrowser::itemWidgetCreated,
             this, &MainWindow::onItemWidgetCreated );

    if (browserOrNull() == browser) {
        const int index = ui->tabWidget->currentIndex();
        tabChanged(index, index);
    }
}

void MainWindow::onBrowserDestroyed(ClipboardBrowserPlaceholder *placeholder)
{
    if (placeholder == getPlaceholder()) {
        updateContextMenu(0);
        updateItemPreviewAfterMs(0);
    }
}

void MainWindow::onItemSelectionChanged(const ClipboardBrowser *browser)
{
    if (browser == browserOrNull()) {
        updateContextMenu(0);
        updateItemPreviewAfterMs(0);
    }
}

void MainWindow::onItemsChanged(const ClipboardBrowser *browser)
{
    if (browser == browserOrNull()) {
        updateContextMenu(contextMenuUpdateIntervalMsec);
        updateItemPreviewAfterMs(itemPreviewUpdateIntervalMsec);
    }

    const ClipboardBrowserPlaceholder *placeholder = getPlaceholderForTrayMenu();
    if (placeholder && placeholder->browser() == browser)
        updateTrayMenuItems();
}

void MainWindow::onInternalEditorStateChanged(const ClipboardBrowser *browser)
{
    if (browser == browserOrNull()) {
        updateContextMenu(0);
        updateItemPreviewAfterMs(0);
    }
}

void MainWindow::onItemWidgetCreated(const PersistentDisplayItem &item)
{
    if ( m_displayCommands.isEmpty() )
        return;

    m_displayItemList.append(item);
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
            actionOutput(this, act, command.output, command.outputTab, QRegularExpression(command.sep));
        else
            actionOutput(this, act, command.output, command.outputTab);
    }

    m_sharedData->actions->action(act);
}

void MainWindow::onSearchShowRequest(const QString &text)
{
    enterSearchMode();
    ui->searchBar->setText(text);
    ui->searchBar->end(false);
}

void MainWindow::runDisplayCommands()
{
    if ( m_displayItemList.isEmpty() )
        return;

    if ( !isInternalActionId(m_displayActionId) ) {
        m_currentDisplayItem = m_displayItemList.takeFirst();
        const auto action = runScript("runDisplayCommands()", m_currentDisplayItem.data());
        m_displayActionId = action->id();
    }

    emit sendActionData(m_displayActionId, QByteArray());
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
    updateTrayMenuCommands();
}

void MainWindow::setFilter(const QString &text)
{
    if ( text.isEmpty() ) {
        enterBrowseMode();
    } else {
        enterSearchMode(text);
        getPlaceholder()->setFocus();
    }
}

QString MainWindow::filter() const
{
    return ui->searchBar->isVisible() ? ui->searchBar->text() : QString();
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

ClipboardBrowserPlaceholder *MainWindow::getPlaceholder(const QString &tabName) const
{
    for (auto placeholder : findChildren<ClipboardBrowserPlaceholder*>()) {
        if ( placeholder->tabName() == tabName )
            return placeholder;
    }
    return nullptr;
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

ClipboardBrowserPlaceholder *MainWindow::createTab(
        const QString &name, TabNameMatching nameMatch)
{
    if ( name.isEmpty() )
        return nullptr;

    const int i = nameMatch == MatchExactTabName
            ? findTabIndexExactMatch(name)
            : findTabIndex(name);

    if (i != -1)
        return getPlaceholder(i);

    auto placeholder = new ClipboardBrowserPlaceholder(name, m_sharedData, this);
    connect( placeholder, &ClipboardBrowserPlaceholder::browserCreated,
             this, &MainWindow::onBrowserCreated );
    connect( placeholder, &ClipboardBrowserPlaceholder::browserDestroyed,
             this, [this, placeholder]() { onBrowserDestroyed(placeholder); } );

    ui->tabWidget->addTab(placeholder, name);
    saveTabPositions();
    return placeholder;
}

template <typename SlotReturnType>
QAction *MainWindow::createAction(int id, MainWindowActionSlot<SlotReturnType> slot, QMenu *menu, QWidget *parent)
{
    QAction *act = parent
        ? actionForMenuItem(id, parent, Qt::WidgetWithChildrenShortcut)
        : actionForMenuItem(id, this, Qt::WindowShortcut);
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

QVector<Command> MainWindow::commandsForMenu(const QVariantMap &data, const QString &tabName, const QVector<Command> &allCommands)
{
    QVector<Command> commands;
    for (const auto &command : allCommands) {
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
    if ( m_menuCommands.isEmpty() ) {
        interruptMenuCommandFilters(&m_itemMenuMatchCommands);
        return;
    }

    auto data = addSelectionData(*c);
    const auto commands = commandsForMenu(data, c->tabName(), m_menuCommands);

    for (const auto &command : commands) {
        QString name = command.name;
        QMenu *currentMenu = createSubMenus(&name, m_menuItem);
        auto act = new CommandAction(command, name, currentMenu);
        c->addAction(act);

        addMenuMatchCommand(&m_itemMenuMatchCommands, command.matchCmd, act);

        connect(act, &CommandAction::triggerCommand,
                this, &MainWindow::onItemCommandActionTriggered);
    }

    runMenuCommandFilters(&m_itemMenuMatchCommands, data);
}

void MainWindow::addCommandsToTrayMenu(const QVariantMap &clipboardData)
{
    if ( m_trayMenuCommands.isEmpty() ) {
        interruptMenuCommandFilters(&m_trayMenuMatchCommands);
        return;
    }

    ClipboardBrowserPlaceholder *placeholder = getPlaceholderForTrayMenu();
    if (!placeholder)
        return;

    // Pass current window title to commands in tray menu.
    auto data = clipboardData;
    if (m_lastWindow)
        data.insert( mimeWindowTitle, m_lastWindow->getTitle() );

    const auto commands = commandsForMenu(data, placeholder->tabName(), m_trayMenuCommands);

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

void MainWindow::runMenuCommandFilters(MenuMatchCommands *menuMatchCommands, QVariantMap &data)
{
    if ( menuMatchCommands->actions.isEmpty() ) {
        interruptMenuCommandFilters(menuMatchCommands);
        return;
    }

    data[COPYQ_MIME_PREFIX "match-commands"] = menuMatchCommands->matchCommands;

    const bool isRunning = isInternalActionId(menuMatchCommands->actionId);
    if (isRunning) {
        m_sharedData->actions->setActionData(menuMatchCommands->actionId, data);
    } else {
        const auto act = runScript("runMenuCommandFilters()", data);
        menuMatchCommands->actionId = act->id();
    }

    const int currentRun = ++menuMatchCommands->currentRun;
    emit sendActionData(menuMatchCommands->actionId, QByteArray::number(currentRun));
}

void MainWindow::interruptMenuCommandFilters(MainWindow::MenuMatchCommands *menuMatchCommands)
{
    ++menuMatchCommands->currentRun;
    menuMatchCommands->matchCommands.clear();
    menuMatchCommands->actions.clear();

    const bool isRunning = isInternalActionId(menuMatchCommands->actionId);
    if (isRunning)
        emit sendActionData(menuMatchCommands->actionId, QByteArray());
}

void MainWindow::stopMenuCommandFilters(MainWindow::MenuMatchCommands *menuMatchCommands)
{
    ++menuMatchCommands->currentRun;
    menuMatchCommands->matchCommands.clear();
    menuMatchCommands->actions.clear();
    terminateAction(&menuMatchCommands->actionId);
}

void MainWindow::terminateAction(int *actionId)
{
    if (*actionId == -1)
        return;

    const int id = *actionId;
    *actionId = -1;
    emit sendActionData(id, "ABORT");
}

bool MainWindow::isItemMenuDefaultActionValid() const
{
    const auto defaultAction = m_menuItem->defaultAction();
    return defaultAction != nullptr && defaultAction->isEnabled();
}

void MainWindow::updateToolBar()
{
    clearActions(m_toolBar);

    if ( m_toolBar->isHidden() )
        return;

    QAction *act = actionForMenuItem(Actions::File_New, this, Qt::WindowShortcut);
    m_toolBar->addAction(act);

    for ( auto action : m_menuItem->actions() ) {
        if ( action->isSeparator() ) {
            m_toolBar->addSeparator();
        } else if ( !action->icon().isNull() ) {
            act = m_toolBar->addAction(QString());

            const auto update = [=]() {
                const QIcon icon = action->icon();
                act->setIcon(icon);

                const QString text = action->text().remove("&");
                const QString shortcut = action->shortcut().toString(QKeySequence::NativeText);
                const QString label = text + (shortcut.isEmpty() ? QString() : "\n[" + shortcut + "]");
                act->setText(label);

                const QString tooltip = "<center>" + escapeHtml(text)
                        + (shortcut.isEmpty() ? QString() : "<br /><b>" + escapeHtml(shortcut) + "</b>") + "</center>";
                act->setToolTip(tooltip);
                act->setEnabled(action->isEnabled());

                if ( action->isCheckable() ) {
                    act->setCheckable(true);
                    act->setChecked(action->isChecked());
                }
            };

            connect(act, &QAction::triggered, action, &QAction::triggered);
            connect(action, &QAction::changed, act, update);
            update();
        }
    }

    m_toolBar->setFrozen(false);
}

void MainWindow::setTrayEnabled(bool enable)
{
    m_timerTrayAvailable.stop();

    const bool trayAlreadyEnabled = m_tray != nullptr;
    if (enable == trayAlreadyEnabled)
        return;

    if (enable) {
        if ( QSystemTrayIcon::isSystemTrayAvailable() ) {
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
    } else {
        // Hide tray on Ubuntu (buggy sni-qt) before disabling.
        m_tray->hide();

        delete m_tray;
        m_tray = nullptr;

        if ( isHidden() && !isMinimized() )
            minimizeWindow();
    }
}

bool MainWindow::isWindowVisible() const
{
    return !isMinimized() && isVisible() && m_isActiveWindow;
}

void MainWindow::onEscape()
{
    if ( browseMode() ) {
        auto c = browser();
        if (c && !c->hasFocus()) {
            enterBrowseMode();
            return;
        }

        hideWindow();
        if (c)
            c->setCurrent(0);
    } else {
        enterBrowseMode();
    }
}

void MainWindow::updateActionShortcuts()
{
    QList<QKeySequence> usedShortcuts;

    for (auto act : m_menuItem->findChildren<CommandAction*>()) {
        if (!act->isEnabled() && !act->isVisible())
            continue;

        if ( act->property(propertyActionFilterCommandFailed).toBool() )
            continue;

        const Command &command = act->command();
        QList<QKeySequence> uniqueShortcuts;

        for (const auto &shortcutText : command.shortcuts) {
            const QKeySequence shortcut(shortcutText, QKeySequence::PortableText);
            if ( !shortcut.isEmpty() && !usedShortcuts.contains(shortcut) ) {
                usedShortcuts.append(shortcut);
                uniqueShortcuts.append(shortcut);

                if ( !isItemMenuDefaultActionValid() && isItemActivationShortcut(shortcut) )
                    m_menuItem->setDefaultAction(act);
            }
        }

        if (!uniqueShortcuts.isEmpty())
            act->setShortcuts(uniqueShortcuts);
    }

    for (int id = 0; id < m_actions.size(); ++id) {
        QAction *action = m_actions[id];
        if (!action)
            continue;

        QList<QKeySequence> shortcuts = m_menuItems[id].shortcuts;
        for (const auto &shortcut : usedShortcuts)
            shortcuts.removeAll(shortcut);

        action->setShortcuts(shortcuts);
    }
}

QAction *MainWindow::actionForMenuItem(int id, QWidget *parent, Qt::ShortcutContext context)
{
    Q_ASSERT(id < m_menuItems.size());

    m_actions.resize(m_menuItems.size());

    QPointer<QAction> &action = m_actions[id];
    if (action && !action->isEnabled() && !action->isVisible()) {
        action->deleteLater();
        action = nullptr;
    }

    const MenuItem &item = m_menuItems[id];

    if (!action) {
        action = new QAction(item.text, parent);
        action->setShortcutContext(context);
        parent->addAction(action);
    }

    action->setIcon( getIcon(item.iconName, item.iconId) );

    return action;
}

void MainWindow::addMenuItems(TrayMenu *menu, ClipboardBrowserPlaceholder *placeholder, int maxItemCount, const QString &searchText)
{
    WidgetSizeGuard sizeGuard(menu);
    menu->clearClipboardItems();

    if (maxItemCount <= 0)
        return;

    if (!placeholder)
        return;

    const ClipboardBrowser *c = placeholder->createBrowser();
    if (!c)
        return;

    int itemCount = 0;
    for ( int i = 0; i < c->length() && itemCount < maxItemCount; ++i ) {
        const QModelIndex index = c->model()->index(i, 0);
        if ( !searchText.isEmpty() ) {
            const QString itemText = index.data(contentType::text).toString().toLower();
            if ( !itemText.contains(searchText.toLower()) )
                continue;
        }
        const QVariantMap data = index.data(contentType::data).toMap();
        menu->addClipboardItemAction(data, m_options.trayImages);
        ++itemCount;
    }
}

void MainWindow::activateMenuItem(ClipboardBrowserPlaceholder *placeholder, const QVariantMap &data, bool omitPaste)
{
    if ( m_sharedData->moveItemOnReturnKey ) {
        const auto itemHash = ::hash(data);
        if (placeholder) {
            ClipboardBrowser *c = placeholder->createBrowser();
            if (c)
                c->moveToTop(itemHash);
        }
    }

    if ( QGuiApplication::queryKeyboardModifiers().testFlag(Qt::ShiftModifier) )
        setClipboard( createDataMap(mimeText, data.value(mimeText) ) );
    else
        setClipboard(data);

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

    menu->popup( toScreen(pos, menu) );

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
        importDialog.setWindowTitle( tr("Options for Import") );
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

        updateEnabledCommands();
    }

    return in->status() == QDataStream::Ok;
}

void MainWindow::updateEnabledCommands()
{
    updateCommands(loadAllCommands(), false);
}

void MainWindow::updateCommands(QVector<Command> allCommands, bool forceSave)
{
    m_automaticCommands.clear();
    m_menuCommands.clear();
    m_scriptCommands.clear();
    m_trayMenuCommands.clear();

    QVector<Command> displayCommands;

    if ( addPluginCommands(&allCommands) || forceSave )
        saveCommands(allCommands);

    const auto disabledPluginCommands = m_sharedData->itemFactory->commands(false);
    Commands commands;
    for (const auto &command : allCommands) {
        if ( command.enable && !hasCommandFuzzy(disabledPluginCommands, command) )
            commands.append(command);
    }

    for (const auto &command : commands) {
        const auto type = command.type();

        if (type & CommandType::Automatic)
            m_automaticCommands.append(command);

        if (type & CommandType::Display)
            displayCommands.append(command);

        if (type & CommandType::Menu)
            m_menuCommands.append(command);

        if (m_options.trayCommands && type & CommandType::GlobalShortcut)
            m_trayMenuCommands.append(command);

        if (type & CommandType::Script)
            m_scriptCommands.append(command);
    }

    if (m_displayCommands != displayCommands) {
        m_displayItemList.clear();
        m_displayCommands = displayCommands;
        reloadBrowsers();
    }

    updateContextMenu(contextMenuUpdateIntervalMsec);
    updateTrayMenuCommands();
    emit commandsSaved(commands);
}

bool MainWindow::addPluginCommands(QVector<Command> *allCommands)
{
    const auto oldSize = allCommands->size();

    for ( const auto &command : m_sharedData->itemFactory->commands() ) {
        if ( !hasCommandFuzzy(*allCommands, command) )
            allCommands->append(command);
    }

    return oldSize != allCommands->size();
}

void MainWindow::disableHideWindowOnUnfocus()
{
    m_timerHideWindowIfNotActive.disconnect();
}

void MainWindow::enableHideWindowOnUnfocus()
{
    initSingleShotTimer( &m_timerHideWindowIfNotActive, 100, this, &MainWindow::hideWindowIfNotActive );
}

void MainWindow::hideWindowIfNotActive()
{
    if ( isVisible() && !hasDialogOpen(this) && !isAnyApplicationWindowActive() ) {
        COPYQ_LOG("Auto-hiding unfocused main window");
        hideWindow();
    }
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

bool MainWindow::maybeCloseCommandDialog()
{
    return !m_commandDialog || m_commandDialog->maybeClose(this);
}

void MainWindow::showError(const QString &msg)
{
    const auto notificationId = qHash(msg);
    auto notification = createNotification( QString::number(notificationId) );
    notification->setTitle( tr("CopyQ Error", "Notification error message title") );
    notification->setMessage(msg);
    notification->setIcon(IconTimesCircle);
}

Notification *MainWindow::createNotification(const QString &id)
{
    return m_sharedData->notifications->createNotification(id);
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

void MainWindow::loadSettings(QSettings &settings, AppConfig &appConfig)
{
    stopMenuCommandFilters(&m_itemMenuMatchCommands);
    stopMenuCommandFilters(&m_trayMenuMatchCommands);
    terminateAction(&m_displayActionId);

    theme().decorateMainWindow(this);
    ui->scrollAreaItemPreview->setObjectName("ClipboardBrowser");
    theme().decorateItemPreview(ui->scrollAreaItemPreview);

    setUseSystemIcons( theme().useSystemIcons() );

    m_options.confirmExit = appConfig.option<Config::confirm_exit>();

    // always on top window hint
    bool alwaysOnTop = appConfig.option<Config::always_on_top>();
    setAlwaysOnTop(this, alwaysOnTop);
    setAlwaysOnTop(m_commandDialog.data(), alwaysOnTop);

    // Vi mode
    m_options.viMode = appConfig.option<Config::vi>();
    m_trayMenu->setViModeEnabled(m_options.viMode);
    m_menu->setViModeEnabled(m_options.viMode);

    // Number search
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
    clearActions(m_toolBar);
    m_toolBar->setHidden(hideToolbar);
    bool hideToolBarLabels = appConfig.option<Config::hide_toolbar_labels>();
    m_toolBar->setToolButtonStyle(hideToolBarLabels ? Qt::ToolButtonIconOnly
                                                      : Qt::ToolButtonTextUnderIcon);

    m_options.hideMainWindow = appConfig.option<Config::hide_main_window>();
    m_options.closeOnUnfocus = appConfig.option<Config::close_on_unfocus>();

    const bool hideInTaskBar = appConfig.option<Config::hide_main_window_in_task_bar>();
    setHideInTaskBar(this, hideInTaskBar);

    // create tabs
    const Tabs tabs;
    const QStringList tabNames = savedTabs();
    for (const auto &name : tabNames) {
        auto placeholder = createTab(name, MatchExactTabName);

        const TabProperties tab = tabs.tabProperties(name);
        placeholder->setStoreItems(tab.storeItems);

        int maxItemCount = tab.maxItemCount;
        if (maxItemCount <= 0)
            maxItemCount = m_sharedData->maxItems;
        else if (maxItemCount > Config::maxItems)
            maxItemCount = Config::maxItems;
        placeholder->setMaxItemCount(maxItemCount);
    }

    Q_ASSERT( ui->tabWidget->count() > 0 );

    // Save any tabs loaded from new tab files.
    appConfig.setOption("tabs", tabNames);

    reloadBrowsers();

    ui->tabWidget->updateTabs();

    m_timerSaveTabPositions.stop();

    updateContextMenu(contextMenuUpdateIntervalMsec);
    updateItemPreviewAfterMs(itemPreviewUpdateIntervalMsec);

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

    const auto toolTipStyleSheet = theme().getToolTipStyleSheet();
    m_trayMenu->setStyleSheet(toolTipStyleSheet);
    m_menu->setStyleSheet(toolTipStyleSheet);

    setTrayEnabled( !appConfig.option<Config::disable_tray>() );
    updateTrayMenuItems();
    updateTrayMenuCommands();

    updateIcon();

    ui->searchBar->loadSettings();

    settings.beginGroup("Shortcuts");
    loadShortcuts(&m_menuItems, settings);
    updateActionShortcuts();
    settings.endGroup();

    enterBrowseMode();

    updateEnabledCommands();
}

void MainWindow::loadTheme(const QSettings &themeSettings)
{
    m_sharedData->theme.loadTheme(themeSettings);
    if (themeSettings.status() != QSettings::NoError)
        return;

    {
        Settings settings;
        settings.beginGroup("Theme");
        m_sharedData->theme.saveTheme(settings.settingsData());
        settings.endGroup();
    }

    emit configurationChanged();
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

    // It can be unexpected to have search active or random items selected when
    // reopening main window. This resets search and selection after the window
    // is closed.
    if ( !browseMode() ) {
        enterBrowseMode();
        auto c = browser();
        if (c)
            c->setCurrent(0);
    }
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

void MainWindow::showBrowser(const ClipboardBrowser *browser)
{
    int i = 0;
    for( ; i < ui->tabWidget->count() && getPlaceholder(i)->browser() != browser; ++i ) {}
    setCurrentTab(i);
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
    activateMenuItem( getPlaceholderForMenu(), data, omitPaste );
}

void MainWindow::onTrayActionTriggered(const QVariantMap &data, bool omitPaste)
{
    activateMenuItem( getPlaceholderForTrayMenu(), data, omitPaste );
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

    if ( !m_trayMenu->isVisible() && m_timerUpdateTrayMenuItems.isActive() ) {
        m_timerUpdateTrayMenuItems.stop();
        updateTrayMenuItemsTimeout();
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
        auto c = browserOrNull();
        if (c) {
            c->filterItems( browseMode() ? QRegularExpression() : ui->searchBar->filter() );

            if ( current >= 0 ) {
                if( !c->currentIndex().isValid() && isVisible() ) {
                    c->setCurrent(0);
                }
            }

            setTabOrder(ui->searchBar, c);
        }
    }

    updateContextMenu(0);
    updateItemPreviewAfterMs(0);

    if (m_options.trayCurrentTab)
        updateTrayMenuItems();
}

void MainWindow::saveTabPositions()
{
    m_timerSaveTabPositions.start();
}

void MainWindow::doSaveTabPositions()
{
    const QStringList tabs = ui->tabWidget->tabs();
    AppConfig().setOption("tabs", tabs);
}

void MainWindow::tabsMoved(const QString &oldPrefix, const QString &newPrefix)
{
    const QStringList tabs = ui->tabWidget->tabs();
    Q_ASSERT( oldPrefix == newPrefix || !tabs.contains(oldPrefix) );
    Q_ASSERT( !tabs.contains(QString()) );

    const QString prefix = oldPrefix + '/';

    // Rename tabs if needed.
    for (int i = 0 ; i < tabs.size(); ++i) {
        auto placeholder = getPlaceholder(i);
        const QString oldTabName = placeholder->tabName();

        if ( (oldTabName == oldPrefix || oldTabName.startsWith(prefix)) && newPrefix != oldPrefix) {
            const QString newName = newPrefix + oldTabName.mid(oldPrefix.size());
            updateTabIcon(newName, placeholder->tabName());
            if ( placeholder->setTabName(newName) ) {
                auto c = placeholder->browser();
                if (c)
                    ui->tabWidget->setTabItemCount( newName, c->length() );
            }
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
    return m_sharedData->actions->actionData(id);
}

void MainWindow::setActionData(int id, const QVariantMap &data)
{
    m_sharedData->actions->setActionData(id, data);
}

void MainWindow::setCommands(const QVector<Command> &commands)
{
    if ( !maybeCloseCommandDialog() )
        return;

    updateCommands(commands, true);
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

bool MainWindow::setMenuItemEnabled(int actionId, int currentRun, int menuItemMatchCommandIndex, const QVariantMap &menuItem)
{
    if (actionId != m_trayMenuMatchCommands.actionId && actionId != m_itemMenuMatchCommands.actionId)
        return false;

    const auto &menuMatchCommands = actionId == m_trayMenuMatchCommands.actionId
            ? m_trayMenuMatchCommands
            : m_itemMenuMatchCommands;

    if (currentRun != menuMatchCommands.currentRun)
        return false;

    if (menuMatchCommands.actions.size() <= menuItemMatchCommandIndex)
        return false;

    auto action = menuMatchCommands.actions[menuItemMatchCommandIndex];
    if (!action)
        return true;

    for (auto it = menuItem.constBegin(); it != menuItem.constEnd(); ++it) {
        const auto key = it.key().toUtf8();
        if (key == "color" || key == "tag")
            continue;

        const auto value = it.value();
        if (key == "icon") {
            const QString icon = value.toString();
            const QString colorName = menuItem.value("color").toString();
            const QColor color = colorName.isEmpty() ? getDefaultIconColor(*this) : deserializeColor(colorName);
            const QString tag = menuItem.value("tag").toString();
            action->setIcon( iconFromFile(icon, tag, color) );
        } else {
            action->setProperty(key, value);
        }
    }
    const bool enabled = action->isEnabled();
    action->setProperty(propertyActionFilterCommandFailed, !enabled);

    const auto shortcuts = action->shortcuts();

    if ( !enabled && (actionId == m_trayMenuMatchCommands.actionId || !m_menuItem->isVisible()) )
        action->deleteLater();

    if ( !shortcuts.isEmpty() )
        updateActionShortcuts();

    return true;
}

QVariantMap MainWindow::setDisplayData(int actionId, const QVariantMap &data)
{
    if (m_displayActionId != actionId)
        return QVariantMap();

    m_currentDisplayItem.setData(data);

    clearHiddenDisplayData();

    if ( m_displayItemList.isEmpty() )
        return QVariantMap();

    m_currentDisplayItem = m_displayItemList.takeFirst();
    m_sharedData->actions->setActionData(actionId, m_currentDisplayItem.data());
    return m_currentDisplayItem.data();
}

void MainWindow::nextTab()
{
    ui->tabWidget->nextTab();
}

void MainWindow::previousTab()
{
    ui->tabWidget->previousTab();
}

void MainWindow::setClipboard(const QVariantMap &data)
{
    setClipboard(data, ClipboardMode::Clipboard);
#ifdef HAS_MOUSE_SELECTIONS
    setClipboard(data, ClipboardMode::Selection);
#endif
}

void MainWindow::setClipboard(const QVariantMap &data, ClipboardMode mode)
{
    m_clipboard->setData(mode, data);
}

void MainWindow::setClipboardAndSelection(const QVariantMap &data)
{
    setClipboard(data);
}

void MainWindow::moveToClipboard(ClipboardBrowser *c, int row)
{
    const auto index = c ? c->index(row) : QModelIndex();
    if ( index.isValid() )
        c->moveToClipboard(index);
    else
        setClipboard(QVariantMap());
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

    ::setSessionIconEnabled(!disable);

    updateIcon();

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

ClipboardBrowserPlaceholder *MainWindow::getPlaceholderForMenu()
{
    const auto i = findTabIndex(m_menuTabName);
    return i != -1 ? getPlaceholder(i) : nullptr;
}

ClipboardBrowserPlaceholder *MainWindow::getPlaceholderForTrayMenu()
{
    if (m_options.trayCurrentTab)
        return getPlaceholder();

    if ( m_options.trayTabName.isEmpty() )
        return m_options.clipboardTab.isEmpty() ? nullptr : getPlaceholder(m_options.clipboardTab);

    int i = findTabIndex(m_options.trayTabName);
    return i != -1 ? getPlaceholder(i) : nullptr;
}

void MainWindow::onFilterChanged(const QRegularExpression &re)
{
    if (re.pattern().isEmpty())
        enterBrowseMode();
    else if ( browseMode() )
        enterSearchMode();

    auto c = browser();
    if (c)
        c->filterItems(re);
    updateItemPreviewAfterMs(2 * itemPreviewUpdateIntervalMsec);
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

    auto platform = platformNativeInterface();
    PlatformWindowPtr lastWindow = platform->getCurrentWindow();
    if (lastWindow) {
        COPYQ_LOG( QString("Focus window is \"%1\"").arg(lastWindow->getTitle()) );
        m_lastWindow = lastWindow;
    }

    if (m_options.closeOnUnfocus)
        m_timerHideWindowIfNotActive.start();
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

    auto c = browserOrNull();
    if (c)
        c->filterItems(QRegularExpression());
}

void MainWindow::enterSearchMode()
{
    ui->searchBar->show();
    ui->searchBar->setFocus(Qt::ShortcutFocusReason);

    if ( !ui->searchBar->text().isEmpty() ) {
        auto c = browserOrNull();
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

void MainWindow::updateTrayMenuItemsTimeout()
{
    // Update tray only if not currently visible.
    if ( m_trayMenu->isVisible() ) {
        updateTrayMenuItems();
        return;
    }

    COPYQ_LOG("Updating tray menu");

    WidgetSizeGuard sizeGuard(m_trayMenu);

    interruptMenuCommandFilters(&m_trayMenuMatchCommands);
    m_trayMenu->clearClipboardItems();

    filterTrayMenuItems(QString());
}

void MainWindow::filterMenuItems(const QString &searchText)
{
    addMenuItems(m_menu, getPlaceholderForMenu(), m_menuMaxItemCount, searchText);
}

void MainWindow::filterTrayMenuItems(const QString &searchText)
{
    addMenuItems(m_trayMenu, getPlaceholderForTrayMenu(), m_options.trayItems, searchText);
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
    m_sharedData->actions->showProcessManagerDialog(this);
}

ActionDialog *MainWindow::openActionDialog(const QVariantMap &data)
{
    auto actionDialog = openDialog<ActionDialog>(this);
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

    stealFocus(*actionDialog);

    return actionDialog;
}

void MainWindow::openActionDialog()
{
    auto c = browser();
    const auto data = c ? addSelectionData(*c) : QVariantMap();
    openActionDialog(data);
}

void MainWindow::showItemContent()
{
    auto c = browser( ui->tabWidget->currentIndex() );
    if (!c)
        return;

    const QModelIndex current = c->currentIndex();
    if ( current.isValid() )
        openDialog<ClipboardDialog>(current, c->model(), this);
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

        m_commandDialog = openDialog<CommandDialog>(pluginCommands, formats, parent);
        connect(this, &QObject::destroyed, m_commandDialog.data(), &QWidget::close);
        connect(m_commandDialog.data(), &CommandDialog::commandsSaved, this, &MainWindow::updateEnabledCommands);
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

ClipboardBrowser *MainWindow::browserOrNull()
{
    const ClipboardBrowserPlaceholder *currentPlaceholder = getPlaceholder();
    return currentPlaceholder ? currentPlaceholder->browser() : nullptr;
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

void MainWindow::addAndFocusTab(const QString &name)
{
    auto placeholder = createTab(name, MatchExactTabName);
    if (!placeholder)
        return;

    int i = 0;
    for( ; i < ui->tabWidget->count() && getPlaceholder(i) != placeholder; ++i ) {}
    setCurrentTab(i);

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
    c->add( cloneData(*data), row );
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
    setClipboard(data);
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
    exportDialog.setWindowTitle( tr("Options for Export") );
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
                    this, tr("Export Error"),
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
                    this, tr("Import Error"),
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
        m_sharedData->actions->addFinishedAction(cmd.name);
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
                actionOutput(this, act, cmd.output, cmd.outputTab, QRegularExpression(cmd.sep));
            else
                actionOutput(this, act, cmd.output, cmd.outputTab);
        }

        m_sharedData->actions->action(act);
        return act;
    }

    return nullptr;
}

void MainWindow::runInternalAction(Action *action)
{
    m_sharedData->actions->internalAction(action);
}

bool MainWindow::isInternalActionId(int id) const
{
    return id != -1 && m_sharedData->actions->isInternalActionId(id);
}

void MainWindow::openNewTabDialog(const QString &name)
{
    auto d = new TabDialog(TabDialog::TabNew, this);
    d->setAttribute(Qt::WA_DeleteOnClose, true);
    d->setTabs(ui->tabWidget->tabs());

    d->setTabName(name);

    connect( d, &TabDialog::newTabNameAccepted,
             this, &MainWindow::addAndFocusTab );

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
        if ( placeholder->setTabName(name) ) {
            ui->tabWidget->setTabName(tabIndex, name);
            saveTabPositions();
        }
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

bool MainWindow::unloadTab(const QString &tabName)
{
    ClipboardBrowserPlaceholder *placeholder = getPlaceholder(tabName);
    return !placeholder || placeholder->expire();
}

void MainWindow::forceUnloadTab(const QString &tabName)
{
    ClipboardBrowserPlaceholder *placeholder = getPlaceholder(tabName);
    if (!placeholder)
        return;

    placeholder->unloadBrowser();

    placeholder->createLoadButton();
}

MainWindow::~MainWindow()
{
    delete ui;
}
