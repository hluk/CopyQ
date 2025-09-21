// SPDX-License-Identifier: GPL-3.0-or-later

#include "tabwidget.h"
#include "tabbar.h"
#include "tabtree.h"

#include "common/config.h"

#include <QAction>
#include <QBoxLayout>
#include <QEvent>
#include <QMainWindow>
#include <QMimeData>
#include <QPoint>
#include <QSettings>
#include <QStackedWidget>
#include <QToolBar>

namespace {

QString getTabWidgetConfigurationFilePath()
{
    return getConfigurationFilePath("_tabs.ini");
}

template <typename Receiver, typename Slot>
void addTabAction(QWidget *widget, const QKeySequence &shortcut,
                  Receiver *receiver, Slot slot,
                  Qt::ShortcutContext context = Qt::WindowShortcut)
{
    auto act = new QAction(widget);
    act->setShortcut(shortcut);
    act->setShortcutContext(context);
    QObject::connect(act, &QAction::triggered, receiver, slot);
    widget->addAction(act);
}

} // namespace

TabWidget::TabWidget(QWidget *parent)
    : QWidget(parent)
    , m_toolBar(new QToolBar(this))
    , m_toolBarTree(new QToolBar(this))
    , m_stackedWidget(nullptr)
    , m_hideTabBar(false)
    , m_showTabItemCount(false)
{
    // Set object name for tool bars so they can be saved with QMainWindow::saveState().
    m_toolBar->setObjectName("toolBarTabBar");
    m_toolBarTree->setObjectName("toolBarTabTree");

    m_toolBar->setContextMenuPolicy(Qt::NoContextMenu);
    m_toolBarTree->setContextMenuPolicy(Qt::NoContextMenu);

    m_toolBar->installEventFilter(this);
    connect( m_toolBar, &QToolBar::orientationChanged,
             this, &TabWidget::onToolBarOrientationChanged );

    auto layout = new QBoxLayout(QBoxLayout::TopToBottom, this);
    setLayout(layout);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_stackedWidget = new QStackedWidget(this);
    layout->addWidget(m_stackedWidget);

    createTabBar();

    addTabAction(this, QKeySequence::PreviousChild, this, &TabWidget::previousTab);
    addTabAction(this, QKeySequence::NextChild, this, &TabWidget::nextTab);
}

QString TabWidget::getCurrentTabPath() const
{
    return m_tabs->getCurrentTabPath();
}

bool TabWidget::isTabGroup(const QString &tab) const
{
    return m_tabs->isTabGroup(tab);
}

bool TabWidget::isTabGroupSelected() const
{
    QWidget *w = currentWidget();
    return w != nullptr && w->isHidden();
}

int TabWidget::currentIndex() const
{
    return m_stackedWidget->currentIndex();
}

QWidget *TabWidget::widget(int tabIndex) const
{
    return m_stackedWidget->widget(tabIndex);
}

int TabWidget::count() const
{
    return m_stackedWidget->count();
}

QString TabWidget::tabName(int tabIndex) const
{
    return m_tabs->tabName(tabIndex);
}

int TabWidget::tabIndex(const QString &tabName) const
{
    for (int i = 0; i < count(); ++i) {
        if ( tabName == this->tabName(i) )
            return i;
    }
    return -1;
}

void TabWidget::setTabName(int tabIndex, const QString &tabName)
{
    m_tabs->setTabName(tabIndex, tabName);
    m_tabs->adjustSize();
}

void TabWidget::setTabItemCountVisible(bool visible)
{
    m_showTabItemCount = visible;
}

void TabWidget::setTabIcon(const QString &tabName, const QString &icon)
{
    m_tabs->setTabIcon(tabName, icon);
}

