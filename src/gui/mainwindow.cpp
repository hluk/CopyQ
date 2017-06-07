/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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
#include "common/commandstore.h"
#include "common/config.h"
#include "common/contenttype.h"
#include "common/display.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/shortcuts.h"
#include "common/textdata.h"
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

#ifdef HAS_TESTS
#   include <QTest>
#endif

#include <algorithm>
#include <memory>

namespace {

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
    QObject::connect( window, SIGNAL(tabGroupSelected(bool)),
                      action, SLOT(setDisabled(bool)) );
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

#ifdef HAS_MOUSE_SELECTIONS
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
    std::unique_ptr<Dialog> dialog( new Dialog(dialogParent) );
    WindowGeometryGuard::create( dialog.get() );
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->setWindowIcon(appIcon());
    dialog->activateWindow();
    dialog->show();
    return dialog.release();
}

QString defaultTabName()
{
    const QString tab = AppConfig().option<Config::clipboard_tab>();
    return tab.isEmpty() ? defaultClipboardTabName() : tab;
}

void loadItemFactorySettings(ItemFactory *itemFactory, QSettings *settings)
{
    // load settings for each plugin
    settings->beginGroup("Plugins");
    for ( auto &loader : itemFactory->loaders() ) {
        settings->beginGroup(loader->id());

        QVariantMap s;
        for (const auto &name : settings->allKeys()) {
            s[name] = settings->value(name);
        }
        loader->loadSettings(s);
        itemFactory->setLoaderEnabled( loader, settings->value("enabled", true).toBool() );

        settings->endGroup();
    }
    settings->endGroup();

    // load plugin priority
    const QStringList pluginPriority =
            settings->value("plugin_priority", QStringList()).toStringList();
    itemFactory->setPluginPriority(pluginPriority);
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

#ifdef HAS_TESTS
/**
 * Read base64-encoded settings from "COPYQ_TEST_SETTINGS" enviroment variable if not empty.
 *
 * The settings are initially taken from "CopyQ_test_settings" property of test object returned by
 * ItemLoaderInterface::tests().
 */
void resetTestsSettings()
{
    // Reset settings on first run of each test case.
    Settings settings;
    settings.clear();

    const auto settingsData = qgetenv("COPYQ_TEST_SETTINGS");
    if ( settingsData.isEmpty() )
        return;

    QVariant testSettings;
    const auto data = QByteArray::fromBase64(settingsData);
    QDataStream input(data);
    input >> testSettings;
    const auto testSettingsMap = testSettings.toMap();

    const auto testId = qApp->property("CopyQ_test_id").toString();
    const bool pluginsTest = testId != "CORE";

    if (pluginsTest) {
        settings.beginGroup("Plugins");
        settings.beginGroup(testId);
    }

    for (const auto &key : testSettingsMap.keys())
        settings.setValue(key, testSettingsMap[key]);

    if (pluginsTest) {
        settings.endGroup();
        settings.endGroup();
    }

    settings.setValue("CopyQ_test_id", testId);
}
#endif

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
    , m_sharedData(new ClipboardBrowserShared(itemFactory))
    , m_lastWindow()
    , m_notifications(nullptr)
    , m_actionHandler(new ActionHandler(this))
    , m_menu( new TrayMenu(this) )
    , m_menuMaxItemCount(-1)
    , m_commandDialog(nullptr)
    , m_canUpdateTitleFromScript(true)
    , m_iconSnip(false)
    , m_wasMaximized(false)
    , m_showItemPreview(false)
    , m_menuItems(menuItems())
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

    updateIcon();

    updateFocusWindows();

    // signals & slots
    connect( m_trayMenu, SIGNAL(aboutToShow()),
             this, SLOT(updateTrayMenuItems()), Qt::QueuedConnection );
    connect( m_trayMenu, SIGNAL(aboutToShow()),
             this, SLOT(updateFocusWindows()) );
    connect( m_trayMenu, SIGNAL(searchRequest(QString)),
             this, SLOT(addTrayMenuItems(QString)) );
    connect( m_trayMenu, SIGNAL(clipboardItemActionTriggered(uint,bool)),
             this, SLOT(onTrayActionTriggered(uint,bool)) );

    connect( m_menu, SIGNAL(aboutToShow()),
             this, SLOT(updateFocusWindows()) );
    connect( m_menu, SIGNAL(searchRequest(QString)),
             this, SLOT(addMenuItems(QString)) );
    connect( m_menu, SIGNAL(clipboardItemActionTriggered(uint,bool)),
             this, SLOT(onMenuActionTriggered(uint,bool)) );

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
    connect( ui->searchBar, SIGNAL(returnPressed()),
             this, SLOT(findNextOrPrevious()) );
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

    connect(&m_itemMenuCommandTester, SIGNAL(requestActionStart(Action*)),
            m_actionHandler, SLOT(action(Action*)));
    connect(&m_trayMenuCommandTester, SIGNAL(requestActionStart(Action*)),
            m_actionHandler, SLOT(action(Action*)));
    connect(&m_automaticCommandTester, SIGNAL(requestActionStart(Action*)),
            m_actionHandler, SLOT(action(Action*)));

    connect(itemFactory, SIGNAL(error(QString)),
            this, SLOT(showError(QString)));
    connect(itemFactory, SIGNAL(addCommands(QList<Command>)),
            this, SLOT(addCommands(QList<Command>)));

    m_commands = loadEnabledCommands();
    loadSettings();

    ui->tabWidget->setCurrentIndex(0);

    initSingleShotTimer( &m_timerUpdateFocusWindows, 50, this, SLOT(updateFocusWindows()) );
    initSingleShotTimer( &m_timerUpdateContextMenu, 0, this, SLOT(updateContextMenuTimeout()) );
    initSingleShotTimer( &m_timerShowWindow, 250 );
    initSingleShotTimer( &m_timerTrayAvailable, 1000, this, SLOT(createTrayIfSupported()) );
    initSingleShotTimer( &m_timerTrayIconSnip, 250, this, SLOT(updateIconSnipTimeout()) );
    initSingleShotTimer( &m_timerSaveTabPositions, 1000, this, SLOT(doSaveTabPositions()) );

    // browse mode by default
    enterBrowseMode();

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
            showBrowser(i);
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

void MainWindow::showEvent(QShowEvent *event)
{
    m_timerShowWindow.start();
    QMainWindow::showEvent(event);
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
    act = createAction( Actions::File_New, SLOT(editNewItem()), menu );
    disableActionWhenTabGroupSelected(act, this);

    // - import
    createAction( Actions::File_Import, SLOT(importData()), menu );

    // - export
    act = createAction( Actions::File_Export, SLOT(exportData()), menu );
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
    createAction( Actions::Edit_FindItems, SLOT(findNextOrPrevious()), menu );

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

void MainWindow::updateContextMenu()
{
    m_itemMenuCommandTester.abort();

    for (auto action : m_menuItem->actions())
        delete action;

    for (auto menu : m_menuItem->findChildren<QMenu*>())
        delete menu;

    m_menuItem->clear();
    // Omit tool bar flickering.
    ui->toolBar->setUpdatesEnabled(false);
    ui->toolBar->setEnabled(false);

    m_timerUpdateContextMenu.start();
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
    const bool shouldSnip = hasRunningAction() || m_clipboardStoringDisabled;
    if (m_iconSnip != shouldSnip) {
        m_iconSnip = shouldSnip;
        m_timerTrayIconSnip.start(250);
        updateIcon();
    }
}

void MainWindow::updateContextMenuTimeout()
{
    updateItemPreview();

    if ( ui->tabWidget->isTabGroupSelected() )
        return;

    auto c = getPlaceholder()->createBrowser();
    if ( !c || c->isInternalEditorOpen())
        return;

    setDisabledShortcuts(QList<QKeySequence>());

    addCommandsToItemMenu(c);

    m_menuItem->addSeparator();

    addItemAction( Actions::Item_MoveToClipboard, c, SLOT(moveToClipboard()) );
    addItemAction( Actions::Item_ShowContent, c, SLOT(showItemContent()) );
    QAction *togglePreviewAction =
            addItemAction( Actions::Item_ShowPreview, this, SLOT(updateItemPreview()) );
    addItemAction( Actions::Item_Remove, c, SLOT(remove()) );
    addItemAction( Actions::Item_Edit, c, SLOT(editSelected()) );
    addItemAction( Actions::Item_EditNotes, c, SLOT(editNotes()) );
    addItemAction( Actions::Item_EditWithEditor, c, SLOT(openEditor()) );
    addItemAction( Actions::Item_Action, this, SLOT(openActionDialog()) );
    addItemAction( Actions::Item_NextFormat, this, SLOT(nextItemFormat()) );
    addItemAction( Actions::Item_PreviousFormat, this, SLOT(previousItemFormat()) );

    m_menuItem->addSeparator();

    addItemAction( Actions::Item_MoveUp, this, SLOT(moveUp()) );
    addItemAction( Actions::Item_MoveDown, this, SLOT(moveDown()) );
    addItemAction( Actions::Item_MoveToTop, this, SLOT(moveToTop()) );
    addItemAction( Actions::Item_MoveToBottom, this, SLOT(moveToBottom()) );

    togglePreviewAction->setCheckable(true);
    togglePreviewAction->setChecked(m_showItemPreview);
    connect( togglePreviewAction, SIGNAL(toggled(bool)),
             this, SLOT(setItemPreviewVisible(bool)), Qt::UniqueConnection );

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
    m_commands = loadEnabledCommands();
    updateContextMenu();
    emit commandsSaved();
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
        for (int i = selected.size() - 1; i >= 0; --i) {
            const auto data = c->copyIndex(selected[i]);
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

void MainWindow::on_tabWidget_dropItems(const QString &tabName, const QMimeData *data)
{
    auto browser = tab(tabName);

    if (browser) {
        const QVariantMap dataMap = data->hasFormat(mimeItems)
                ? cloneData(*data, QStringList() << mimeItems) : cloneData(*data);
        browser->paste(dataMap, 0);
    }
}

void MainWindow::showContextMenu(const QPoint &position)
{
    m_menuItem->exec(position);
}

void MainWindow::updateContextMenu(const ClipboardBrowser *browser)
{
    if ( browser == getPlaceholder()->browser() )
        updateContextMenu();
}

void MainWindow::automaticCommandTestFinished(const Command &command, bool passed)
{
    if ( passed && canExecuteCommand(command, m_automaticCommandTester.data(), defaultTabName()) )
        runAutomaticCommand(command);
    else
        m_automaticCommandTester.start();
}

QAction *MainWindow::enableActionForCommand(QMenu *menu, const Command &command, bool enable)
{
    CommandAction *act = nullptr;
    for (auto action : menu->findChildren<CommandAction*>()) {
        if (!action->isEnabled() && action->command().matchCmd == command.matchCmd) {
            act = action;
            break;
        }
    }

    Q_ASSERT(act);

    if (enable) {
        act->setEnabled(true);
        return act;
    }

    QMenu *parentMenu = qobject_cast<QMenu*>(act->parentWidget());
    delete act;

    // Remove empty menus.
    while ( parentMenu && parentMenu->actions().isEmpty() ) {
        QMenu *parentMenu2 = qobject_cast<QMenu*>(parentMenu->parentWidget());
        delete parentMenu;
        parentMenu = parentMenu2;
    }

    return nullptr;
}

void MainWindow::addCommandsToItemMenu(const Command &command, bool passed)
{
    auto act = enableActionForCommand(m_menuItem, command, passed);
    if (act) {
        const auto shortcuts = act->shortcuts();
        if ( !shortcuts.isEmpty() ) {
            setDisabledShortcuts(m_disabledShortcuts + shortcuts);

            if ( !isItemMenuDefaultActionValid() ) {
                for (const auto &shortcut : shortcuts) {
                    if ( isItemActivationShortcut(shortcut) ) {
                        m_menuItem->setDefaultAction(act);
                        break;
                    }
                }
            }
        }

        updateToolBar();
    }
    m_itemMenuCommandTester.start();
}

void MainWindow::addCommandsToTrayMenu(const Command &command, bool passed)
{
    enableActionForCommand(m_trayMenu, command, passed);
    m_trayMenuCommandTester.start();
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
    connect( browser, SIGNAL(changeClipboard(QVariantMap)),
             this, SLOT(setClipboard(QVariantMap)) );
    connect( browser, SIGNAL(requestShow(const ClipboardBrowser*)),
             this, SLOT(showBrowser(const ClipboardBrowser*)) );
    connect( browser, SIGNAL(error(QString)),
             this, SLOT(showError(QString)) );
    connect( browser, SIGNAL(doubleClicked(QModelIndex)),
             this, SLOT(activateCurrentItem()) );
    connect( browser, SIGNAL(itemCountChanged(QString,int)),
             ui->tabWidget, SLOT(setTabItemCount(QString,int)) );
    connect( browser, SIGNAL(showContextMenu(QPoint)),
             this, SLOT(showContextMenu(QPoint)) );
    connect( browser, SIGNAL(updateContextMenu(const ClipboardBrowser *)),
             this, SLOT(updateContextMenu(const ClipboardBrowser *)) );
    connect( browser, SIGNAL(searchRequest()),
             this, SLOT(findNextOrPrevious()) );
    connect( browser, SIGNAL(searchHideRequest()),
             this, SLOT(hideSearchBar()) );
}

void MainWindow::onNotificationButtonClicked(const NotificationButton &button)
{
    const QString mimeNotificationData = COPYQ_MIME_PREFIX "notification-data";

    QVariantMap data;
    data.insert(mimeNotificationData, button.data);

    Command cmd;
    cmd.cmd = button.script;
    cmd.input = mimeNotificationData;

    action(data, cmd);
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
    updateTrayMenuItems();
    showClipboardMessage(m_clipboardData);
}

bool MainWindow::canUpdateTitleFromScript() const
{
    return m_canUpdateTitleFromScript;
}

void MainWindow::setFilter(const QString &text)
{
    ui->searchBar->setText(text);
    enterBrowseMode();
}

void MainWindow::invoke(Callable *callable)
{
    (*callable)();
}

#ifdef HAS_TESTS
void MainWindow::keyClicks(const QString &keys, int delay)
{
    QWidget *widget;

    if ( m_trayMenu->isVisible() ) {
        widget = m_trayMenu;
    } else if ( m_menu->isVisible() ) {
        widget = m_menu;
    } else {
        // This is needed to properly focus just opened dialogs.
        auto modalWidget = QApplication::activeModalWidget();
        if (modalWidget)
            QApplication::setActiveWindow(modalWidget);

        widget = QApplication::focusWidget();
        if (!widget) {
            COPYQ_LOG("No focused widget -> using main window");
            widget = this;
        }
    }

    const auto className = widget->metaObject()->className();

    auto widgetName = QString("%1:%2")
            .arg(widget->objectName())
            .arg(className);

    if (widget != widget->window()) {
        widgetName.append(
                    QString(" in %1:%2")
                    .arg(widget->window()->objectName())
                    .arg(widget->window()->metaObject()->className()) );
    }

    // There could be some animation/transition effect on check boxes
    // so wait for checkbox to be set.
    if ( className == QString("QCheckBox") )
        waitFor(100);

    COPYQ_LOG( QString("Sending keys \"%1\" to %2.")
               .arg(keys)
               .arg(widgetName) );

    if ( keys.startsWith(":") ) {
        const auto text = keys.mid(1);

        const auto popupMessage = QString("%1 (%2)")
                .arg( quoteString(text) )
                .arg(widgetName);
        const int msec = std::max( 1500, delay * text.size() );
        showMessage(QString(), popupMessage, IconKeyboard, msec);

        QTest::keyClicks(widget, text, Qt::NoModifier, delay);

        // Increment key clicks sequence number after typing all the text.
        ++m_receivedKeyClicks;
    } else {
        // Increment key clicks sequence number before opening any modal dialogs.
        ++m_receivedKeyClicks;

        const QKeySequence shortcut(keys, QKeySequence::PortableText);

        if ( shortcut.isEmpty() ) {
            COPYQ_LOG( QString("Cannot parse shortcut \"%1\"!").arg(keys) );
            return;
        }

        const auto popupMessage = QString("%1 (%2)")
                .arg(shortcut.toString())
                .arg(widgetName);
        const int msec = std::min( 8000, std::max( 1500, delay * 40 ) );
        showMessage(QString(), popupMessage, IconKeyboard, msec);

        const auto key = static_cast<uint>(shortcut[0]);
        QTest::keyClick( widget,
                         Qt::Key(key & ~Qt::KeyboardModifierMask),
                         Qt::KeyboardModifiers(key & Qt::KeyboardModifierMask),
                         0 );
    }

    COPYQ_LOG( QString("Key \"%1\" sent to %2.")
               .arg(keys)
               .arg(widgetName) );
}

uint MainWindow::sendKeyClicks(const QString &keys, int delay)
{
    // Don't stop when modal window is open.
    QMetaObject::invokeMethod( this, "keyClicks", Qt::QueuedConnection,
                               Q_ARG(const QString &, keys),
                               Q_ARG(int, delay)
                               );

    return ++m_sentKeyClicks;
}

uint MainWindow::lastReceivedKeyClicks()
{
    return m_receivedKeyClicks;
}

void MainWindow::resetTestSession(const QString &clipboardTabName)
{
    if ( qApp->property("CopyQ_test_id").toString().isEmpty() )
        return;

    // Remove all tabs except the clipboard tab for tests.
    while ( ui->tabWidget->count() != 1 )
        removeTab(false, 0);

    const auto placeholder = createTab(clipboardTabName, MatchExactTabName);
    if ( placeholder != getPlaceholder(0) )
        removeTab(false, 0);

    {
        const auto c = placeholder->createBrowser();
        Q_ASSERT(c);

        // Remove all items from the clipboard tab.
        const auto model = c->model();
        model->removeRows( 0, model->rowCount() );
        c->saveItems();
    }

    setCommands(QList<Command>());

    resetTestsSettings();

    // Set clipboard tab.
    AppConfig().setOption( Config::clipboard_tab::name(), clipboardTabName );

    emit configurationChanged();

    // Assert everything after configuration is loaded.
    Q_ASSERT( ui->tabWidget->count() == 1 );
    Q_ASSERT( placeholder == getPlaceholder(0) );
    Q_ASSERT( placeholder->createBrowser() != nullptr );
    Q_ASSERT( placeholder->createBrowser()->length() == 0 );
}
#endif

void MainWindow::updateNotifications()
{
    if (m_notifications == nullptr) {
        m_notifications = new NotificationDaemon(this);
        connect( m_notifications, SIGNAL(notificationButtonClicked(NotificationButton)),
                 this, SLOT(onNotificationButtonClicked(NotificationButton)) );
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
    connect( placeholder, SIGNAL(browserCreated(ClipboardBrowser*)),
             this, SLOT(onBrowserCreated(ClipboardBrowser*)) );

    ui->tabWidget->addTab(placeholder, name);
    saveTabPositions();
    return placeholder;
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
    QAction *act = actionForMenuItem(id, getPlaceholder(), Qt::WidgetWithChildrenShortcut);
    if (slot)
        connect( act, SIGNAL(triggered()), receiver, slot, Qt::UniqueConnection );
    m_menuItem->addAction(act);
    return act;
}

QList<Command> MainWindow::commandsForMenu(const QVariantMap &data, const QString &tabName)
{
    QList<Command> commands;
    for (const auto &command : m_commands) {
        if ( command.inMenu && !command.name.isEmpty() && canExecuteCommand(command, data, tabName) ) {
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
    if ( m_commands.isEmpty() )
        return;

    const auto data = addSelectionData(*c);
    const QList<Command> commands = commandsForMenu(data, c->tabName());

    QList<QKeySequence> usedShortcuts = m_disabledShortcuts;

    QList<Command> disabledCommands;

    for (const auto &command : commands) {
        QString name = command.name;
        QMenu *currentMenu = createSubMenus(&name, m_menuItem);
        QAction *act = new CommandAction(command, name, currentMenu);
        c->addAction(act);

        if (!command.matchCmd.isEmpty()) {
            act->setDisabled(true);
            disabledCommands.append(command);
        }

        connect(act, SIGNAL(triggerCommand(CommandAction*,QString)),
                this, SLOT(onItemCommandActionTriggered(CommandAction*,QString)));

        QList<QKeySequence> uniqueShortcuts;

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

    m_itemMenuCommandTester.setCommands(disabledCommands, data);
    m_itemMenuCommandTester.start();
}

void MainWindow::addCommandsToTrayMenu(const QVariantMap &clipboardData)
{
    if ( m_commands.isEmpty() )
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

    const QList<Command> commands = commandsForMenu(data, c->tabName());

    QList<Command> disabledCommands;

    for (const auto &command : commands) {
        QString name = command.name;
        QMenu *currentMenu = createSubMenus(&name, m_trayMenu);
        QAction *act = new CommandAction(command, name, currentMenu);

        if (!command.matchCmd.isEmpty()) {
            act->setDisabled(true);
            disabledCommands.append(command);
        }

        connect(act, SIGNAL(triggerCommand(CommandAction*,QString)),
                this, SLOT(onClipboardCommandActionTriggered(CommandAction*,QString)));
    }

    m_trayMenuCommandTester.setCommands(disabledCommands, data);
    m_trayMenuCommandTester.start();
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
        } else if ( action->isEnabled() && !action->icon().isNull() ) {
            const QIcon icon = action->icon();
            const QString text = action->text().remove("&");
            const QString shortcut = action->shortcut().toString(QKeySequence::NativeText);
            const QString label = text + (shortcut.isEmpty() ? QString() : "\n[" + shortcut + "]");
            const QString tooltip = "<center>" + escapeHtml(text)
                    + (shortcut.isEmpty() ? QString() : "<br /><b>" + escapeHtml(shortcut) + "</b>") + "</center>";
            act = ui->toolBar->addAction( icon, label, action, SIGNAL(triggered()) );
            act->setToolTip(tooltip);

            if ( action->isCheckable() ) {
                act->setCheckable(true);
                act->setChecked(action->isChecked());
                connect( act, SIGNAL(triggered(bool)),
                         action, SLOT(setChecked(bool)) );
                connect( action, SIGNAL(toggled(bool)),
                         act, SLOT(setChecked(bool)) );
            }
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

void MainWindow::runAutomaticCommand(const Command &command)
{
    Q_ASSERT(!m_currentAutomaticCommand);

    const QVariantMap data = m_automaticCommandTester.data();

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

void MainWindow::onEscape()
{
    if ( browseMode() ) {
        hideWindow();
        auto c = browser();
        if (c)
            c->setCurrent(0);
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
    for (const auto &shortcut : m_disabledShortcuts)
        shortcuts.removeAll(shortcut);

    action->setShortcuts(shortcuts);
}

void MainWindow::updateActionShortcuts()
{
    for (int id = 0; id < m_actions.size(); ++id)
        updateActionShortcuts(id);
}

void MainWindow::pasteClipboard(const PlatformWindowPtr &window)
{
    // Wait for clipboard to be set (and message sent to clipboard monitor process).
    QApplication::processEvents(QEventLoop::WaitForMoreEvents, 50);

    window->pasteClipboard();
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

void MainWindow::onMenuActionTriggered(ClipboardBrowser *c, uint itemHash, bool omitPaste)
{
    if ( !c || !c->moveToClipboard(itemHash) )
        return;

    PlatformWindowPtr lastWindow = m_lastWindow;

    if ( m_options.trayItemPaste && lastWindow && !omitPaste && canPaste() )
        pasteClipboard(lastWindow);
}

QWidget *MainWindow::toggleMenu(TrayMenu *menu, const QPoint &pos)
{
    if ( menu->isVisible() ) {
        menu->close();
        return menu;
    }

    menu->popup( toScreen(pos, menu->width(), menu->height()) );

    menu->raise();
    menu->activateWindow();
    QApplication::setActiveWindow(menu);
    QApplication::processEvents();
    stealFocus(*menu);

    return menu;
}

QWidget *MainWindow::toggleMenu(TrayMenu *menu)
{
    return toggleMenu(menu, QCursor::pos());
}

bool MainWindow::exportData(const QString &fileName, const QStringList &tabs, bool exportConfiguration, bool exportCommands)
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

        auto c = browser(i);
        if (!c)
            return false;

        const auto &tabName = c->tabName();

        QByteArray tabBytes;
        {
            QDataStream tabOut(&tabBytes, QIODevice::WriteOnly);
            tabOut.setVersion(QDataStream::Qt_4_7);
            if ( !serializeData(*c->model(), &tabOut) )
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
        if (!c)
            return false;

        const auto tabBytes = tabMap.value("data").toByteArray();
        QDataStream tabIn(tabBytes);
        tabIn.setVersion(QDataStream::Qt_4_7);

        // Don't read items based on current value of "maxitems" option since
        // the option can be later also imported.
        const int maxItems = importConfiguration ? Config::maxItems : m_sharedData->maxItems;
        if ( !deserializeData( c->model(), &tabIn, maxItems ) )
            return false;
    }

    if (importConfiguration) {
        // Configuration dialog shouldn't be open.
        if (cm)
            return false;

        Settings settings;

        for ( const auto &name : settingsMap.keys() )
            settings.setValue(name, settingsMap[name]);

        emit configurationChanged();
    }

    if (importCommands) {
        // Close command dialog.
        if ( !maybeCloseCommandDialog() )
            return false;

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
            for ( const auto &key : commandMap.keys() )
                settings.setValue(key, commandMap[key]);
        }

        settings.endArray();

        onCommandDialogSaved();
    }

    return in->status() == QDataStream::Ok;
}

const Theme &MainWindow::theme() const
{
    return m_sharedData->theme;
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
            QString tabName = w->tabText(i);
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
    return m_actionHandler->hasRunningAction();
}

bool MainWindow::maybeCloseCommandDialog()
{
    return !m_commandDialog || m_commandDialog->maybeClose(this);
}

void MainWindow::showMessage(
        const QString &title,
        const QString &msg,
        ushort icon,
        int msec,
        const QString &notificationId,
        const NotificationButtons &buttons)
{
    showMessage(title, msg, QString(QChar(icon)), msec, notificationId, buttons);
}

void MainWindow::showMessage(
        const QString &title,
        const QString &msg,
        const QString &icon,
        int msec,
        const QString &notificationId,
        const NotificationButtons &buttons)
{
    notificationDaemon()->create(title, msg, icon, msec, notificationId, buttons);
}

void MainWindow::showClipboardMessage(const QVariantMap &data)
{
    if ( m_options.itemPopupInterval != 0 && m_options.clipboardNotificationLines > 0) {
        const QString clipboardNotificationId = "CopyQ_clipboard_notification";
        if (data.isEmpty()) {
            notificationDaemon()->removeNotification(clipboardNotificationId);
        } else {
            notificationDaemon()->create(
                        data, m_options.clipboardNotificationLines, QString(QChar(IconPaste)),
                        m_options.itemPopupInterval * 1000, clipboardNotificationId,
                        NotificationButtons() );
        }
    }
}

void MainWindow::showError(const QString &msg)
{
    showMessage( tr("CopyQ Error", "Notification error message title"),
                 msg, IconRemoveSign );
}

void MainWindow::addCommands(const QList<Command> &commands)
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
                if ( !c->hasFocus() )
                    c->setFocus();

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
            resetStatus();
            if (c)
                c->setCurrent(0);
            break;
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
        updateWindowTransparency();
    } else if (type == QEvent::WindowDeactivate) {
        m_timerShowWindow.start();
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
    delayedUpdateForeignFocusWindows();
    return QMainWindow::x11Event(event);
}
#   elif defined(Q_OS_WIN)
bool MainWindow::winEvent(MSG *message, long *result)
{
    delayedUpdateForeignFocusWindows();
    return QMainWindow::winEvent(message, result);
}
#   elif defined(Q_OS_MAC)
bool MainWindow::macEvent(EventHandlerCallRef caller, EventRef event)
{
    delayedUpdateForeignFocusWindows();
    return QMainWindow::macEvent(caller, event);
}
#   endif
#else
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
    delayedUpdateForeignFocusWindows();
    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif

void MainWindow::resetStatus()
{
    if ( !ui->searchBar->text().isEmpty() )
        ui->searchBar->clear();
    enterBrowseMode();
}

void MainWindow::loadSettings()
{
    COPYQ_LOG("Loading configuration");

    QSettings settings;

    loadItemFactorySettings(m_sharedData->itemFactory, &settings);

    settings.beginGroup("Theme");
    m_sharedData->theme.loadTheme(settings);
    settings.endGroup();

    theme().decorateToolBar(ui->toolBar);
    theme().decorateMainWindow(this);
    ui->scrollAreaItemPreview->setObjectName("ClipboardBrowser");
    theme().decorateItemPreview(ui->scrollAreaItemPreview);

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

    m_options.transparency = appConfig.option<Config::transparency>();
    m_options.transparencyFocused = appConfig.option<Config::transparency_focused>();
    updateWindowTransparency();

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

    // shared data for browsers
    m_sharedData->loadFromConfiguration();

    // reload settings of existing tabs
    for( int i = 0; i < ui->tabWidget->count(); ++i )
        getPlaceholder(i)->reloadBrowser();

    // create tabs
    const QStringList tabs = savedTabs();
    for (const auto &name : tabs)
        createTab(name, MatchExactTabName);

    if ( ui->tabWidget->count() == 0 )
        createTab( defaultTabName(), MatchExactTabName );
    else
        setTabs(tabs); // Save any tabs loaded from new tab files.

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
    m_options.trayMenuOpenOnLeftClick = appConfig.option<Config::tray_menu_open_on_left_click>();
    m_options.itemPopupInterval = appConfig.option<Config::item_popup_interval>();
    m_options.clipboardNotificationLines = appConfig.option<Config::clipboard_notification_lines>();
    m_options.clipboardTab = appConfig.option<Config::clipboard_tab>();

    m_trayMenu->setStyleSheet( theme().getToolTipStyleSheet() );
    m_menu->setStyleSheet( theme().getToolTipStyleSheet() );

    initTray();
    updateTrayMenuItems();

    if (m_notifications != nullptr)
        updateNotifications();

    updateIcon();

    ui->searchBar->loadSettings();

    settings.beginGroup("Shortcuts");
    loadShortcuts(&m_menuItems, settings);
    settings.endGroup();

    resetStatus();

    COPYQ_LOG("Configuration loaded");
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

void MainWindow::showBrowser(int index)
{
    if (setCurrentTab(index))
        showWindow();
}

void MainWindow::onMenuActionTriggered(uint itemHash, bool omitPaste)
{
    onMenuActionTriggered( getTabForMenu(), itemHash, omitPaste );
}

void MainWindow::onTrayActionTriggered(uint itemHash, bool omitPaste)
{
    onMenuActionTriggered( getTabForTrayMenu(), itemHash, omitPaste );
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
    return toggleMenu(m_trayMenu);
}

bool MainWindow::toggleMenu(const QString &tabName, int itemCount, const QPoint &position)
{
    // Just close the previously opened menu if parameters are the same.
    if ( m_menu->isVisible()
         && (m_menuTabName == tabName && m_menuMaxItemCount == itemCount) )
    {
        m_menu->close();
        return m_menu;
    }

    WidgetSizeGuard sizeGuard(m_menu);

    m_menuTabName = tabName;
    m_menuMaxItemCount = itemCount;
    if (m_menuMaxItemCount < 0)
        m_menuMaxItemCount = m_options.trayItems > 0 ? m_options.trayItems : 10;

    m_menu->clearAllActions();
    addMenuItems(QString());

    if ( m_menu->isVisible() )
        m_menu->close();

    if ( m_menu->isEmpty() )
        return m_menu;

    if (position.x() >= 0 && position.y() >= 0)
        return toggleMenu(m_menu, position);

    return toggleMenu(m_menu);
}

void MainWindow::tabChanged(int current, int)
{
    bool currentIsTabGroup = current == -1;

    emit tabGroupSelected(currentIsTabGroup);

    if (currentIsTabGroup) {
        m_actionHandler->setCurrentTab(QString());
    } else {
        // update item menu (necessary for keyboard shortcuts to work)
        auto c = browser();
        if (c) {
            c->filterItems( ui->searchBar->filter() );

            if ( current >= 0 ) {
                if( !c->currentIndex().isValid() && isVisible() ) {
                    c->setCurrent(0);
                }
            }

            setTabOrder(ui->searchBar, c);

            m_actionHandler->setCurrentTab(c->tabName());
        }
    }

    updateContextMenu();

    if (m_options.trayCurrentTab)
        updateTrayMenuItems();
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

void MainWindow::tabMenuRequested(const QPoint &pos, int tab)
{
    auto placeholder = getPlaceholder(tab);
    if (placeholder == nullptr)
        return;
    const QString tabName = placeholder->tabName();
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
    auto c = tab(tabName);
    if (c)
        c->addUnique(data);
}

void MainWindow::updateFirstItem(QVariantMap data)
{
    const bool syncToSelection = data.remove(mimeSyncToSelection) != 0;
    const bool syncToClipboard = data.remove(mimeSyncToClipboard) != 0;
    const QString outputTab = getTextData( data.take(mimeOutputTab).toByteArray() );

    // Synchronize clipboard and X11 selection.
    if (syncToSelection)
        emit changeClipboard(data, QClipboard::Selection);

    if (syncToClipboard)
        emit changeClipboard(data, QClipboard::Clipboard);

    if ( !outputTab.isEmpty() ) {
        auto c = tab(outputTab);
        if (c) {
            c->addUnique(data);
            c->setCurrent(0);
        }
    }
}

QStringList MainWindow::config(const QStringList &nameValue)
{
    ConfigurationManager configurationManager;

    if ( nameValue.isEmpty() ) {
        QStringList options = configurationManager.options();
        options.sort();
        QString opts;
        for (const auto &option : options)
            opts.append( option + "\n  " + configurationManager.optionToolTip(option).replace('\n', "\n  ") + '\n' );
        return QStringList(opts);
    }

    QStringList result;

    // Check if option names are valid.
    for (int i = 0; i < nameValue.size(); i += 2) {
        const auto &name = nameValue[i];
        if ( !configurationManager.options().contains(name) ) {
            result.append(nameValue[i]);
            result.append(QString());
        }
    }

    configurationManager.loadSettings();

    if ( result.isEmpty() ) {
        bool emitConfigurationChanged = false;
        for (int i = 0; i < nameValue.size(); i += 2) {
            const auto &name = nameValue[i];
            const auto value = nameValue.value(i + 1);
            if ( !value.isNull() && configurationManager.setOptionValue(name, value) )
                emitConfigurationChanged = true;

            result.append(nameValue[i]);
            result.append( configurationManager.optionValue(name) );
        }

        if (emitConfigurationChanged)
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

void MainWindow::setCommands(const QList<Command> &commands)
{
    if ( !maybeCloseCommandDialog() )
        return;

    m_commands = commands;
    saveCommands(commands);
    updateContextMenu();
    if (m_options.trayCommands)
        updateTrayMenuItems();
}

void MainWindow::runAutomaticCommands(QVariantMap data)
{
    bool isClipboard = isClipboardData(data);

    // Don't abort currently commands if X11 selection changes rapidly.
    if (!isClipboard && (!m_automaticCommandTester.isCompleted() || m_currentAutomaticCommand))
        return;

    QList<Command> commands;
    const QString tabName = defaultTabName();
    for (const auto &command : m_commands) {
        if (command.automatic) {
            commands.append(command);
            if ( command.outputTab.isEmpty() )
                commands.last().outputTab = tabName;
        }
    }

    const bool storeData = needStore(data);
    if (storeData)
        setTextData(&data, m_options.clipboardTab, mimeOutputTab);

    const bool syncClipboardToSelection = needSyncClipboardToSelection(data);
    if (syncClipboardToSelection)
        data.insert(mimeSyncToSelection, QByteArray());

    const bool syncSelectionToClipboard = needSyncSelectionToClipboard(data);
    if (syncSelectionToClipboard)
        data.insert(mimeSyncToClipboard, QByteArray());

    // Add clipboard data to clipboard tab (if configured) and synchronize clipboard/selection.
    if (syncClipboardToSelection || syncSelectionToClipboard || storeData)
        commands.append(automaticCommand("Add Item from Clipboard", "updateFirst()"));

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

    // Some menu commands may depend on clipboard content.
    updateContextMenu();
}

void MainWindow::setClipboard(const QVariantMap &data, QClipboard::Mode mode)
{
    emit changeClipboard(data, mode);
}

void MainWindow::setClipboard(const QVariantMap &data)
{
    setClipboard(data, QClipboard::Clipboard);
#ifdef HAS_MOUSE_SELECTIONS
    setClipboard(data, QClipboard::Selection);
#endif
}

void MainWindow::activateCurrentItem()
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

    // Copy current item or selection to clipboard.
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
        if (m_options.activatePastes() && canPaste())
            pasteClipboard(lastWindow);
        else if (m_options.activateFocuses())
            lastWindow->raise();
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
        connect( m_tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
                 this, SLOT(trayActivated(QSystemTrayIcon::ActivationReason)) );
        updateIcon();
        updateTrayTooltip();
        m_tray->setContextMenu(m_trayMenu);
        m_tray->show();

        if ( isMinimized() )
            hideWindow();
    } else {
        m_timerTrayAvailable.start();
    }
}

void MainWindow::updateFocusWindows()
{
    PlatformPtr platform = createPlatformNativeInterface();
    PlatformWindowPtr lastWindow = platform->getCurrentWindow();
    if (lastWindow)
        m_lastWindow = lastWindow;
}

void MainWindow::hideSearchBar()
{
    ui->searchBar->hide();
    ui->searchBar->clear();
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
        } else if ( c->hasFocus() ) {
            c->setCurrent( c->currentIndex().row() + (next ? 1 : -1) );
        } else {
            c->setFocus();
        }
    }
}

void MainWindow::enterBrowseMode()
{
    getPlaceholder()->setFocus();
    if ( ui->searchBar->text().isEmpty() )
        ui->searchBar->hide();
}

void MainWindow::enterSearchMode()
{
    ui->searchBar->show();
    ui->searchBar->setFocus(Qt::ShortcutFocusReason);
}
void MainWindow::enterSearchMode(const QString &txt)
{
    enterSearchMode();
    ui->searchBar->setText( ui->searchBar->text() + txt );
}

void MainWindow::updateTrayMenuItems()
{
    WidgetSizeGuard sizeGuard(m_trayMenu);

    m_trayMenuCommandTester.abort();
    m_trayMenu->clearAllActions();

    QAction *act = m_trayMenu->addAction(
                appIcon(), tr("&Show/Hide"), this, SLOT(toggleVisible()) );
    m_trayMenu->setDefaultAction(act);
    addTrayAction(Actions::File_Preferences);
    addTrayAction(Actions::File_ToggleClipboardStoring);
    m_trayMenu->addSeparator();
    addTrayAction(Actions::File_Exit);

    addTrayMenuItems(QString());

    // Add commands.
    if (m_options.trayCommands) {
        // Show clipboard content as disabled item.
        const QString format = tr("&Clipboard: %1", "Tray menu clipboard item format");
        act = m_trayMenu->addAction( iconClipboard(),
                                            QString(), this, SLOT(showClipboardContent()) );
        act->setText( textLabelForData(m_clipboardData, act->font(), format, true) );
        m_trayMenu->addCustomAction(act);

        int i = m_trayMenu->actions().size();
        addCommandsToTrayMenu(m_clipboardData);
        QList<QAction *> actions = m_trayMenu->actions();
        for ( ; i < actions.size(); ++i )
            m_trayMenu->addCustomAction(actions[i]);
    }
}

void MainWindow::addMenuItems(const QString &searchText)
{
    addMenuItems(m_menu, getTabForMenu(), m_menuMaxItemCount, searchText);
}

void MainWindow::addTrayMenuItems(const QString &searchText)
{
    addMenuItems(m_trayMenu, getTabForTrayMenu(), m_options.trayItems, searchText);
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

void MainWindow::openActionDialog(const QVariantMap &data)
{
    std::unique_ptr<ActionDialog> actionDialog( m_actionHandler->createActionDialog(ui->tabWidget->tabs()) );
    connect( actionDialog.get(), SIGNAL(saveCommand(Command)),
             this, SLOT(onSaveCommand(Command)) );

    actionDialog->setInputData(data);
    actionDialog->show();
    stealFocus(*actionDialog.release());
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
    connect( &configurationManager, SIGNAL(configurationChanged()),
             this, SIGNAL(configurationChanged()) );
    connect( &configurationManager, SIGNAL(error(QString)),
             this, SLOT(showError(QString)) );
    connect( &configurationManager, SIGNAL(openCommandDialogRequest()),
             this, SLOT(openCommands()) );

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
    auto c = browser();
    if (!c)
        return false;

    ImportExportDialog exportDialog(this);
    exportDialog.setWindowTitle( tr("CopyQ Options for Export") );
    exportDialog.setTabs( ui->tabWidget->tabs() );
    exportDialog.setCurrentTab( c->tabName() );
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

    if ( !exportData(fileName, tabs, exportConfiguration, exportCommands) ) {
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

bool MainWindow::importData(const QString &fileName, ImportOptions options)
{
    // Compatibility with v2.9.0 and earlier.
    if ( loadTab(fileName) )
        return true;

    QFile file(fileName);
    if ( !file.open(QIODevice::ReadOnly) )
        return false;

    QDataStream in(&file);

    return importDataV3(&in, options);
}

bool MainWindow::exportAllData(const QString &fileName)
{
    const auto tabs = ui->tabWidget->tabs();
    const bool exportConfiguration = true;
    const bool exportCommands = true;

    return exportData(fileName, tabs, exportConfiguration, exportCommands);
}

bool MainWindow::importData()
{
    const auto fileName = QFileDialog::getOpenFileName(
                this, QString(), QString(), importExportFileDialogFilter() );
    if ( fileName.isNull() )
        return false;

    if ( !importData(fileName, ImportOptions::Select) ) {
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
        std::unique_ptr<ActionDialog> actionDialog( m_actionHandler->createActionDialog(ui->tabWidget->tabs()) );

        actionDialog->setInputData(data);
        actionDialog->setCommand(cmd);
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
        actionDialog.release();
    } else if ( cmd.cmd.isEmpty() ) {
        m_actionHandler->addFinishedAction(cmd.name);
    } else {
        auto act = new Action();
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

    return nullptr;
}

void MainWindow::newTab(const QString &name)
{
    auto d = new TabDialog(TabDialog::TabNew, this);
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
    auto d = new TabDialog(TabDialog::TabGroupRename, this);
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
    const int i = tab >= 0 ? tab : ui->tabWidget->currentIndex();
    if (i < 0)
        return;

    auto d = new TabDialog(TabDialog::TabRename, this);
    d->setAttribute(Qt::WA_DeleteOnClose, true);
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

    auto placeholder = getPlaceholder(tabIndex);
    if (placeholder) {
        updateTabIcon(name, placeholder->tabName());
        placeholder->setTabName(name);
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
                           ).arg( w->tabText(tabIndex).remove('&') ),
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
