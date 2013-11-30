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
#include "common/common.h"
#include "common/command.h"
#include "common/contenttype.h"
#include "gui/aboutdialog.h"
#include "gui/actiondialog.h"
#include "gui/clipboardbrowser.h"
#include "gui/clipboarddialog.h"
#include "gui/configtabappearance.h"
#include "gui/configurationmanager.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "gui/notificationdaemon.h"
#include "gui/tabdialog.h"
#include "gui/tabwidget.h"
#include "gui/traymenu.h"
#include "item/clipboarditem.h"
#include "item/clipboardmodel.h"
#include "item/serialize.h"
#include "platform/platformnativeinterface.h"

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
#include <QPainter>

#ifdef HAS_TESTS
#   include <QTest>
#endif

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

const QIcon iconClipboard() { return getIcon("clipboard", IconPaste); }
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

void colorizePixmap(QPixmap *pix, const QColor &from, const QColor &to)
{
    QPixmap pix2( pix->size() );
    pix2.fill(to);
    pix2.setMask( pix->createMaskFromColor(from, Qt::MaskOutColor) );

    QPainter p(pix);
    p.drawPixmap(0, 0, pix2);
}

QColor sessionNameToColor(const QString &name)
{
    if (name.isEmpty())
        return QColor(Qt::white);

    int r = 0;
    int g = 0;
    int b = 0;

    foreach (const QChar &c, name) {
        const ushort x = c.unicode() % 3;
        if (x == 0)
            r += 255;
        else if (x == 1)
            g += 255;
        else
            b += 255;
    }

    int max = qMax(r, qMax(g, b));
    r = r * 255 / max;
    g = g * 255 / max;
    b = b * 255 / max;

    return QColor(r, g, b);
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
        , hideMenuBar(false)
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
    bool hideMenuBar;

    int itemActivationCommands;

    bool clearFirstTab;

    bool showTray;
    bool trayItemPaste;
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_menuCommand(NULL)
#ifdef Q_OS_MAC
    , m_trayMenuCommand(NULL)
#endif // Q_OS_MAC
    , m_menuItem(NULL)
    , m_trayMenu( new TrayMenu(this) )
    , m_tray( new QSystemTrayIcon(this) )
    , m_browsemode(false)
    , m_options(new MainWindowOptions)
    , m_monitoringDisabled(false)
    , m_actionToggleMonitoring()
    , m_actionMonitoringDisabled()
    , m_actions()
    , m_sharedData(new ClipboardBrowserShared)
    , m_trayPasteWindow()
    , m_pasteWindow(0)
    , m_lastWindow(0)
    , m_timerUpdateFocusWindows( new QTimer(this) )
    , m_timerShowWindow( new QTimer(this) )
    , m_trayTimer(NULL)
    , m_sessionName()
    , m_notifications(NULL)
    , m_timerMiminizing(NULL)
    , m_minimizeUnsupported(false)
{
    ui->setupUi(this);

    // create configuration manager
    ConfigurationManager::createInstance(this);

    ConfigurationManager::instance()->loadGeometry(this);

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
    connect( this, SIGNAL(changeClipboard(const ClipboardItem*)),
             this, SLOT(clipboardChanged(const ClipboardItem*)) );

    // settings
    loadSettings();

    ui->tabWidget->setCurrentIndex(0);

    m_timerUpdateFocusWindows->setSingleShot(true);
    m_timerUpdateFocusWindows->setInterval(50);

    m_timerShowWindow->setSingleShot(true);
    m_timerShowWindow->setInterval(250);

    // notify window if configuration changes
    ConfigurationManager *cm = ConfigurationManager::instance();
    connect( cm, SIGNAL(configurationChanged()),
             this, SLOT(loadSettings()) );

    // browse mode by default
    enterBrowseMode();

    m_tray->setContextMenu(m_trayMenu);
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
        QApplication::exit();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if ( !closeMinimizes() ) {
        hide();
    } else {
        if ( isMinimized() ) {
            QApplication::exit();
            return;
        }
        showMinimized();
    }

    event->ignore();
    QMainWindow::closeEvent(event);
}

void MainWindow::hideEvent(QHideEvent *event)
{
    QMainWindow::hideEvent(event);
    if ( closeMinimizes() )
        showMinimized();
}

void MainWindow::showEvent(QShowEvent *event)
{
    m_timerShowWindow->start();
    QMainWindow::showEvent(event);
#ifdef COPYQ_WS_X11
    ConfigurationManager::instance()->loadGeometry(this);
#endif
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (!m_timerShowWindow->isActive())
        ConfigurationManager::instance()->saveGeometry(this);
}

void MainWindow::moveEvent(QMoveEvent *event)
{
    QMainWindow::moveEvent(event);
    if (!m_timerShowWindow->isActive())
        ConfigurationManager::instance()->saveGeometry(this);
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
    act = createAction( Actions::File_ToggleClipboardStoring, SLOT(toggleMonitoring()), menu );
    m_actionToggleMonitoring = act;
    m_actionMonitoringDisabled = act;
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
    m_menuCommand = menubar->addMenu(tr("Co&mmands"));
    m_menuCommand->setEnabled(false);

#ifdef Q_OS_MAC
    // Make a copy of m_menuCommand for the trayMenu, necessary on OS X due
    // to QTBUG-31549, and/or QTBUG-34160
    m_trayMenuCommand = m_trayMenu->addMenu(tr("Co&mmands"));
    m_trayMenuCommand->setEnabled(false);
#else
    m_trayMenu->addMenu(m_menuCommand);
#endif // Q_OS_MAC

    // Help
    menu = menubar->addMenu(tr("&Help"));
    act = createAction( Actions::Help_Help, SLOT(openAboutDialog()), menu );

    // Tray menu
    act = m_trayMenu->addAction( iconTray(false), tr("&Show/Hide"),
                                 this, SLOT(toggleVisible()) );
    m_trayMenu->setDefaultAction(act);
    addTrayAction(Actions::File_New);
    act = addTrayAction(Actions::Item_Action);
    act->setWhatsThis( tr("Open action dialog") );
    addTrayAction(Actions::File_Preferences);
    addTrayAction(Actions::File_ToggleClipboardStoring);
    addTrayAction(Actions::File_Exit);
#ifdef Q_OS_MAC
    m_trayMenu->addMenu(m_trayMenuCommand);
#else
    m_trayMenu->addMenu(m_menuCommand);
#endif // Q_OS_MAC
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

void MainWindow::closeAction(Action *action)
{
    QString msg;

    QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information;

    if ( action->actionFailed() || action->exitStatus() != QProcess::NormalExit ) {
        msg += tr("Error: %1\n").arg(action->errorString()) + action->errorOutput();
        icon = QSystemTrayIcon::Critical;
    } else if ( action->exitCode() != 0 ) {
        msg += tr("Exit code: %1\n").arg(action->exitCode()) + action->errorOutput();
        icon = QSystemTrayIcon::Warning;
    } else if ( !action->inputFormats().isEmpty() ) {
        QModelIndex index = action->index();
        ClipboardBrowser *c = findBrowser(index);
        if (c) {
            QStringList removeFormats;
            if ( action->inputFormats().size() > 1 ) {
                QVariantMap data;
                deserializeData( &data, action->input() );
                removeFormats = data.keys();
            } else {
                removeFormats.append( action->inputFormats()[0] );
            }

            removeFormats.removeAll( action->outputFormat() );
            if ( !removeFormats.isEmpty() )
                c->model()->setData(index, removeFormats, contentType::removeFormats);
        }
    }

    if ( !msg.isEmpty() )
        showMessage( tr("Command %1")
                     .arg(quoteString(action->command())), msg, icon );

    delete m_actions.take(action);
    action->deleteLater();

    if ( m_actions.isEmpty() ) {
        m_menuCommand->setEnabled(false);
#ifdef Q_OS_MAC
        m_trayMenuCommand->setEnabled(false);
#endif // Q_OS_MAC
        updateIcon();
    }
}

void MainWindow::updateIcon()
{
    QIcon icon = iconTray(m_monitoringDisabled);
    QColor color = sessionNameToColor(m_sessionName);

    if (m_options->showTray) {
        if ( !m_actions.isEmpty() ) {
            m_tray->setIcon( iconTrayRunning(m_monitoringDisabled) );
        } else if ( m_sessionName.isEmpty() ) {
            m_tray->setIcon(icon);
        } else {
            QPixmap trayPix = icon.pixmap( m_options->showTray ? m_tray->geometry().size() : iconSize() );
            colorizePixmap( &trayPix, QColor(0x7f, 0xca, 0x9b), color );
            m_tray->setIcon( QIcon(trayPix) );
        }
    }

    if ( m_sessionName.isEmpty() ) {
        setWindowIcon(icon);
    } else {
        QPixmap pix = icon.pixmap( m_options->showTray ? m_tray->geometry().size() : iconSize() );
        colorizePixmap( &pix, QColor(0x7f, 0xca, 0x9b), color );
        setWindowIcon( QIcon(pix) );
    }
}

void MainWindow::updateNotifications()
{
    if (m_notifications == NULL)
        m_notifications = new NotificationDaemon(this);

    ConfigurationManager *cm = ConfigurationManager::instance();
    const ConfigTabAppearance *appearance = cm->tabAppearance();
    m_notifications->setBackground( appearance->themeColor("notification_bg") );
    m_notifications->setForeground( appearance->themeColor("notification_fg") );

    QFont font = appearance->themeFont("notification_font");
    m_notifications->setFont(font);

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
        ClipboardBrowser *c = getBrowser(i);
        if (c->model() == index.model())
            return c;
    }

    return NULL;
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

bool MainWindow::isValidWindow(WId wid) const
{
    return wid != WId();
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

void MainWindow::setHideMenuBar(bool hide)
{
    if (!m_options->hideMenuBar)
        return;

    const QColor color = palette().color(QPalette::Highlight);
    ui->widgetShowMenuBar->setStyleSheet( QString("*{background-color:%1}").arg(color.name()) );
    ui->widgetShowMenuBar->installEventFilter(this);
    ui->widgetShowMenuBar->show();

    menuBar()->setNativeMenuBar(!m_options->hideMenuBar);

    // Hiding menu bar completely disables shortcuts for child QAction.
    menuBar()->setStyleSheet(hide ? QString("QMenuBar{height:0}") : QString());
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
    bool noText = !data.contains(mimeText);
    const QString text = noText ? QString() : getTextData(data);
    const QString windowTitle = data.value(mimeWindowTitle).toString();

    foreach (const Command &c, m_sharedData->commands) {
        if (c.automatic && (c.remove || !c.cmd.isEmpty() || !c.tab.isEmpty())) {
            if ( ((noText && c.re.isEmpty()) || (!noText && c.re.indexIn(text) != -1))
                 && (c.input.isEmpty() || c.input == mimeItems || data.contains(c.input))
                 && (windowTitle.isNull() || c.wndre.indexIn(windowTitle) != -1) )
            {
                if (c.automatic) {
                    Command cmd = c;
                    if ( cmd.outputTab.isEmpty() )
                        cmd.outputTab = sourceTab;

                    if ( cmd.input.isEmpty() || cmd.input == mimeItems || data.contains(cmd.input) )
                        action(data, cmd);
                }
                if (!c.tab.isEmpty())
                    addToTab(data, c.tab);
                if (c.remove || c.transform)
                    return false;
            }
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

    connect( c, SIGNAL(changeClipboard(const ClipboardItem*)),
             this, SLOT(onChangeClipboardRequest(const ClipboardItem*)) );
    connect( c, SIGNAL(requestActionDialog(const QVariantMap, const Command)),
             this, SLOT(action(const QVariantMap, const Command)) );
    connect( c, SIGNAL(requestActionDialog(const QVariantMap, const Command, const QModelIndex)),
             this, SLOT(action(const QVariantMap&, const Command, const QModelIndex)) );
    connect( c, SIGNAL(requestActionDialog(const QVariantMap)),
             this, SLOT(openActionDialog(const QVariantMap)) );
    connect( c, SIGNAL(requestShow(const ClipboardBrowser*)),
             this, SLOT(showBrowser(const ClipboardBrowser*)) );
    connect( c, SIGNAL(requestHide()),
             this, SLOT(close()) );
    connect( c, SIGNAL(doubleClicked(QModelIndex)),
             this, SLOT(activateCurrentItem()) );
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
    return c;
}

bool MainWindow::isTrayMenuVisible() const
{
    return m_trayMenu->isVisible();
}

void MainWindow::setSessionName(const QString &sessionName)
{
    m_sessionName = sessionName;
    updateIcon();
}

WId MainWindow::mainWinId() const
{
    return winId();
}

WId MainWindow::trayMenuWinId() const
{
    return m_trayMenu->winId();
}

void MainWindow::showMessage(const QString &title, const QString &msg,
                             QSystemTrayIcon::MessageIcon icon, int msec, int notificationId)
{
    QColor color = notificationDaemon()->foreground();
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

void MainWindow::showClipboardMessage(const ClipboardItem *item)
{
    if ( m_options->itemPopupInterval != 0 && m_options->clipboardNotificationLines > 0) {
        const QColor color = notificationDaemon()->foreground();
        const QPixmap icon =
                ConfigurationManager::instance()->iconFactory()->createPixmap(IconPaste, color, 16);
        notificationDaemon()->create( item->data(), m_options->clipboardNotificationLines, icon,
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

    if (event->key() == Qt::Key_Alt) {
        if (m_options->hideTabs)
            setHideTabs(false);
        if (m_options->hideMenuBar)
            setHideMenuBar(false);
    }

    if (m_browsemode && m_options->viMode) {
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

        case Qt::Key_Backspace:
            resetStatus();
            c->setCurrent(0);
            break;

        case Qt::Key_Escape:
            if ( ui->searchBar->isHidden() ) {
                close();
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
    if (event->key() == Qt::Key_Alt) {
        if (m_options->hideTabs)
            setHideTabs(true);
        if (m_options->hideMenuBar) {
            setHideMenuBar(true);
            enterBrowseMode(m_browsemode);
        }
    }
    QMainWindow::keyReleaseEvent(event);
}

bool MainWindow::event(QEvent *event)
{
    if (event->type() == QEvent::Enter) {
        updateFocusWindows();
        updateWindowTransparency(true);
    } else if (event->type() == QEvent::Leave) {
        updateWindowTransparency(false);

        setHideTabs(m_options->hideTabs);
        setHideMenuBar(m_options->hideMenuBar);
    } else if (event->type() == QEvent::WindowActivate) {
        if ( m_timerMiminizing != NULL && m_timerMiminizing->isActive() ) {
            // Window manager ignores window minimizing -- hide it instead.
            m_minimizeUnsupported = true;
            hide();
            return true;
        }

        updateWindowTransparency();

        // Update highligh color of show/hide widget for menu bar.
        setHideMenuBar(m_options->hideMenuBar);
    } else if (event->type() == QEvent::WindowDeactivate) {
        m_lastWindow = 0;
        m_pasteWindow = 0;
        updateWindowTransparency();

        setHideTabs(m_options->hideTabs);
        setHideMenuBar(m_options->hideMenuBar);
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
        getBrowser()->clearFilter();
    }
    enterBrowseMode();
}

void MainWindow::loadSettings()
{
    log( tr("Loading configuration") );

    ConfigurationManager *cm = ConfigurationManager::instance();
    m_options->confirmExit = cm->value("confirm_exit").toBool();

    // update menu items and icons
    createMenu();

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

    m_options->hideTabs = cm->value("hide_tabs").toBool();
    setHideTabs(m_options->hideTabs);

    // Don't override menu bar style if unnecessary.
    if ( m_options->hideMenuBar != cm->value("hide_menu_bar").toBool() ) {
        m_options->hideMenuBar = !m_options->hideMenuBar;
        if (m_options->hideMenuBar) {
            setHideMenuBar(true);
        } else {
            ui->widgetShowMenuBar->removeEventFilter(this);
            ui->widgetShowMenuBar->hide();
            menuBar()->setStyleSheet(QString());
        }
    }

    saveCollapsedTabs();

    // tab bar position
    int tabPosition = cm->value("tab_position").toInt();
    ui->tabWidget->setTabPosition(
          tabPosition == 0 ? QBoxLayout::BottomToTop
        : tabPosition == 1 ? QBoxLayout::TopToBottom
        : tabPosition == 2 ? QBoxLayout::RightToLeft
        : tabPosition == 3 ? QBoxLayout::LeftToRight
        : tabPosition == 4 ? QBoxLayout::RightToLeft
                           : QBoxLayout::LeftToRight);
    ui->tabWidget->setTreeModeEnabled(tabPosition > 3);
    cm->tabAppearance()->decorateTabs(ui->tabWidget);

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
        }
        showMinimized();
    }

    if (m_notifications != NULL)
        updateNotifications();

    updateIcon();

    ui->searchBar->loadSettings();

    log( tr("Configuration loaded") );
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

    ConfigurationManager::instance()->loadGeometry(this);
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

    createPlatformNativeInterface()->raiseWindow(winId());
}

bool MainWindow::toggleVisible()
{
    // Showing/hiding window in quick succession doesn't work well on X11.
    if ( m_timerShowWindow->isActive() )
        return false;

    if ( isVisible() && !isMinimized() ) {
        close();
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
    if (c->select(clipboardItemHash) && m_options->trayItemPaste && isValidWindow(m_trayPasteWindow)) {
        QApplication::processEvents();
        createPlatformNativeInterface()->pasteToWindow(m_trayPasteWindow);
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

    emit tabGroupSelected(currentIsTabGroup);

    if (currentIsTabGroup)
        return;

    // update item menu (necessary for keyboard shortcuts to work)
    ClipboardBrowser *c = browser();
    c->setContextMenu(m_menuItem);
    setHideMenuBar(m_options->hideMenuBar);

    c->filterItems( ui->searchBar->filter() );

    if ( current >= 0 ) {
        if( !c->currentIndex().isValid() && isVisible() ) {
            c->setCurrent(0);
        }
    }
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
    if (m_monitoringDisabled)
        return;

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
            ClipboardItem *first = c->at(0);
            const QByteArray newText = data2[mimeText].toByteArray();
            const QByteArray firstItemText = first->data(mimeText);
            if ( first->data().contains(mimeText) && (newText == firstItemText || (
                     data2.value(mimeWindowTitle) == first->data().value(mimeWindowTitle)
                     && (newText.startsWith(firstItemText) || newText.endsWith(firstItemText)))) )
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

void MainWindow::clipboardChanged(const ClipboardItem *item)
{
    QString text = textLabelForData( item->data() );
    m_tray->setToolTip( tr("Clipboard:\n%1", "Tray tooltip format").arg(text) );

    showClipboardMessage(item);

    const QString clipboardContent = textLabelForData( item->data() );
    if ( m_sessionName.isEmpty() ) {
        setWindowTitle( tr("%1 - CopyQ", "Main window title format (%1 is clipboard content label)")
                        .arg(clipboardContent) );
    } else {
        setWindowTitle( tr("%1 - %2 - CopyQ",
                           "Main window title format (%1 is clipboard content label, %2 is session name)")
                        .arg(clipboardContent)
                        .arg(m_sessionName) );
    }
}

void MainWindow::setClipboard(const ClipboardItem *item)
{
    if ( !isVisible() || isMinimized() )
        showClipboardMessage(item);
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
    if ( m_options->activateCloses() )
        close();
    if ( m_options->activateFocuses() || m_options->activatePastes() ) {
        PlatformPtr platform = createPlatformNativeInterface();
        if ( m_options->activateFocuses() && isValidWindow(lastWindow) )
            platform->raiseWindow(lastWindow);
        if ( m_options->activatePastes() && isValidWindow(pasteWindow) ) {
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

void MainWindow::pasteToCurrentWindow()
{
    PlatformPtr platform = createPlatformNativeInterface();
    platform->pasteToWindow( platform->getPasteWindow() );
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

QString MainWindow::sendKeys(const QString &keys) const
{
#ifdef HAS_TESTS
    QWidget *w = QApplication::focusWidget();
    if (!w)
        return tr("Cannot send keys, no widget is focused!");

    if (keys.startsWith(":")) {
        QTest::keyClicks(w, keys.mid(1), Qt::NoModifier, 100);
    } else {
        const QKeySequence shortcut(keys);

        if ( shortcut.isEmpty() ) {
            return tr("Cannot parse key \"%1\"!").arg(keys);
        } else {
            QTest::keyClick(w, Qt::Key(shortcut[0] & ~Qt::KeyboardModifierMask),
                            Qt::KeyboardModifiers(shortcut[0] & Qt::KeyboardModifierMask), 100);
        }
    }
    return QString();
#else
    Q_UNUSED(keys);
    return tr("This is only available if tests are compiled!");
#endif
}

ClipboardBrowser *MainWindow::getTabForTrayMenu()
{
    return m_options->trayCurrentTab ? browser()
                            : m_options->trayTabName.isEmpty() ? browser(0) : findTab(m_options->trayTabName);
}

void MainWindow::addItems(const QStringList &items, const QString &tabName)
{
    ClipboardBrowser *c = tabName.isEmpty() ? browser() : createTab(tabName);
    {
        ClipboardBrowser::Lock lock(c);
        foreach (const QString &item, items)
            c->add(item);
    }
}

void MainWindow::addItems(const QStringList &items, const QModelIndex &index)
{
    ClipboardBrowser *c = findBrowser(index);
    if (c == NULL)
        return;

    QVariantMap dataMap;
    dataMap.insert( mimeText, items.join(QString()).toLocal8Bit() );
    c->model()->setData(index, dataMap, contentType::updateData);
}

void MainWindow::addItem(const QByteArray &data, const QString &format, const QString &tabName)
{
    ClipboardBrowser *c = tabName.isEmpty() ? browser() : createTab(tabName);
    c->add( createDataMap(format, data) );
}

void MainWindow::addItem(const QByteArray &data, const QString &format, const QModelIndex &index)
{
    ClipboardBrowser *c = findBrowser(index);
    if (c == NULL)
        return;

    QVariantMap dataMap;
    if (format == mimeItems)
        deserializeData(&dataMap, data);
    else
        dataMap.insert(format, data);
    c->model()->setData(index, dataMap, contentType::updateData);
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
            hide();

        updateIcon();
    } else {
        m_trayTimer->start();
    }
}

void MainWindow::actionStarted(Action *action)
{
    // menu item
    QString text = tr("KILL") + " " + action->command();
    QString tooltip = tr("<b>COMMAND:</b>") + "<br />" + escapeHtml(text) + "<br />" +
                      tr("<b>INPUT:</b>") + "<br />" +
                      escapeHtml( QString::fromLocal8Bit(action->input()) );

    QAction *act = m_actions[action] = new QAction(this);
    act->setToolTip(tooltip);
    act->setWhatsThis(tooltip);

    connect( act, SIGNAL(triggered()),
             action, SLOT(terminate()) );

    m_menuCommand->addAction(act);
    m_menuCommand->setEnabled(true);

#ifdef Q_OS_MAC
    m_trayMenuCommand->addAction(act);
    m_trayMenuCommand->setEnabled(true);
#endif // Q_OS_MAC

    updateIcon();

    act->setText( elideText(text, act->font(), QString(), true) );
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
    if ( isActiveWindow() || (!m_options->activateFocuses() && !m_options->activatePastes()) )
        return;

    PlatformPtr platform = createPlatformNativeInterface();
    if ( m_options->activatePastes() ) {
        WId pasteWindow = platform->getPasteWindow();
        if ( isValidWindow(pasteWindow) )
            m_pasteWindow = pasteWindow;
    }
    if ( m_options->activateFocuses() ) {
        WId lastWindow = platform->getCurrentWindow();
        if ( isValidWindow(lastWindow) )
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

    if(m_browsemode){
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
    PlatformPtr platform = createPlatformNativeInterface();
    m_trayPasteWindow = platform->getPasteWindow();

    ClipboardBrowser *c = getTabForTrayMenu();

    m_trayMenu->clearClipboardItemActions();
    m_trayMenu->clearCustomActions();

    // Add items.
    const int len = (c != NULL) ? qMin( m_options->trayItems, c->length() ) : 0;
    const int current = c->currentIndex().row();
    for ( int i = 0; i < len; ++i ) {
        const ClipboardItem *item = c->at(i);
        if (item != NULL)
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
    AboutDialog *aboutDialog = new AboutDialog(this);
    aboutDialog->show();
    aboutDialog->activateWindow();
}

void MainWindow::showClipboardContent()
{
    ClipboardDialog *d = new ClipboardDialog(QVariantMap(), this);
    connect( d, SIGNAL(finished(int)), d, SLOT(deleteLater()) );
    d->show();
}

ActionDialog *MainWindow::createActionDialog()
{
    ActionDialog *actionDialog = new ActionDialog(this);
    actionDialog->setOutputTabs(ui->tabWidget->tabs(), QString());

    connect( actionDialog, SIGNAL(accepted(Action*)),
             this, SLOT(action(Action*)) );
    connect( actionDialog, SIGNAL(finished(int)),
             actionDialog, SLOT(deleteLater()) );

    return actionDialog;
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

    emit changeClipboard(&item);
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

    if ( !saveTab(fileName, tab_index) ) {
        QMessageBox::critical( this, tr("CopyQ Error Saving File"),
                               tr("Cannot save file %1!")
                               .arg(quoteString(fileName)) );
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

void MainWindow::action(const QVariantMap &data, const Command &cmd, const QModelIndex &outputIndex)
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

void MainWindow::newTab(const QString &name)
{
    TabDialog *d = new TabDialog(TabDialog::TabNew, this);
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
    connect( d, SIGNAL(finished(int)),
             d, SLOT(deleteLater()) );

    d->open();
}

void MainWindow::renameTabGroup(const QString &name)
{
    TabDialog *d = new TabDialog(TabDialog::TabGroupRename, this);

    d->setTabs(ui->tabWidget->tabs());
    d->setTabGroupName(name);

    connect( d, SIGNAL(accepted(QString, QString)),
             this, SLOT(renameTabGroup(QString, QString)) );
    connect( d, SIGNAL(finished(int)),
             d, SLOT(deleteLater()) );

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
    int i = tab >= 0 ? tab : ui->tabWidget->currentIndex();
    if (i < 0)
        return;

    d->setTabIndex(i);
    d->setTabs(ui->tabWidget->tabs());
    d->setTabName(browser(i)->tabName());

    connect( d, SIGNAL(accepted(QString, int)),
             this, SLOT(renameTab(QString, int)) );
    connect( d, SIGNAL(finished(int)),
             d, SLOT(deleteLater()) );

    d->open();
}

void MainWindow::renameTab(const QString &name, int tabIndex)
{
    if ( name.isEmpty() || ui->tabWidget->tabs().contains(name) )
        return;

    ClipboardBrowser *c = browser(tabIndex);

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
    saveCollapsedTabs();
    ConfigurationManager::instance()->disconnect();
    m_tray->hide();
    delete ui;
}