void TabWidget::insertTab(int tabIndex, QWidget *widget, const QString &tabName)
{
    const bool firstTab = count() == 0;
    m_stackedWidget->insertWidget(tabIndex, widget);

    m_tabs->insertTab(tabIndex, tabName);
    m_toolBarCurrent->layout()->setSizeConstraint(QLayout::SetMinAndMaxSize);

    if (firstTab)
        emit currentChanged(0, -1);

    updateTabItemCount(tabName);
    updateToolBar();
}

void TabWidget::removeTab(int tabIndex)
{
    if (tabIndex == currentIndex())
        setCurrentIndex(tabIndex == 0 ? 1 : 0);

    // Item count must be updated If tab is removed but tab group remains.
    const QString tabName = this->tabName(tabIndex);
    m_tabs->setTabItemCount(tabName, QString());

    QWidget *w = m_stackedWidget->widget(tabIndex);
    m_tabItemCounters.remove(w);
    m_stackedWidget->removeWidget(w);
    delete w;

    m_tabs->removeTab(tabIndex);

    updateToolBar();
}

QStringList TabWidget::tabs() const
{
    QStringList tabs;
    tabs.reserve( count() );
    for( int i = 0; i < count(); ++i )
        tabs.append( tabName(i) );

    return tabs;
}

void TabWidget::addToolBars(QMainWindow *mainWindow)
{
    mainWindow->addToolBar(Qt::TopToolBarArea, m_toolBar);
    mainWindow->addToolBar(Qt::LeftToolBarArea, m_toolBarTree);
}

void TabWidget::saveTabInfo()
{
    QSettings settings(getTabWidgetConfigurationFilePath(), QSettings::IniFormat);

    m_tabs->updateCollapsedTabs(&m_collapsedTabs);
    settings.setValue("TabWidget/collapsed_tabs", m_collapsedTabs);

    QVariantMap tabItemCounters;
    for (int i = 0; i < count(); ++i) {
        const int count = m_tabItemCounters.value(widget(i), -1);
        if (count >= 0)
            tabItemCounters[tabName(i)] = count;
    }
    settings.setValue("TabWidget/tab_item_counters", tabItemCounters);
}

void TabWidget::loadTabInfo()
{
    QSettings settings(getTabWidgetConfigurationFilePath(), QSettings::IniFormat);

    m_collapsedTabs = settings.value("TabWidget/collapsed_tabs").toStringList();

    const QVariantMap tabItemCounters = settings.value("TabWidget/tab_item_counters").toMap();
    for (int i = 0; i < count(); ++i) {
        const QVariant count = tabItemCounters.value(tabName(i));
        if (count.isValid())
            m_tabItemCounters[widget(i)] = count.toInt();
    }
}

void TabWidget::updateTabs(QSettings &settings)
{
    loadTabInfo();

    for (auto it = m_tabItemCounters.constBegin(); it != m_tabItemCounters.constEnd(); ++it) {
        QWidget *w = it.key();
        const int i = m_stackedWidget->indexOf(w);
        if (i != -1)
            m_tabs->setTabItemCount(m_tabs->tabName(i), itemCountLabel(m_tabs->tabName(i)));
    }

    m_tabs->setCollapsedTabs(m_collapsedTabs);

    QHash<QString, QString> tabIcons;
    const int size = settings.beginReadArray(QStringLiteral("Tabs"));
    for(int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        const QString name = settings.value(QStringLiteral("name")).toString();
        const QString icon = settings.value(QStringLiteral("icon")).toString();
        tabIcons[name] = icon;
    }
    settings.endArray();
    m_tabs->updateTabIcons(tabIcons);

    m_tabs->adjustSize();
}

void TabWidget::setCurrentIndex(int tabIndex)
{
    if (m_ignoreCurrentTabChanges)
        return;

    QWidget *w = currentWidget();
    const int current = isTabGroupSelected() ? -1 : currentIndex();

    if (tabIndex == current)
        return;

    if (tabIndex != -1) {
        m_stackedWidget->setCurrentIndex(tabIndex);

        w = currentWidget();
        if (w == nullptr)
            return;

        w->show();
        if (m_toolBarCurrent->hasFocus())
            w->setFocus();

        m_tabs->setCurrentTab(tabIndex);
    } else if (w != nullptr) {
        if (w->hasFocus())
            m_toolBarCurrent->setFocus();
        w->hide();
    }

    emit currentChanged(tabIndex, current);
}

