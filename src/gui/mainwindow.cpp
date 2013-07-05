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
#include "gui/configtabappearance.h"
#include "gui/iconfactory.h"
#include "gui/tabdialog.h"
#include "gui/tabwidget.h"
#include "gui/traymenu.h"
#include "item/clipboarditem.h"
#include "item/clipboardmodel.h"
#include "platform/platformnativeinterface.h"

#include <QAction>
#include <QBitmap>
#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QTimer>
#include <QPainter>

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
const QIcon iconReverse() { return getIcon("view-sort-descending", IconSortByAlphabetAlt); }
const QIcon iconSave() { return getIcon("document-save", IconSave); }
const QIcon iconSort() { return getIcon("view-sort-ascending", IconSortByAlphabet); }
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

QString textLabelForData(const QMimeData *data, int maxChars)
{
    const QStringList formats = data->formats();

    if ( formats.indexOf("text/plain") != -1 )
        return MainWindow::tr("\"%1\"").arg( elideText(data->text(), maxChars) );
    else if ( formats.indexOf(QRegExp("^image/.*")) != -1 )
        return MainWindow::tr("<IMAGE>");
    else if ( formats.indexOf(QString("text/uri-list")) != -1 )
        return MainWindow::tr("<FILES>");
    else if ( formats.isEmpty() || (formats.size() == 1 && formats[0] == mimeWindowTitle) )
        return MainWindow::tr("<EMPTY>");
    return MainWindow::tr("<DATA>");
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
    , m_clearFirstTab(false)
    , m_actions()
    , m_sharedData(new ClipboardBrowserShared)
    , m_trayItemPaste(true)
    , m_trayPasteWindow()
    , m_pasteWindow()
    , m_lastWindow()
    , m_timerUpdateFocusWindows( new QTimer(this) )
    , m_timerGeometry( new QTimer(this) )
    , m_sessionName()
{
    ui->setupUi(this);

    updateIcon();

    // signals & slots
    connect( tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
             this, SLOT(trayActivated(QSystemTrayIcon::ActivationReason)) );
    connect( trayMenu, SIGNAL(aboutToShow()),
             this, SLOT(updateTrayMenuItems()) );
    connect( trayMenu, SIGNAL(clipboardItemActionTriggered(uint)),
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
    connect( m_timerSearch, SIGNAL(timeout()),
             this, SLOT(onTimerSearch()) );
    connect( ui->searchBar, SIGNAL(textChanged(QString)),
             m_timerSearch, SLOT(start()) );
    connect( m_timerUpdateFocusWindows, SIGNAL(timeout()),
             this, SLOT(updateFocusWindows()) );
    connect( this, SIGNAL(changeClipboard(const ClipboardItem*)),
             this, SLOT(clipboardChanged(const ClipboardItem*)) );

    // settings
    loadSettings();

    ui->tabWidget->setCurrentIndex(0);

    // search timer
    m_timerSearch->setSingleShot(true);
    m_timerSearch->setInterval(200);

    m_timerUpdateFocusWindows->setSingleShot(true);
    m_timerUpdateFocusWindows->setInterval(50);

    m_timerGeometry->setSingleShot(true);
    m_timerGeometry->setInterval(250);

    // notify window if configuration changes
    ConfigurationManager *cm = ConfigurationManager::instance();
    connect( cm, SIGNAL(configurationChanged()),
             this, SLOT(loadSettings()) );

    // browse mode by default
    enterBrowseMode();

    tray->show();

    tray->setContextMenu(trayMenu);

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
    hide();

    if ( aboutDialog && !aboutDialog->isHidden() ) {
        aboutDialog->close();
    }

    event->ignore();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (!m_timerGeometry->isActive())
        ConfigurationManager::instance()->saveGeometry(this);
}

void MainWindow::moveEvent(QMoveEvent *event)
{
    QMainWindow::moveEvent(event);
    if (!m_timerGeometry->isActive())
        ConfigurationManager::instance()->saveGeometry(this);
}

void MainWindow::createMenu()
{
    QMenuBar *menubar = menuBar();
    QMenu *menu;
    QAction *act;

    menubar->clear();
    trayMenu->clear();

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
    connect(this, SIGNAL(tabGroupSelected(bool)),
            act, SLOT(setDisabled(bool)) );

    act = menu->addAction( act->icon(), act->text(),
                           this, SLOT(editNewItem()),
                           QKeySequence::New );
    connect(this, SIGNAL(tabGroupSelected(bool)),
            act, SLOT(setDisabled(bool)) );

    // - import tab
    menu->addAction( iconOpen(), tr("&Import Tab..."),
                     this, SLOT(loadTab()),
                     QKeySequence(tr("Ctrl+I")) );

    // - export tab
    act = menu->addAction( iconSave(), tr("&Export Tab..."),
                           this, SLOT(saveTab()),
                           QKeySequence::Save );
    connect(this, SIGNAL(tabGroupSelected(bool)),
            act, SLOT(setDisabled(bool)) );

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
    menu->addAction( iconClipboard(),
                     tr("Show &Clipboard Content"),
                     this, SLOT(showClipboardContent()),
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
    connect(this, SIGNAL(tabGroupSelected(bool)),
            menu, SLOT(setDisabled(bool)) );

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
    itemMenu = menubar->addMenu( tr("&Item") );
    connect(this, SIGNAL(tabGroupSelected(bool)),
            itemMenu, SLOT(setDisabled(bool)) );

    // Tabs
    menu = menubar->addMenu(tr("&Tabs"));

    // add tab
    menu->addAction( iconTabNew(), tr("&New tab"),
                     this, SLOT(newTab()),
                     QKeySequence(tr("Ctrl+T")) );

    act = menu->addAction( iconTabRename(), tr("Re&name tab"),
                           this, SLOT(renameTab()),
                           QKeySequence(tr("Ctrl+F2")) );
    connect(this, SIGNAL(tabGroupSelected(bool)),
            act, SLOT(setDisabled(bool)) );

    act = menu->addAction( iconTabRemove(), tr("Re&move tab"),
                           this, SLOT(removeTab()),
                           QKeySequence(tr("Ctrl+W")) );
    connect(this, SIGNAL(tabGroupSelected(bool)),
            act, SLOT(setDisabled(bool)) );

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

void MainWindow::popupTabBarMenu(const QPoint &pos, const QString &tab)
{
    QMenu menu(this);

    const int tabIndex = findBrowserIndex(tab);
    bool hasTab = tabIndex != -1;
    bool isGroup = ui->tabWidget->isTabGroup(tab);

    QAction *actNew = menu.addAction( iconTabNew(), tr("&New tab") );
    QAction *actRenameGroup =
            isGroup ? menu.addAction( iconTabRename(), tr("Rename &group \"%1\"").arg(tab) ) : NULL;
    QAction *actRename =
            hasTab ? menu.addAction( iconTabRename(), tr("Re&name tab \"%1\"").arg(tab) ) : NULL;
    QAction *actRemove =
            hasTab ? menu.addAction( iconTabRemove(), tr("Re&move tab \"%1\"").arg(tab) ) : NULL;
    QAction *actRemoveGroup =
            isGroup ? menu.addAction( iconTabRename(), tr("Remove group \"%1\"").arg(tab) ) : NULL;

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

    if ( !m_sessionName.isEmpty() ) {
        QPixmap pix = icon.pixmap( tray->geometry().size() );
        colorizePixmap( &pix, QColor(0x7f, 0xca, 0x9b), sessionNameToColor(m_sessionName) );
        icon = pix;
    }

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
        if (c->getID() == id)
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

    menuBar()->setNativeMenuBar(!m_hideMenuBar);

    // Hiding menu bar completely disables shortcuts for child QAction.
    menuBar()->setStyleSheet(hide ? "QMenuBar{height:0}" : "");
}

void MainWindow::updateTabsAutoSaving()
{
    getBrowser(0)->setSavingEnabled(!m_clearFirstTab);

    TabWidget *tabs = ui->tabWidget;
    for ( int i = 1; i < tabs->count(); ++i )
        getBrowser(i)->setSavingEnabled(true);
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
    /* check name */
    int i = findTabIndex(name);
    if (i != -1)
        return getBrowser(i);

    ClipboardBrowser *c = new ClipboardBrowser(this, m_sharedData);
    c->setID(name);
    c->loadSettings();
    c->setAutoUpdate(true);

    // Preload items only in the first tab.
    bool firstTab = ui->tabWidget->count() == 0;
    if (firstTab)
        c->loadItems();

    connect( c, SIGNAL(changeClipboard(const ClipboardItem*)),
             this, SLOT(onChangeClipboardRequest(const ClipboardItem*)) );
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

    ConfigurationManager::instance()->setTabs( ui->tabWidget->tabs() );

    if (firstTab)
        tabChanged(0, -1);

    return c;
}

bool MainWindow::isTrayMenuVisible() const
{
    return trayMenu->isVisible();
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

    if (c->editing())
        return;

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
            if ( ui->widgetFind->isHidden() )
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
    cm->setTabs(ui->tabWidget->tabs());
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
    ui->tabWidget->clear();
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

    /* are tabs already loaded? */
    QStringList tabs = cm->value("tabs").toStringList();
    foreach (const QString &name, tabs) {
        ClipboardBrowser *c;
        c = createTab(name);
        c->loadSettings();
    }

    if ( ui->tabWidget->count() == 0 )
        addTab( tr("&clipboard") );

    m_clearFirstTab = cm->value("clear_first_tab").toBool();
    updateTabsAutoSaving();

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

    trayMenu->setStyleSheet( cm->tabAppearance()->getToolTipStyleSheet() );

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

    // Don't save geometry after window shown.
    m_timerGeometry->start();

    ConfigurationManager::instance()->loadGeometry(this);

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

    QApplication::processEvents();
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
    if (c->select(clipboardItemHash) && m_trayItemPaste && isForeignWindow(m_trayPasteWindow)) {
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
    trayMenu->toggle();
    return trayMenu->isVisible();
}

void MainWindow::tabChanged(int current, int previous)
{
    if (previous != -1) {
        ClipboardBrowser *before = browser(previous);
        if (before != NULL)
            before->setContextMenu(NULL);
    }

    bool currentIsTabGroup = current == -1;

    itemMenu->clear();

    emit tabGroupSelected(currentIsTabGroup);

    if (currentIsTabGroup)
        return;

    // update item menu (necessary for keyboard shortcuts to work)
    ClipboardBrowser *c = browser();
    c->setContextMenu(itemMenu);
    setHideMenuBar(m_hideMenuBar);

    c->filterItems( ui->searchBar->text() );

    if ( current >= 0 ) {
        if( !c->currentIndex().isValid() && isVisible() ) {
            c->setCurrent(0);
        }
    }

    m_lastTab = current;
}

void MainWindow::tabMoved(int, int)
{
    ConfigurationManager *cm = ConfigurationManager::instance();
    cm->setTabs(ui->tabWidget->tabs());
    cm->saveSettings();
    updateTabsAutoSaving();
}

void MainWindow::tabMoved(const QString &oldPrefix, const QString &newPrefix, const QString &afterPrefix)
{
    QStringList tabs;
    TabWidget *w = ui->tabWidget;
    for( int i = 0; i < w->count(); ++i )
        tabs << getBrowser(i)->getID();

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

    ConfigurationManager *cm = ConfigurationManager::instance();

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
                        newName = tr("%1 (%2)", "Format for renaming items")
                                .arg(oldName)
                                .arg(++num);
                    } while ( tabs.contains(newName) );

                    w->setTabText(from, newName);
                }

                ClipboardBrowser *c = browser(from);
                c->setID(newName);
                c->saveItems();
                cm->removeItems(tab);
            }

            // Move tab.
            const int to = afterIndex + (down ? 0 : d + 1);
            w->moveTab(from, to);
            ++d;
        }
    }

    cm->setTabs(w->tabs());
}

