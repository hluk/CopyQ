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

#ifndef TRAYMENU_H
#define TRAYMENU_H

#include <QMenu>
#include <QPointer>
#include <QTimer>

class QAction;
class QModelIndex;

class TrayMenu final : public QMenu
{
    Q_OBJECT
public:
    explicit TrayMenu(QWidget *parent = nullptr);

    /**
     * Add clipboard item action with number key hint.
     *
     * Triggering this action emits clipboardItemActionTriggered() signal.
     */
    void addClipboardItemAction(const QVariantMap &data, bool showImages);

    void clearClipboardItems();

    void clearCustomActions();

    /** Add custom action. */
    void addCustomAction(QAction *action);

    /** Clear clipboard item actions and curstom actions. */
    void clearAllActions();

    /** Handle Vi shortcuts. */
    void setViModeEnabled(bool enabled);

    /** Enable searching for numbers. */
    void setNumberSearchEnabled(bool enabled);

    /** Filter clipboard items. */
    void search(const QString &text);

    void markItemInClipboard(const QVariantMap &clipboardData);

signals:
    /** Emitted if numbered action triggered. */
    void clipboardItemActionTriggered(const QVariantMap &itemData, bool omitPaste);

    void searchRequest(const QString &text);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void actionEvent(QActionEvent *event) override;

    void leaveEvent(QEvent *event) override;

    void inputMethodEvent(QInputMethodEvent *event) override;

private:
    void clearActionsWithProperty(const char *property);

    void onClipboardItemActionTriggered();

    void updateActiveAction();

    void setSearchMenuItem(const QString &text);

    QPointer<QAction> m_clipboardItemActionsSeparator;
    QPointer<QAction> m_customActionsSeparator;
    QPointer<QAction> m_searchAction;
    int m_clipboardItemActionCount;

    bool m_omitPaste;
    bool m_viMode;
    bool m_numberSearch;

    QString m_searchText;

    QTimer m_timerUpdateActiveAction;
};

#endif // TRAYMENU_H
