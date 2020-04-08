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

    loadTabInfo();
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

void TabWidget::setTabName(int tabIndex, const QString &tabName)
{
    const QString oldTabName = this->tabName(tabIndex);
    if ( m_tabItemCounters.contains(oldTabName) )
        m_tabItemCounters.insert( tabName, m_tabItemCounters.take(oldTabName) );

    m_tabs->setTabName(tabIndex, tabName);

    m_tabs->adjustSize();
}

void TabWidget::setTabItemCountVisible(bool visible)
{
    m_showTabItemCount = visible;
    for (int i = 0; i < count(); ++i)
        updateTabItemCount( tabName(i) );

    m_tabs->adjustSize();
}

void TabWidget::updateTabIcon(const QString &tabName)
{
    m_tabs->updateTabIcon(tabName);
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

    const QString tabName = this->tabName(tabIndex);
    m_tabItemCounters.remove(tabName);

    // Item count must be updated If tab is removed but tab group remains.
    m_tabs->setTabItemCount(tabName, QString());

    QWidget *w = m_stackedWidget->widget(tabIndex);
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

void TabWidget::moveTab(int from, int to)
{
    Q_ASSERT(to >= 0);
    Q_ASSERT(from >= 0);
    Q_ASSERT(to < count());
    Q_ASSERT(from < count());

    m_tabs->moveTab(from, to);

    bool isCurrent = currentIndex() == from;

    m_stackedWidget->insertWidget(to, m_stackedWidget->widget(from));

    if (isCurrent)
        setCurrentIndex(to);
}

void TabWidget::addToolBars(QMainWindow *mainWindow)
{
    mainWindow->addToolBar(Qt::TopToolBarArea, m_toolBar);
    mainWindow->addToolBar(Qt::LeftToolBarArea, m_toolBarTree);
}

void TabWidget::saveTabInfo()
{
    QStringList tabs;
    tabs.reserve( count() );
    for ( int i = 0; i < count(); ++i )
        tabs.append( tabName(i) );

    QSettings settings(getTabWidgetConfigurationFilePath(), QSettings::IniFormat);

    m_tabs->updateCollapsedTabs(&m_collapsedTabs);
    settings.setValue("TabWidget/collapsed_tabs", m_collapsedTabs);

    QVariantMap tabItemCounters;
    for (const auto &key : tabs) {
        const int count = m_tabItemCounters.value(key, -1);
        if (count >= 0)
            tabItemCounters[key] = count;
    }
    settings.setValue("TabWidget/tab_item_counters", tabItemCounters);
}

void TabWidget::loadTabInfo()
{
    QSettings settings(getTabWidgetConfigurationFilePath(), QSettings::IniFormat);

    m_collapsedTabs = settings.value("TabWidget/collapsed_tabs").toStringList();

    QVariantMap tabItemCounters = settings.value("TabWidget/tab_item_counters").toMap();
    m_tabItemCounters.clear();
    for (auto it = tabItemCounters.constBegin(); it != tabItemCounters.constEnd(); ++it)
        m_tabItemCounters[it.key()] = it.value().toInt();
}

void TabWidget::updateTabs()
{
    m_tabs->setCollapsedTabs(m_collapsedTabs);
    m_tabs->updateTabIcons();
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
    const QStringList tabs = this->tabs();

    if (enabled)
        createTabTree();
    else
        createTabBar();

    m_ignoreCurrentTabChanges = true;
    for (int i = 0; i < tabs.size(); ++i) {
        const QString &tabName = tabs[i];
        m_tabs->insertTab(i, tabName);
        m_tabs->setTabItemCount(tabName, itemCountLabel(tabName));
    }
    m_tabs->setCurrentTab( currentIndex() );
    m_ignoreCurrentTabChanges = false;

    m_tabs->setCollapsedTabs(m_collapsedTabs);

    updateToolBar();
}

void TabWidget::setTabItemCount(const QString &tabName, int itemCount)
{
    if ( m_tabItemCounters.value(tabName, -1) == itemCount)
        return;

    m_tabItemCounters[tabName] = itemCount;

    updateTabItemCount(tabName);
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

    const int count = m_tabItemCounters.value(name, -1);
    return count > 0 ? QString::number(count) : count == 0 ? QString() : QString("?");
}
