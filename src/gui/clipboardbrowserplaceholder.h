// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include "gui/clipboardbrowsershared.h"

#include <QByteArray>
#include <QString>
#include <QTimer>
#include <QWidget>

class ClipboardBrowser;
class QProgressBar;
class QPushButton;

class ClipboardBrowserPlaceholder final : public QWidget
{
    Q_OBJECT

public:
    enum class AskPassword {
        IfNeeded, Avoid
    };
    ClipboardBrowserPlaceholder(
            const QString &tabName, const ClipboardBrowserSharedPtr &shared, QWidget *parent);

    /// Returns browser (nullptr if not yet created).
    ClipboardBrowser *browser() const { return m_browser; }

    ClipboardBrowser *createBrowser(AskPassword askPassword = AskPassword::IfNeeded);

    bool setTabName(const QString &tabName);
    QString tabName() const { return m_tabName; }

    void setMaxItemCount(int count);
    void setStoreItems(bool store);
    void setEncryptedExpireSeconds(int seconds);

    void removeItems();

    bool isDataLoaded() const;

    /// Create browser if it doesn't exist and even if it previously failed.
    ClipboardBrowser *createBrowserAgain();

    /// Unload and reload (when needed) browser and settings.
    void reloadBrowser();

    /// Unload browser and data.
    bool expire();

    void unloadBrowser();

    void createLoadButton();

signals:
    void browserCreated(ClipboardBrowser *browser);
    void browserLoaded(ClipboardBrowser *browser);
    void browserDestroyed();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool event(QEvent *event) override;

private:
    void setActiveWidget(QWidget *widget);

    bool canExpire() const;
    bool hasActiveFocus() const;
    int encryptedExpireRemainingMs() const;
    bool shouldPromptForLockedTabPassword() const;
    void restartPasswordExpiry();

    void restartExpiring();

    bool isEditorOpen() const;

    void onFilterProgressChanged(int percent);

    ClipboardBrowser *m_browser = nullptr;
    QPushButton *m_loadButton = nullptr;
    QProgressBar *m_filterProgressBar = nullptr;

    QString m_tabName;
    int m_maxItemCount = 200;
    bool m_storeItems = true;
    int m_encryptedExpireSeconds = 0;
    ClipboardBrowserSharedPtr m_sharedData;

    QTimer m_timerExpire;
    QTimer m_timerPasswordExpire;
    QByteArray m_data;
    std::chrono::steady_clock::time_point m_passwordExpiredAt;
};