void TabWidget::nextTab()
{
    m_tabs->nextTab();
}

void TabWidget::previousTab()
{
    m_tabs->previousTab();
}

void TabWidget::setTabBarHidden(bool hidden)
{
    m_hideTabBar = hidden;
    updateToolBar();
}

void TabWidget::setTreeModeEnabled(bool enabled)
{
    setTreeModeEnabled(enabled, this->tabs());
}

void TabWidget::setTabItemCount(const QString &tabName, int itemCount)
{
    QWidget *w = m_stackedWidget->widget(tabIndex(tabName));
    if ( m_tabItemCounters.value(w, -1) == itemCount)
        return;

    m_tabItemCounters[w] = itemCount;

    updateTabItemCount(tabName);
}

void TabWidget::setTabsOrder(const QStringList &tabs)
{
    QStringList currentTabs = this->tabs();
    if (tabs == currentTabs)
        return;

    m_ignoreCurrentTabChanges = true;

    for (int i = 0; i < tabs.size(); ++i) {
        const int tabIndex = currentTabs.indexOf(tabs[i]);
        if (tabIndex != -1 && tabIndex != i) {
            QWidget *widget = m_stackedWidget->widget(tabIndex);
            m_stackedWidget->removeWidget(widget);
            m_stackedWidget->insertWidget(i, widget);
            currentTabs.move(tabIndex, i);
        }
    }

    m_stackedWidget->setCurrentIndex( currentIndex() );
    setTreeModeEnabled(m_toolBarCurrent == m_toolBarTree, currentTabs);

    m_ignoreCurrentTabChanges = false;
}

bool TabWidget::eventFilter(QObject *, QEvent *event)
{
    if (event->type() == QEvent::Move)
        updateToolBar();

    return false;
}

void TabWidget::onTabMoved(int from, int to)
{
    m_stackedWidget->insertWidget(to, m_stackedWidget->widget(from));
    emit tabMoved(from, to);
}

void TabWidget::onTabsMoved(const QString &oldPrefix, const QString &newPrefix, const QList<int> &indexes)
{
    const int newCurrentIndex = indexes.indexOf(currentIndex());
    Q_ASSERT(newCurrentIndex != -1);

    m_stackedWidget->hide();

    QVector<QWidget*> widgets;
    widgets.reserve(m_stackedWidget->count());

    while ( m_stackedWidget->count() > 0 ) {
        QWidget *w = m_stackedWidget->widget(0);
        widgets.append(w);
        m_stackedWidget->removeWidget(w);
    }

    for (int index : indexes) {
        Q_ASSERT(index >= 0);
        Q_ASSERT(index < widgets.count());
        m_stackedWidget->insertWidget(-1, widgets[index]);
    }

    m_stackedWidget->setCurrentIndex(newCurrentIndex);

    m_stackedWidget->show();

    saveTabInfo();

    emit tabsMoved(oldPrefix, newPrefix);
}

void TabWidget::onToolBarOrientationChanged(Qt::Orientation orientation)
{
    if (m_tabBar) {
        if (orientation == Qt::Vertical)
            m_tabBar->setShape(QTabBar::RoundedWest);
        else
            m_tabBar->setShape(QTabBar::RoundedNorth);
    }
}

void TabWidget::onTreeItemClicked()
{
    auto w = currentWidget();
    if (w)
        w->setFocus(Qt::MouseFocusReason);
}

