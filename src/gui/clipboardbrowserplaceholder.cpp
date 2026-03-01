// SPDX-License-Identifier: GPL-3.0-or-later

#include "clipboardbrowserplaceholder.h"

#include "common/common.h"
#include "common/log.h"
#include "common/timer.h"
#include "gui/passwordprompt.h"
#include "item/itemstore.h"
#include "item/serialize.h"
#include "gui/clipboardbrowser.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"

#include <QApplication>
#include <QDataStream>
#include <QLoggingCategory>
#include <QIODevice>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>

namespace {

Q_DECLARE_LOGGING_CATEGORY(logCategory)
Q_LOGGING_CATEGORY(logCategory, "copyq.config")

} // namespace

ClipboardBrowserPlaceholder::ClipboardBrowserPlaceholder(
        const QString &tabName, const ClipboardBrowserSharedPtr &shared, QWidget *parent)
    : QWidget(parent)
    , m_tabName(tabName)
    , m_sharedData(shared)
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    initSingleShotTimer( &m_timerExpire, 0, this, &ClipboardBrowserPlaceholder::expire );
    initSingleShotTimer( &m_timerPasswordExpire, 0, this, &ClipboardBrowserPlaceholder::restartPasswordExpiry);
}

ClipboardBrowser *ClipboardBrowserPlaceholder::createBrowser(AskPassword askPassword)
{
    if (m_browser)
        return m_browser;

    if (m_loadButton)
        return nullptr;

    if (m_sharedData->tabsEncrypted && !m_sharedData->encryptionKey.isValid()) {
        qCDebug(logCategory) << "Locking tab (password not provided):" << m_tabName;
        createLoadButton();
        return nullptr;
    }

    if (askPassword == AskPassword::IfNeeded && shouldPromptForLockedTabPassword()) {
        QPointer<ClipboardBrowserPlaceholder> self(this);
        qCDebug(logCategory) << "Prompting for password for tab:" << m_tabName;
        const auto key = m_sharedData->passwordPrompt->prompt(
            PasswordSource::IgnoreEnvAndKeychain);

        if (!self)
            return nullptr;

        if (m_browser)
            return m_browser;

        if (m_loadButton)
            return nullptr;

        if (!key.isValid()) {
            qCDebug(logCategory) << "Locking tab:" << m_tabName;
            createLoadButton();
            return nullptr;
        }
        qCDebug(logCategory) << "Unlocking tab:" << m_tabName;
    }

    qCDebug(logCategory) << "Loading tab:" << m_tabName;

    std::unique_ptr<ClipboardBrowser> c( new ClipboardBrowser(m_tabName, m_sharedData, this) );

    c->setStoreItems(m_storeItems);
    c->setMaxItemCount(m_maxItemCount);

    if ( !c->loadItems(m_data) ) {
        createLoadButton();
        return nullptr;
    }

    connect( c.get(), &ClipboardBrowser::itemSelectionChanged,
             this, &ClipboardBrowserPlaceholder::restartExpiring);
    connect( c.get(), &ClipboardBrowser::itemsChanged,
             this, &ClipboardBrowserPlaceholder::restartExpiring);
    connect( c.get(), &ClipboardBrowser::filterProgressChanged,
             this, &ClipboardBrowserPlaceholder::onFilterProgressChanged);
    connect( c.get(), &ClipboardBrowser::editingFinished,
             this, &ClipboardBrowserPlaceholder::restartPasswordExpiry );

    m_browser = c.release();

    emit browserCreated(m_browser);
    if (!m_browser)
        return nullptr;

    setActiveWidget(m_browser);

    restartExpiring();
    restartPasswordExpiry();

    emit browserLoaded(m_browser);
    return m_browser;
}

bool ClipboardBrowserPlaceholder::setTabName(const QString &tabName)
{
    if ( isEditorOpen() ) {
        if ( !m_browser->setTabName(tabName) )
            return false;
        reloadBrowser();
    } else if (m_storeItems) {
        unloadBrowser();
        if ( !moveItems(m_tabName, tabName) ) {
            if ( isVisible() )
                createBrowser(AskPassword::Avoid);
            return false;
        }
    }

    ::removeItems(m_tabName);
    m_tabName = tabName;

    if ( isVisible() )
        createBrowser(AskPassword::Avoid);

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

void ClipboardBrowserPlaceholder::setEncryptedExpireSeconds(int seconds)
{
    if (shouldPromptForLockedTabPassword()) {
        m_passwordExpiredAt = std::chrono::steady_clock::now();
    }
    m_encryptedExpireSeconds = seconds;
    restartPasswordExpiry();
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
    if (m_sharedData->tabsEncrypted && m_sharedData->passwordPrompt && !m_sharedData->encryptionKey.isValid()) {
        QPointer<ClipboardBrowserPlaceholder> self(this);
        qCDebug(logCategory) << "Prompting for initial password for tab:" << m_tabName;
        m_sharedData->passwordPrompt->prompt(
            PasswordSource::IgnoreEnvAndKeychain,
            [self](const Encryption::EncryptionKey &key){
                if (self && key.isValid())
                    self->m_sharedData->encryptionKey = key;
            });

        if (!self || !m_sharedData->encryptionKey.isValid())
            return nullptr;
    }

    if (m_loadButton) {
        m_loadButton->hide();
        m_loadButton->deleteLater();
        m_loadButton = nullptr;
    }

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
            createBrowser(AskPassword::Avoid);
    }
}