void MainWindow::tabMenuRequested(const QPoint &pos, int tab)
{
    ClipboardBrowser *c = getBrowser(tab);
    if (c == NULL)
        return;
    const QString tabName = c->getID();
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

void MainWindow::addToTab(const QMimeData *data, const QString &tabName, bool moveExistingToTop)
{
    if (m_monitoringDisabled)
        return;

    ClipboardBrowser *c = tabName.isEmpty() ? browser(0) : findBrowser(tabName);

    if ( c == NULL && !tabName.isEmpty() )
        c = createTab(tabName);

    if (c == NULL)
        return;

    c->loadItems();

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
    QString text = textLabelForData(item->data(), 256);
    tray->setToolTip( tr("Clipboard:\n%1", "Tray tooltip format").arg(text) );

    const QString clipboardContent = elideText(text, 30);
    if ( m_sessionName.isEmpty() )
        setWindowTitle( tr("%1 - CopyQ").arg(clipboardContent) );
    else
        setWindowTitle( tr("%1 - %2 - CopyQ").arg(clipboardContent).arg(m_sessionName) );
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

void MainWindow::pasteToCurrentWindow()
{
    PlatformPtr platform = createPlatformNativeInterface();
    platform->pasteToWindow( platform->getPasteWindow() );
}

ClipboardBrowser *MainWindow::getTabForTrayMenu()
{
    return m_trayCurrentTab ? browser()
                            : m_trayTabName.isEmpty() ? browser(0) : findTab(m_trayTabName);
}

void MainWindow::addItems(const QStringList &items, const QString &tabName)
{
    ClipboardBrowser *c = tabName.isEmpty() ? browser() : createTab(tabName);
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
    ClipboardBrowser *c = tabName.isEmpty() ? browser() : createTab(tabName);
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

    if(m_browsemode){
        // browse mode
        browser()->setFocus();
        if ( ui->searchBar->text().isEmpty() )
            ui->widgetFind->hide();
    } else {
        // search mode
        ui->widgetFind->show();
        ui->searchBar->setFocus(Qt::ShortcutFocusReason);
    }
}

void MainWindow::updateTrayMenuItems()
{
    PlatformPtr platform = createPlatformNativeInterface();
    m_trayPasteWindow = platform->getPasteWindow();

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
        QAction *act = trayMenu->addAction( iconClipboard(),
                                            textLabelForData(data, 128),
                                            this, SLOT(showClipboardContent()) );
        act->setText( tr("&Clipboard: %1", "Tray menu clipboard item format").arg(act->text()) );
        elideText(act, false);
        trayMenu->addCustomAction(act);

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
    QApplication::processEvents();
    WId wid = actionDialog->winId();
    createPlatformNativeInterface()->raiseWindow(wid);
    return wid;
}

void MainWindow::openPreferences()
{
    if ( !isEnabled() )
        return;

    // Turn off "always on top" so that configuration dialog is not below main window.
    Qt::WindowFlags flags = windowFlags();
    setWindowFlags(flags & ~Qt::WindowStaysOnTopHint);

    if ( ConfigurationManager::instance()->exec() == QDialog::Rejected )
        setWindowFlags(flags);
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
    QStringList existingTabs = ui->tabWidget->tabs();
    int i = 0;
    while ( existingTabs.contains(tabName) ) {
        log(tabName);
        tabName = baseTabName + " (" + QString::number(++i) + ')';
    }

    ClipboardBrowser *c = createTab(tabName);
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
    d->setTabName(browser(i)->getID());

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

    TabWidget *w = ui->tabWidget;
    ClipboardBrowser *c = browser(tabIndex);
    QString oldName = c->getID();

    c->setID(name);
    c->saveItems();
    w->setTabText(tabIndex, name);

    ConfigurationManager *cm = ConfigurationManager::instance();
    cm->removeItems(oldName);
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
                ui->tabWidget->setCurrentIndex(0);
            c->purgeItems();
            c->deleteLater();
            w->removeTab(tabIndex);
            ConfigurationManager::instance()->setTabs( ui->tabWidget->tabs() );
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