void TabWidget::createTabBar()
{
    auto tabBar = new TabBar(this);

    tabBar->setObjectName("tab_bar");

    tabBar->setExpanding(false);
    tabBar->setMovable(true);

    connect( tabBar, &TabBar::tabBarMenuRequested,
             this, &TabWidget::tabBarMenuRequested );
    connect( tabBar, &TabBar::tabRenamed,
             this, &TabWidget::tabRenamed );
    connect( tabBar, &QTabBar::tabCloseRequested,
             this, &TabWidget::tabCloseRequested );
    connect( tabBar, &TabBar::dropItems,
             this, &TabWidget::dropItems );
    connect( tabBar, &QTabBar::currentChanged,
             this, &TabWidget::setCurrentIndex );
    connect( tabBar, &QTabBar::tabMoved,
             this, &TabWidget::onTabMoved );

    delete m_tabs;
    m_tabs = tabBar;
    m_tabBar = tabBar;

    m_toolBarCurrent = m_toolBar;
    m_toolBarCurrent->addWidget(tabBar);
}

void TabWidget::createTabTree()
{
    auto tabTree = new TabTree(this);
    tabTree->setObjectName("tab_tree");

    connect( tabTree, &TabTree::tabTreeMenuRequested,
             this, &TabWidget::tabTreeMenuRequested );
    connect( tabTree, &TabTree::tabsMoved,
             this, &TabWidget::onTabsMoved );
    connect( tabTree, &TabTree::dropItems,
             this, &TabWidget::dropItems );
    connect( tabTree, &TabTree::currentTabChanged,
             this, &TabWidget::setCurrentIndex );
    connect( tabTree, &QTreeWidget::itemClicked,
             this, &TabWidget::onTreeItemClicked );

    delete m_tabs;
    m_tabs = tabTree;
    m_tabBar = nullptr;

    m_toolBarCurrent = m_toolBarTree;
    m_toolBarCurrent->addWidget(tabTree);
}

void TabWidget::updateToolBar()
{
    bool forceHide = count() == 1;
    m_toolBar->setVisible(!forceHide && !m_hideTabBar && m_toolBarCurrent == m_toolBar);
    m_toolBarTree->setVisible(!forceHide && !m_hideTabBar && m_toolBarCurrent == m_toolBarTree);

    if (m_tabBar) {
        QMainWindow *mainWindow = qobject_cast<QMainWindow*>(m_toolBar->window());
        if (mainWindow) {
            Qt::ToolBarArea area = mainWindow->toolBarArea(m_toolBar);
            if (area == Qt::LeftToolBarArea)
                m_tabBar->setShape(QTabBar::RoundedWest);
            else if (area == Qt::RightToolBarArea)
                m_tabBar->setShape(QTabBar::RoundedEast);
            else if (area == Qt::TopToolBarArea)
                m_tabBar->setShape(QTabBar::RoundedNorth);
            else if (area == Qt::BottomToolBarArea)
                m_tabBar->setShape(QTabBar::RoundedSouth);
        }
    }

    m_tabs->adjustSize();
}

void TabWidget::updateTabItemCount(const QString &name)
{
    m_tabs->setTabItemCount(name, itemCountLabel(name));
    m_tabs->adjustSize();
}

QString TabWidget::itemCountLabel(const QString &name)
{
    if (!m_showTabItemCount)
        return QString();

    QWidget *w = m_stackedWidget->widget(tabIndex(name));
    const int count = m_tabItemCounters.value(w, -1);
    return count >= 0 ? QString::number(count) : QLatin1String("?");
}

void TabWidget::setTreeModeEnabled(bool enabled, const QStringList &tabs)
{
    if (enabled)
        createTabTree();
    else
        createTabBar();

    m_ignoreCurrentTabChanges = true;
    for (int i = 0; i < tabs.size(); ++i) {
        const QString &tabName = tabs[i];
        m_tabs->insertTab(i, tabName);
    }
    m_tabs->setCurrentTab( currentIndex() );
    m_ignoreCurrentTabChanges = false;

    updateToolBar();
}
