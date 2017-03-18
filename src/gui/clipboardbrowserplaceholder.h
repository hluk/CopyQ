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

#ifndef CLIPBOARDBROWSERPLACEHOLDER_H
#define CLIPBOARDBROWSERPLACEHOLDER_H

#include "gui/clipboardbrowsershared.h"

#include <QString>
#include <QTimer>
#include <QWidget>

class ClipboardBrowser;
class MainWindow;
class QPushButton;

class ClipboardBrowserPlaceholder : public QWidget
{
    Q_OBJECT

public:
    ClipboardBrowserPlaceholder(
            const QString &tabName, const ClipboardBrowserSharedPtr &shared, QWidget *parent);

    /// Returns browser (nullptr if not yet created).
    ClipboardBrowser *browser() const { return m_browser; }

    /**
     * Returns browser, creates it first if it doesn't exits (nullptr if it fails to load items).
     *
     * If creating fails it creates reaload button instead and
     * further calls to this function do nothing.
     */
    ClipboardBrowser *createBrowser();

    void setTabName(const QString &tabName);
    QString tabName() const { return m_tabName; }

    void removeItems();

signals:
    void browserCreated(ClipboardBrowser *browser);

public slots:
    /// Create browser if it doesn't exist and even if it previously failed.
    void createBrowserAgain();

    /// Unload and reload (when needed) browser and settings.
    void reloadBrowser();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void expire();

private:
    void setActiveWidget(QWidget *widget);

    void createLoadButton();

    void unloadBrowser();

    bool canExpire() const;

    void restartExpiring();

    ClipboardBrowser *m_browser = nullptr;
    QPushButton *m_loadButton = nullptr;

    QString m_tabName;
    ClipboardBrowserSharedPtr m_sharedData;

    QTimer m_timerExpire;
};

#endif // CLIPBOARDBROWSERPLACEHOLDER_H
