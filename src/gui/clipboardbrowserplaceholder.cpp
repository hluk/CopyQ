// SPDX-License-Identifier: GPL-3.0-or-later

#include "clipboardbrowserplaceholder.h"

#include "common/common.h"
#include "common/log.h"
#include "common/timer.h"
#include "item/itemstore.h"
#include "gui/clipboardbrowser.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"

#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>

ClipboardBrowserPlaceholder::ClipboardBrowserPlaceholder(
        const QString &tabName, const ClipboardBrowserSharedPtr &shared, QWidget *parent)
    : QWidget(parent)
    , m_tabName(tabName)
    , m_sharedData(shared)
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    initSingleShotTimer( &m_timerExpire, 0, this, &ClipboardBrowserPlaceholder::expire );
}

ClipboardBrowser *ClipboardBrowserPlaceholder::createBrowser()
{
    if (m_browser)
        return m_browser;

    if (m_loadButton)
        return nullptr;

    std::unique_ptr<ClipboardBrowser> c( new ClipboardBrowser(m_tabName, m_sharedData, this) );

    c->setStoreItems(m_storeItems);
    c->setMaxItemCount(m_maxItemCount);

    if ( !c->loadItems() ) {
        createLoadButton();
        return nullptr;
    }

    connect( c.get(), &ClipboardBrowser::itemSelectionChanged,
             this, &ClipboardBrowserPlaceholder::restartExpiring);
    connect( c.get(), &ClipboardBrowser::itemsChanged,
             this, &ClipboardBrowserPlaceholder::restartExpiring);

    m_browser = c.release();

    emit browserCreated(m_browser);
    if (!m_browser)
        return nullptr;

    setActiveWidget(m_browser);

    restartExpiring();

    emit browserLoaded(m_browser);
    return m_browser;
}

bool ClipboardBrowserPlaceholder::setTabName(const QString &tabName)
{
    if ( isEditorOpen() ) {
        if ( !m_browser->setTabName(tabName) )
            return false;
        reloadBrowser();
    } else {
        unloadBrowser();
        if ( !moveItems(m_tabName, tabName) ) {
            if ( isVisible() )
                createBrowser();
            return false;
        }
    }

    ::removeItems(m_tabName);
    m_tabName = tabName;

    if ( isVisible() )
        createBrowser();

    return true;
}

void ClipboardBrowserPlaceholder::setMaxItemCount(int count)
{
    m_maxItemCount = count;
    if (m_browser)
        m_browser->setMaxItemCount(count);
}

void ClipboardBrowserPlaceholder::setStoreItems(bool store)
{
    m_storeItems = store;
    if (m_browser)
        m_browser->setStoreItems(store);
}

void ClipboardBrowserPlaceholder::removeItems()
{
    unloadBrowser();

    ::removeItems(m_tabName);
}

bool ClipboardBrowserPlaceholder::isDataLoaded() const
{
    return m_browser != nullptr;
}

ClipboardBrowser *ClipboardBrowserPlaceholder::createBrowserAgain()
{
    delete m_loadButton;
    m_loadButton = nullptr;
    return createBrowser();
}

void ClipboardBrowserPlaceholder::reloadBrowser()
{
    if ( isEditorOpen() ) {
        connect( m_browser, &ClipboardBrowser::editingFinished,
                 this, &ClipboardBrowserPlaceholder::reloadBrowser, Qt::UniqueConnection );
    } else {
        unloadBrowser();
        if ( isVisible() )
            createBrowser();
    }
}

void ClipboardBrowserPlaceholder::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    QTimer::singleShot(0, this, [this](){
        if ( isVisible() )
            createBrowser();
    });
}

void ClipboardBrowserPlaceholder::hideEvent(QHideEvent *event)
{
    restartExpiring();
    QWidget::hideEvent(event);
}

bool ClipboardBrowserPlaceholder::expire()
{
    if (!m_browser)
        return true;

    if (canExpire()) {
        COPYQ_LOG( QString("Tab \"%1\": Expired").arg(m_tabName) );
        unloadBrowser();
        return true;
    }

    restartExpiring();
    return false;
}

void ClipboardBrowserPlaceholder::setActiveWidget(QWidget *widget)
{
    layout()->addWidget(widget);
    setFocusProxy(widget);
    widget->show();
    if (isVisible())
        widget->setFocus();
}

void ClipboardBrowserPlaceholder::createLoadButton()
{
    if (m_loadButton)
        return;

    m_loadButton = new QPushButton(this);
    m_loadButton->setObjectName("ClipboardBrowserRefreshButton");
    m_loadButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_loadButton->setFlat(true);

    const QIcon icon( getIcon("", IconRotateRight) );
    m_loadButton->setIconSize( QSize(64, 64) );
    m_loadButton->setIcon(icon);

    connect( m_loadButton, &QAbstractButton::clicked,
             this, &ClipboardBrowserPlaceholder::createBrowserAgain );

    setActiveWidget(m_loadButton);
}

void ClipboardBrowserPlaceholder::unloadBrowser()
{
    if (!m_browser)
        return;

    COPYQ_LOG( QString("Tab \"%1\": Unloading").arg(m_tabName) );

    // WORKAROUND: This is needed on macOS, to fix refocusing correct widget later.
    m_browser->clearFocus();

    m_browser->hide();
    m_browser->saveUnsavedItems();
    m_browser->deleteLater();
    m_browser = nullptr;

    emit browserDestroyed();
}

bool ClipboardBrowserPlaceholder::canExpire() const
{
    return m_browser
            && m_storeItems
            && !m_browser->isVisible()
            && !isEditorOpen();
}

void ClipboardBrowserPlaceholder::restartExpiring()
{
    const int expireTimeoutMs = 60000 * m_sharedData->minutesToExpire;
    if (expireTimeoutMs > 0)
        m_timerExpire.start(expireTimeoutMs);
}

bool ClipboardBrowserPlaceholder::isEditorOpen() const
{
    return m_browser && (
                m_browser->isInternalEditorOpen()
                || m_browser->isExternalEditorOpen() );
}