void ClipboardBrowserPlaceholder::showEvent(QShowEvent *event)
{
    qCDebug(logCategory) << "Showing tab:" << m_tabName;
    QWidget::showEvent(event);
    QTimer::singleShot(0, this, [this](){
        if ( isVisible() )
            createBrowser();
    });
}

void ClipboardBrowserPlaceholder::hideEvent(QHideEvent *event)
{
    qCDebug(logCategory) << "Hiding tab:" << m_tabName;
    restartExpiring();
    restartPasswordExpiry();
    QWidget::hideEvent(event);
}

bool ClipboardBrowserPlaceholder::event(QEvent *event)
{
    if (event->type() == QEvent::WindowActivate && isVisible()) {
        if (!m_browser && !m_loadButton)
            createBrowser();
    } else if (event->type() == QEvent::WindowDeactivate && isVisible()) {
        restartPasswordExpiry();
    }
    return QWidget::event(event);
}

bool ClipboardBrowserPlaceholder::expire()
{
    if (!m_browser)
        return true;

    if (canExpire()) {
        qCDebug(logCategory) << "Expired tab:" << m_tabName;
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

    qCDebug(logCategory) << "Creating reload button for tab:" << m_tabName;

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

    qCDebug(logCategory) << "Unloading tab:" << m_tabName;

    // Keep unsaved items in memory until expiration.
    m_data.clear();
    if ( !m_storeItems && m_browser->isLoaded() ) {
        QDataStream stream(&m_data, QIODevice::WriteOnly);
        if ( !serializeData(*m_browser->model(), &stream, -1, &m_sharedData->encryptionKey) )
            m_data.clear();
    }

    // WORKAROUND: This is needed on macOS, to fix refocusing correct widget later.
    m_browser->clearFocus();

    m_browser->hide();
    m_browser->saveUnsavedItems();
    m_browser->deleteLater();
    m_browser = nullptr;

    if (m_filterProgressBar)
        m_filterProgressBar->hide();
    m_timerPasswordExpire.stop();

    emit browserDestroyed();
}

bool ClipboardBrowserPlaceholder::canExpire() const
{
    return m_browser
            && m_storeItems
            && !isVisible()
            && !isEditorOpen();
}

bool ClipboardBrowserPlaceholder::hasActiveFocus() const
{
    return isVisible() && qApp->applicationState() == Qt::ApplicationActive;
}

void ClipboardBrowserPlaceholder::restartExpiring()
{
    const int expireTimeoutMs = 60000 * m_sharedData->minutesToExpire;
    if (expireTimeoutMs > 0)
        m_timerExpire.start(expireTimeoutMs);
    else
        m_timerExpire.stop();
}

int ClipboardBrowserPlaceholder::encryptedExpireRemainingMs() const
{
    if (!m_sharedData->passwordPrompt)
        return -1;

    const int timeoutSeconds = m_encryptedExpireSeconds;
    if (timeoutSeconds <= 0)
        return -1;

    const auto lastPrompt = m_sharedData->passwordPrompt->lastSuccessfulPasswordPromptTime();
    const auto now = std::chrono::steady_clock::now();
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPrompt).count();
    const qint64 timeoutMs = static_cast<qint64>(timeoutSeconds) * 1000;
    return qMax<qint64>(0, timeoutMs - elapsedMs);
}

bool ClipboardBrowserPlaceholder::shouldPromptForLockedTabPassword() const
{
    if (!m_sharedData->tabsEncrypted || !m_sharedData->passwordPrompt || !m_sharedData->encryptionKey.isValid())
        return false;

    if (m_passwordExpiredAt > m_sharedData->passwordPrompt->lastSuccessfulPasswordPromptTime())
        return true;

    const int remainingMs = encryptedExpireRemainingMs();
    return remainingMs == 0;
}

void ClipboardBrowserPlaceholder::restartPasswordExpiry()
{
    m_timerPasswordExpire.stop();
    if (!m_browser || !m_sharedData->tabsEncrypted)
        return;

    if (isEditorOpen() || hasActiveFocus())
        return;  // will check later on a signal

    const int remainingMs = encryptedExpireRemainingMs();
    if (remainingMs < 0)
        return;

    if (remainingMs == 0) {
        qCDebug(logCategory) << "Expired tab (password required):" << m_tabName;
        unloadBrowser();
        return;
    }

    m_timerPasswordExpire.start(static_cast<int>(remainingMs));
}

bool ClipboardBrowserPlaceholder::isEditorOpen() const
{
    return m_browser && (
                m_browser->isInternalEditorOpen()
                || m_browser->isExternalEditorOpen() );
}

void ClipboardBrowserPlaceholder::onFilterProgressChanged(int percent)
{
    if (percent >= 100) {
        if (m_filterProgressBar)
            m_filterProgressBar->hide();
        return;
    }

    if (m_filterProgressBar == nullptr) {
        m_filterProgressBar = new QProgressBar(this);
        m_filterProgressBar->setObjectName("ClipboardBrowserFilterProgressBar");
        m_filterProgressBar->setContentsMargins({});
        m_filterProgressBar->setTextVisible(false);
        layout()->addWidget(m_filterProgressBar);
    }

    m_filterProgressBar->setValue(percent);
    m_filterProgressBar->show();
}
